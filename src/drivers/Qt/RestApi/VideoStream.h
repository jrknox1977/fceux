/**
 * @file VideoStream.h
 * @brief MJPEG Video Streaming Infrastructure for Computer Vision Integration
 *
 * This module provides real-time video streaming from FCEUX to external applications
 * like OpenCV. It's designed for machine learning and computer vision use cases
 * where you need to process NES video frames programmatically.
 *
 * ## Architecture Overview
 *
 * The streaming system uses a double-buffering pattern to avoid blocking the
 * emulator thread:
 *
 *   Emulator Thread (60 FPS)              Streaming Thread(s)
 *         |                                      |
 *         v                                      |
 *   [XBuf updated by PPU]                        |
 *         |                                      |
 *         v                                      |
 *   [onFrameComplete()]                          |
 *         |                                      |
 *         v                                      v
 *   [Write to Buffer A] ---- swap ---> [Read from Buffer B]
 *         |                                      |
 *         v                                      v
 *   [Signal waiters]                    [Encode to JPEG]
 *                                                |
 *                                                v
 *                                       [Send over HTTP]
 *
 * ## Usage with OpenCV
 *
 * ```python
 * import cv2
 *
 * # Connect to the MJPEG stream
 * cap = cv2.VideoCapture("http://localhost:8080/api/video/stream?fps=30")
 *
 * while True:
 *     ret, frame = cap.read()
 *     if ret:
 *         # frame is a 240x256x3 BGR numpy array (standard OpenCV format)
 *         # Process with your CV/ML pipeline
 *         gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
 *         cv2.imshow('FCEUX', frame)
 *
 *     if cv2.waitKey(1) & 0xFF == ord('q'):
 *         break
 *
 * cap.release()
 * ```
 *
 * ## Thread Safety
 *
 * - VideoFrameBuffer uses atomic operations for buffer swapping
 * - Condition variable signals new frames to waiting clients
 * - No mutex contention with the emulator thread during normal operation
 *
 * @author Claude Code Assistant
 * @date 2024
 */

#ifndef __VIDEO_STREAM_H__
#define __VIDEO_STREAM_H__

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstdint>

/**
 * @brief Configuration options for video streaming
 *
 * These can be set via query parameters on the /api/video/stream endpoint:
 * - fps: ?fps=30 (1-60)
 * - quality: ?quality=85 (1-100, JPEG quality)
 * - grayscale: ?grayscale=1 (0 or 1)
 *
 * Example: /api/video/stream?fps=15&quality=85&grayscale=0
 */
struct VideoStreamConfig {
    /**
     * Target output frame rate (1-60 FPS)
     *
     * The emulator runs at 60 FPS (NTSC) or 50 FPS (PAL), but you may not
     * need all those frames for CV processing. Lower values reduce bandwidth
     * and CPU usage on both sides.
     *
     * For turn-based games, 10-15 FPS is usually sufficient.
     * For action games or training RL agents, 30-60 FPS may be needed.
     */
    int targetFps = 30;

    /**
     * JPEG compression quality (1-100)
     *
     * Higher values = better quality but larger frames and more CPU usage.
     * - 100: Lossless-ish, ~50-100KB per frame
     * - 90: High quality, ~20-40KB per frame (recommended for CV)
     * - 75: Good quality, ~10-20KB per frame
     * - 50: Acceptable, ~5-10KB per frame
     *
     * For ML/CV, quality 85-95 is usually sufficient since NES graphics
     * are simple pixel art that compresses well.
     */
    int jpegQuality = 90;

    /**
     * Output grayscale instead of color
     *
     * NES only has 64 colors, so grayscale may be sufficient for some
     * CV applications. Reduces bandwidth by ~66% (1 channel vs 3).
     *
     * Note: Some game information is conveyed through color (e.g.,
     * Mario's fire flower outfit), so use with caution.
     */
    bool grayscale = false;
};


/**
 * @brief Double-buffered frame storage for thread-safe video streaming
 *
 * This class manages two RGB buffers that alternate between being written
 * (by the emulator thread) and being read (by streaming threads). This
 * eliminates mutex contention and ensures the emulator is never blocked
 * waiting for network I/O.
 *
 * ## Buffer Layout
 *
 * Each buffer stores a 256x240 RGB image (the native NES resolution):
 * - Total size: 256 * 240 * 3 = 184,320 bytes (~180KB)
 * - Pixel format: RGB888 (3 bytes per pixel, no alpha)
 * - Scanline order: Top-to-bottom, left-to-right
 *
 * ## Why 256x240?
 *
 * We capture from XBuf (the raw NES output) rather than the scaled pixbuf
 * because:
 * 1. Consistent dimensions for ML models (no variable scaling)
 * 2. No scaler artifacts (HQ2x, NTSC filter, etc.)
 * 3. Smaller data size (180KB vs 4MB for 1024x1024)
 * 4. True to the original game graphics
 *
 * ## Memory Overhead
 *
 * - 2 buffers * 180KB = 360KB total
 * - Plus ~30-60KB per client for JPEG encoding buffer
 */
class VideoFrameBuffer {
public:
    //=========================================================================
    // Constants
    //=========================================================================

    /** NES native horizontal resolution */
    static constexpr int WIDTH = 256;

    /** NES native vertical resolution */
    static constexpr int HEIGHT = 240;

    /** Size of one RGB buffer in bytes (256 * 240 * 3) */
    static constexpr int RGB_SIZE = WIDTH * HEIGHT * 3;

    //=========================================================================
    // Lifecycle
    //=========================================================================

    /**
     * @brief Initialize the double-buffer system
     *
     * Allocates two RGB buffers of 180KB each. The buffers are zero-initialized
     * so clients connecting before the first frame will see black.
     */
    VideoFrameBuffer();

    /** Destructor - buffers are automatically freed by vector */
    ~VideoFrameBuffer() = default;

    // Disable copy (buffers are expensive and shouldn't be duplicated)
    VideoFrameBuffer(const VideoFrameBuffer&) = delete;
    VideoFrameBuffer& operator=(const VideoFrameBuffer&) = delete;

    //=========================================================================
    // Producer Interface (called by emulator thread)
    //=========================================================================

    /**
     * @brief Capture the current frame from XBuf
     *
     * This method is called by the emulator thread after each frame is
     * rendered. It:
     * 1. Converts the palette-indexed XBuf to RGB using FCEUD_GetPalette()
     * 2. Writes to the current write buffer
     * 3. Atomically swaps the read/write buffer indices
     * 4. Signals any waiting streaming threads
     *
     * ## Performance
     *
     * The palette-to-RGB conversion takes approximately 0.3-0.5ms on modern
     * hardware. This is fast enough to run every frame at 60 FPS without
     * impacting emulation performance.
     *
     * ## Thread Safety
     *
     * This method is designed to be called from the emulator thread only.
     * The buffer swap is atomic, and the condition variable notification
     * does not block.
     *
     * @param xbuf Pointer to the NES video buffer (256x240, palette-indexed)
     *             This is the global XBuf from video.cpp
     */
    void captureFrame(const uint8_t* xbuf);

    //=========================================================================
    // Consumer Interface (called by streaming threads)
    //=========================================================================

    /**
     * @brief Wait for a new frame to become available
     *
     * Blocks the calling thread until a new frame is captured, or until
     * the timeout expires. Use this in your streaming loop:
     *
     * ```cpp
     * while (client_connected) {
     *     if (buffer.waitForFrame(100)) {
     *         const uint8_t* rgb = buffer.getFrameData();
     *         // Encode and send frame...
     *     }
     *     // Timeout: check if we should exit, then retry
     * }
     * ```
     *
     * @param timeoutMs Maximum time to wait in milliseconds
     * @return true if a new frame is available, false if timed out
     */
    bool waitForFrame(int timeoutMs);

    /**
     * @brief Get pointer to the current readable frame data
     *
     * Returns a pointer to the RGB buffer that is NOT currently being
     * written to. The data remains valid until the next call to
     * waitForFrame() returns true.
     *
     * ## Pixel Format
     *
     * - Format: RGB888 (3 bytes per pixel)
     * - Layout: [R0, G0, B0, R1, G1, B1, ...]
     * - Stride: 256 * 3 = 768 bytes per row
     * - Total: 184,320 bytes
     *
     * ## Example: Accessing a specific pixel
     *
     * ```cpp
     * const uint8_t* rgb = buffer.getFrameData();
     * int x = 128, y = 120;  // Center of screen
     * int offset = (y * WIDTH + x) * 3;
     * uint8_t r = rgb[offset + 0];
     * uint8_t g = rgb[offset + 1];
     * uint8_t b = rgb[offset + 2];
     * ```
     *
     * @return Pointer to RGB pixel data (256x240x3 bytes)
     */
    const uint8_t* getFrameData() const;

    /**
     * @brief Get the current frame number
     *
     * Monotonically increasing counter that increments with each captured
     * frame. Useful for:
     * - Detecting dropped frames
     * - Synchronizing with other data sources
     * - Debugging timing issues
     *
     * @return Frame number (starts at 0, wraps at UINT64_MAX)
     */
    uint64_t getFrameNumber() const;

private:
    //=========================================================================
    // Buffer Storage
    //=========================================================================

    /**
     * Double buffer for RGB frame data
     *
     * m_buffers[0] and m_buffers[1] alternate between read and write roles.
     * When m_writeIdx is 0, we write to buffer 0 and read from buffer 1.
     * After capture, we swap: write to buffer 1, read from buffer 0.
     */
    std::vector<uint8_t> m_buffers[2];

    /**
     * Index of the buffer currently being written to (0 or 1)
     *
     * Atomic to allow lock-free swapping. The read buffer is always
     * (m_writeIdx + 1) % 2.
     */
    std::atomic<int> m_writeIdx{0};

    //=========================================================================
    // Synchronization
    //=========================================================================

    /**
     * Frame counter - increments with each captured frame
     *
     * Used by waitForFrame() to detect new frames. Atomic because it's
     * read by multiple streaming threads.
     */
    std::atomic<uint64_t> m_frameNumber{0};

    /**
     * Mutex protecting the condition variable
     *
     * Only held briefly during wait/notify operations, never during
     * the actual frame capture or encoding.
     */
    mutable std::mutex m_mutex;

    /**
     * Condition variable for frame-ready signaling
     *
     * Streaming threads wait on this; the emulator thread signals it
     * after each frame capture.
     */
    std::condition_variable m_frameReady;

    /**
     * Frame number of the last signaled frame
     *
     * Used by waitForFrame() to determine if a new frame has arrived
     * since the last wait.
     */
    std::atomic<uint64_t> m_lastSignaledFrame{0};
};


/**
 * @brief Singleton manager for video streaming sessions
 *
 * This class coordinates between the emulator and streaming clients:
 * - Tracks connected clients (for optimization - skip capture if no clients)
 * - Owns the shared VideoFrameBuffer
 * - Provides the hook point for the emulator to signal frame completion
 *
 * ## Singleton Pattern
 *
 * Use getInstance() or the global getVideoStreamManager() function:
 *
 * ```cpp
 * // In emulator loop (fceuWrapper.cpp)
 * getVideoStreamManager().onFrameComplete();
 *
 * // In streaming handler (VideoStreamCommands.cpp)
 * VideoStreamManager& mgr = getVideoStreamManager();
 * int clientId = mgr.addClient();
 * // ... streaming loop ...
 * mgr.removeClient(clientId);
 * ```
 *
 * ## Performance Optimization
 *
 * Frame capture is skipped when no clients are connected. This means
 * there's zero overhead when streaming is not in use.
 */
class VideoStreamManager {
public:
    //=========================================================================
    // Singleton Access
    //=========================================================================

    /**
     * @brief Get the singleton instance
     *
     * Thread-safe initialization via C++11 static local variable.
     * The instance is created on first call and destroyed at program exit.
     *
     * @return Reference to the singleton VideoStreamManager
     */
    static VideoStreamManager& getInstance();

    // Disable copy and move (it's a singleton)
    VideoStreamManager(const VideoStreamManager&) = delete;
    VideoStreamManager& operator=(const VideoStreamManager&) = delete;
    VideoStreamManager(VideoStreamManager&&) = delete;
    VideoStreamManager& operator=(VideoStreamManager&&) = delete;

    //=========================================================================
    // Client Management
    //=========================================================================

    /**
     * @brief Register a new streaming client
     *
     * Call this when a client connects to /api/video/stream. The returned
     * ID should be passed to removeClient() when the stream ends.
     *
     * This increments the internal client counter, which enables frame
     * capture in onFrameComplete().
     *
     * @return Unique client ID for this session
     */
    int addClient();

    /**
     * @brief Unregister a streaming client
     *
     * Call this when a client disconnects (either gracefully or due to
     * error). Decrements the client counter.
     *
     * When the last client disconnects, frame capture is disabled to
     * save CPU cycles.
     *
     * @param clientId The ID returned by addClient()
     */
    void removeClient(int clientId);

    /**
     * @brief Get the number of active streaming clients
     *
     * Useful for the /api/video/info endpoint and debugging.
     *
     * @return Number of connected clients (0 = no streaming active)
     */
    int getClientCount() const;

    /**
     * @brief Check if any clients are connected
     *
     * Convenience method for the frame capture optimization.
     *
     * @return true if at least one client is streaming
     */
    bool hasClients() const;

    //=========================================================================
    // Emulator Integration
    //=========================================================================

    /**
     * @brief Called by emulator thread after each frame
     *
     * This is the main integration point. Call this from fceuWrapper.cpp
     * after DoFun() completes (around line 1644):
     *
     * ```cpp
     * // In fceuWrapperUpdate(), after DoFun():
     * #ifdef __FCEU_REST_API_ENABLE__
     *     getVideoStreamManager().onFrameComplete();
     * #endif
     * ```
     *
     * If no clients are connected, this function returns immediately
     * without doing any work (zero overhead when not streaming).
     *
     * If clients ARE connected, it captures XBuf and signals waiters.
     */
    void onFrameComplete();

    //=========================================================================
    // Buffer Access
    //=========================================================================

    /**
     * @brief Get the shared frame buffer
     *
     * Streaming threads use this to wait for and read frames:
     *
     * ```cpp
     * VideoFrameBuffer& buf = getVideoStreamManager().getFrameBuffer();
     * while (streaming) {
     *     if (buf.waitForFrame(100)) {
     *         const uint8_t* rgb = buf.getFrameData();
     *         // ... encode and send ...
     *     }
     * }
     * ```
     *
     * @return Reference to the VideoFrameBuffer
     */
    VideoFrameBuffer& getFrameBuffer();

private:
    //=========================================================================
    // Private Constructor (singleton pattern)
    //=========================================================================

    VideoStreamManager();
    ~VideoStreamManager();

    //=========================================================================
    // Member Variables
    //=========================================================================

    /** The shared double-buffer for frame data */
    VideoFrameBuffer m_frameBuffer;

    /** Number of currently connected streaming clients */
    std::atomic<int> m_clientCount{0};

    /** Counter for generating unique client IDs */
    std::atomic<int> m_nextClientId{1};
};


//=============================================================================
// Global Accessor Function
//=============================================================================

/**
 * @brief Get the global VideoStreamManager instance
 *
 * Convenience function that wraps VideoStreamManager::getInstance().
 * Use this in most code for brevity.
 *
 * @return Reference to the singleton VideoStreamManager
 */
VideoStreamManager& getVideoStreamManager();

#endif // __VIDEO_STREAM_H__
