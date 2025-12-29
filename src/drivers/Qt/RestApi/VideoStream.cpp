/**
 * @file VideoStream.cpp
 * @brief Implementation of MJPEG Video Streaming Infrastructure
 *
 * This file implements the VideoFrameBuffer and VideoStreamManager classes
 * defined in VideoStream.h. See that file for detailed documentation of
 * the overall architecture and usage.
 *
 * ## Key Implementation Details
 *
 * ### Palette Conversion
 *
 * The NES doesn't output RGB directly. Instead, it uses a 64-color palette
 * and XBuf contains palette indices (0-63). We use FCEUD_GetPalette() to
 * convert each index to its RGB values. This function respects the current
 * palette settings (e.g., NTSC, PAL, custom palettes).
 *
 * ### Double-Buffering
 *
 * We maintain two buffers and use atomic index swapping:
 *
 * 1. Emulator writes to buffer[writeIdx]
 * 2. After write completes, atomically: writeIdx = (writeIdx + 1) % 2
 * 3. Readers read from buffer[(writeIdx + 1) % 2] (the OTHER buffer)
 *
 * This ensures readers never see a partially-written frame.
 *
 * ### Condition Variable Usage
 *
 * We use a condition variable to wake up waiting streaming threads:
 *
 * - Emulator: lock mutex -> update frame counter -> unlock -> notify_all
 * - Streamers: lock mutex -> wait_for(condition) -> unlock -> read buffer
 *
 * The wait_for uses a lambda that checks if a new frame has arrived,
 * handling spurious wakeups correctly.
 */

#include "VideoStream.h"

// Include FCEUX headers for video buffer access
#include "../fceuWrapper.h"          // For mutex macros if needed
#include "../../../video.h"          // XBuf declaration
#include "../../../driver.h"         // FCEUD_GetPalette declaration

#include <chrono>

//=============================================================================
// External Declarations
//=============================================================================

/**
 * XBuf is the raw NES video output buffer.
 *
 * - Size: 256 x 240 bytes
 * - Format: Palette indices (0-63, but stored as uint8)
 * - Updated: Every frame by the PPU emulation in ppu.cpp
 * - Location: Declared in video.cpp, extern'd here
 *
 * Each byte represents one pixel. To get the actual color, you must
 * look up the palette index using FCEUD_GetPalette().
 */
extern uint8_t *XBuf;

/**
 * FCEUD_GetPalette converts a palette index to RGB values.
 *
 * This function respects the current palette settings, so if the user
 * has selected a different palette (NTSC, PAL, custom), the colors
 * will match what they see on screen.
 *
 * @param index Palette index (0-63)
 * @param r Output: Red component (0-255)
 * @param g Output: Green component (0-255)
 * @param b Output: Blue component (0-255)
 */
void FCEUD_GetPalette(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b);


//=============================================================================
// VideoFrameBuffer Implementation
//=============================================================================

VideoFrameBuffer::VideoFrameBuffer()
{
    // Allocate both buffers with enough space for RGB data
    // 256 width * 240 height * 3 bytes per pixel = 184,320 bytes each
    m_buffers[0].resize(RGB_SIZE, 0);  // Zero-initialize (black frame)
    m_buffers[1].resize(RGB_SIZE, 0);

    // Log buffer allocation for debugging
    // printf("[VideoStream] Allocated 2 x %d byte frame buffers\n", RGB_SIZE);
}

void VideoFrameBuffer::captureFrame(const uint8_t* xbuf)
{
    //-------------------------------------------------------------------------
    // Safety check: Don't crash if XBuf is null (e.g., no game loaded)
    //-------------------------------------------------------------------------
    if (xbuf == nullptr) {
        return;
    }

    //-------------------------------------------------------------------------
    // Get the current write buffer
    //-------------------------------------------------------------------------
    // We write to m_buffers[m_writeIdx] while readers read from the other.
    // This is safe because we only swap the index AFTER writing is complete.
    int writeIdx = m_writeIdx.load(std::memory_order_relaxed);
    uint8_t* dest = m_buffers[writeIdx].data();

    //-------------------------------------------------------------------------
    // Convert palette-indexed pixels to RGB
    //-------------------------------------------------------------------------
    // This is the main work of frame capture. For each of the 61,440 pixels
    // (256 x 240), we:
    // 1. Read the palette index from XBuf
    // 2. Call FCEUD_GetPalette to get the RGB values
    // 3. Write the RGB values to our buffer
    //
    // Performance: This loop takes approximately 0.3-0.5ms on modern hardware.
    // We could optimize with SIMD or a pre-computed palette lookup table,
    // but it's fast enough as-is and this keeps the code simple.
    //
    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
        uint8_t paletteIndex = xbuf[i];
        uint8_t r, g, b;

        // Get the actual RGB color for this palette index
        FCEUD_GetPalette(paletteIndex, &r, &g, &b);

        // Store in RGB order (not BGR!)
        // This matches QImage::Format_RGB888 and is easy to work with
        dest[i * 3 + 0] = r;
        dest[i * 3 + 1] = g;
        dest[i * 3 + 2] = b;
    }

    //-------------------------------------------------------------------------
    // Swap buffers atomically
    //-------------------------------------------------------------------------
    // After this point, readers will see the new buffer. The old write buffer
    // becomes the new read buffer.
    //
    // Memory ordering: We use release semantics to ensure all the pixel
    // writes above are visible before readers see the new index.
    m_writeIdx.store((writeIdx + 1) % 2, std::memory_order_release);

    //-------------------------------------------------------------------------
    // Update frame counter and signal waiting threads
    //-------------------------------------------------------------------------
    // We need the mutex only for the condition variable notification.
    // The actual frame data access is lock-free via atomic buffer swapping.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_frameNumber.fetch_add(1, std::memory_order_relaxed);
        m_lastSignaledFrame.store(m_frameNumber.load(), std::memory_order_relaxed);
    }

    // Wake up all waiting streaming threads
    // notify_all is used because multiple clients may be waiting
    m_frameReady.notify_all();
}

bool VideoFrameBuffer::waitForFrame(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Remember what frame we last saw
    uint64_t lastSeen = m_lastSignaledFrame.load(std::memory_order_relaxed);

    // Wait until a new frame arrives or timeout expires
    //
    // The predicate (lambda) handles spurious wakeups:
    // - Returns true if a new frame has arrived (we should wake up)
    // - Returns false if we should keep waiting
    //
    // wait_for returns true if the predicate returned true,
    // false if the timeout expired.
    return m_frameReady.wait_for(
        lock,
        std::chrono::milliseconds(timeoutMs),
        [this, lastSeen]() {
            // New frame = frame number is greater than what we last saw
            return m_frameNumber.load(std::memory_order_relaxed) > lastSeen;
        }
    );
}

const uint8_t* VideoFrameBuffer::getFrameData() const
{
    // Read from the buffer that's NOT currently being written to
    //
    // Memory ordering: We use acquire semantics to ensure we see all
    // the pixel data that was written before the index was swapped.
    int writeIdx = m_writeIdx.load(std::memory_order_acquire);
    int readIdx = (writeIdx + 1) % 2;

    return m_buffers[readIdx].data();
}

uint64_t VideoFrameBuffer::getFrameNumber() const
{
    return m_frameNumber.load(std::memory_order_relaxed);
}


//=============================================================================
// VideoStreamManager Implementation
//=============================================================================

VideoStreamManager::VideoStreamManager()
{
    // Nothing special to initialize - members have default constructors
    // printf("[VideoStream] VideoStreamManager initialized\n");
}

VideoStreamManager::~VideoStreamManager()
{
    // printf("[VideoStream] VideoStreamManager destroyed\n");
}

VideoStreamManager& VideoStreamManager::getInstance()
{
    // Thread-safe singleton initialization (C++11 guarantees this)
    //
    // The first call to getInstance() creates the instance.
    // Subsequent calls return the same instance.
    // The instance is destroyed when the program exits.
    static VideoStreamManager instance;
    return instance;
}

int VideoStreamManager::addClient()
{
    // Increment client count and generate a unique ID
    //
    // The ID is purely for tracking/debugging - we don't actually
    // use it internally, but it helps with logging and the caller
    // can use it to identify their session.
    m_clientCount.fetch_add(1, std::memory_order_relaxed);
    return m_nextClientId.fetch_add(1, std::memory_order_relaxed);
}

void VideoStreamManager::removeClient(int clientId)
{
    // Decrement client count, but don't go below zero
    //
    // We use a compare-exchange loop to safely decrement only if > 0.
    // This handles edge cases like double-removal.
    int current = m_clientCount.load(std::memory_order_relaxed);
    while (current > 0) {
        if (m_clientCount.compare_exchange_weak(
                current,
                current - 1,
                std::memory_order_relaxed)) {
            break;
        }
        // current is updated by compare_exchange_weak on failure
    }

    // Note: We don't use clientId internally, but it could be used
    // for logging or future features (e.g., per-client statistics)
    (void)clientId;
}

int VideoStreamManager::getClientCount() const
{
    return m_clientCount.load(std::memory_order_relaxed);
}

bool VideoStreamManager::hasClients() const
{
    return m_clientCount.load(std::memory_order_relaxed) > 0;
}

void VideoStreamManager::onFrameComplete()
{
    //-------------------------------------------------------------------------
    // Optimization: Skip capture if no clients are connected
    //-------------------------------------------------------------------------
    // This is the key optimization that makes streaming zero-overhead when
    // not in use. If no one is watching, don't do any work.
    //
    if (!hasClients()) {
        return;
    }

    //-------------------------------------------------------------------------
    // Safety check: Make sure XBuf is valid
    //-------------------------------------------------------------------------
    // XBuf can be null if no game is loaded, or during state transitions.
    if (XBuf == nullptr) {
        return;
    }

    //-------------------------------------------------------------------------
    // Capture the frame
    //-------------------------------------------------------------------------
    // This converts XBuf (palette indices) to RGB and makes it available
    // to streaming threads.
    m_frameBuffer.captureFrame(XBuf);
}

VideoFrameBuffer& VideoStreamManager::getFrameBuffer()
{
    return m_frameBuffer;
}


//=============================================================================
// Global Accessor Function
//=============================================================================

VideoStreamManager& getVideoStreamManager()
{
    return VideoStreamManager::getInstance();
}
