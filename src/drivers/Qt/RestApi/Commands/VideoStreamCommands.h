/**
 * @file VideoStreamCommands.h
 * @brief MJPEG Streaming HTTP Endpoint Handlers
 *
 * This file declares the HTTP handlers for video streaming endpoints.
 * The main endpoint is GET /api/video/stream which provides an MJPEG
 * stream compatible with OpenCV's cv2.VideoCapture().
 *
 * ## Endpoints
 *
 * ### GET /api/video/stream
 *
 * Returns a multipart MJPEG stream of the emulator display.
 *
 * Query Parameters:
 * - fps (int, 1-60, default 30): Target frame rate
 * - quality (int, 1-100, default 90): JPEG compression quality
 * - grayscale (0|1, default 0): Output grayscale instead of color
 *
 * Response:
 * - Content-Type: multipart/x-mixed-replace; boundary=fceux-frame-boundary
 * - Each part is a JPEG image with Content-Type: image/jpeg
 *
 * Example:
 * ```
 * GET /api/video/stream?fps=30&quality=85
 * ```
 *
 * ### GET /api/video/info
 *
 * Returns JSON information about the video stream.
 *
 * Response:
 * ```json
 * {
 *     "available": true,
 *     "width": 256,
 *     "height": 240,
 *     "active_clients": 1,
 *     "total_frames": 12345
 * }
 * ```
 *
 * ## MJPEG Protocol
 *
 * MJPEG (Motion JPEG) over HTTP is a simple streaming format:
 *
 * 1. Server sends HTTP response with Content-Type: multipart/x-mixed-replace
 * 2. Each frame is a separate MIME part, delimited by a boundary string
 * 3. Each part has its own Content-Type (image/jpeg) and Content-Length
 * 4. Stream continues until client disconnects
 *
 * Format:
 * ```
 * HTTP/1.1 200 OK
 * Content-Type: multipart/x-mixed-replace; boundary=fceux-frame-boundary
 *
 * --fceux-frame-boundary
 * Content-Type: image/jpeg
 * Content-Length: 12345
 *
 * <JPEG binary data>
 * --fceux-frame-boundary
 * Content-Type: image/jpeg
 * Content-Length: 12346
 *
 * <JPEG binary data>
 * ...
 * ```
 *
 * ## OpenCV Compatibility
 *
 * OpenCV's VideoCapture natively supports MJPEG streams:
 *
 * ```python
 * import cv2
 *
 * cap = cv2.VideoCapture("http://localhost:8080/api/video/stream")
 *
 * while True:
 *     ret, frame = cap.read()
 *     if ret:
 *         # frame is a numpy array with shape (240, 256, 3)
 *         # OpenCV uses BGR order, so colors are correct automatically
 *         cv2.imshow('FCEUX', frame)
 *
 *     if cv2.waitKey(1) & 0xFF == ord('q'):
 *         break
 *
 * cap.release()
 * ```
 */

#ifndef __VIDEO_STREAM_COMMANDS_H__
#define __VIDEO_STREAM_COMMANDS_H__

#include "../../../../lib/httplib.h"
#include "../VideoStream.h"

#include <vector>
#include <string>
#include <cstdint>


//=============================================================================
// MJPEG Stream Handler
//=============================================================================

/**
 * @brief HTTP handler for MJPEG video streaming
 *
 * This class provides static methods to handle the /api/video/stream endpoint.
 * It uses cpp-httplib's content provider feature for efficient streaming.
 *
 * ## Implementation Notes
 *
 * The streaming loop runs inside httplib's content provider callback:
 *
 * 1. Parse configuration from query parameters (fps, quality, grayscale)
 * 2. Register as a streaming client with VideoStreamManager
 * 3. Set up multipart/x-mixed-replace response
 * 4. Enter streaming loop:
 *    a. Wait for new frame from VideoFrameBuffer
 *    b. Apply rate limiting based on target FPS
 *    c. Encode frame to JPEG using Qt's QImage
 *    d. Write MJPEG part (boundary + headers + data)
 *    e. Check if client is still connected
 * 5. On disconnect, unregister from VideoStreamManager
 *
 * ## Thread Safety
 *
 * Each streaming client runs in its own thread (httplib is multi-threaded).
 * The VideoFrameBuffer handles synchronization using atomics and
 * condition variables.
 */
class MjpegStreamHandler {
public:
    //=========================================================================
    // HTTP Handler
    //=========================================================================

    /**
     * @brief Handle GET /api/video/stream request
     *
     * This is the main entry point called by the REST API router.
     * It sets up an MJPEG stream and loops until the client disconnects.
     *
     * Query Parameters:
     * - fps: Target frame rate, 1-60 (default: 30)
     * - quality: JPEG quality, 1-100 (default: 90)
     * - grayscale: 0 or 1 (default: 0)
     *
     * @param req The HTTP request (contains query parameters)
     * @param res The HTTP response (will be set to streaming mode)
     */
    static void handle(const httplib::Request& req, httplib::Response& res);

private:
    //=========================================================================
    // Helper Methods
    //=========================================================================

    /**
     * @brief Parse stream configuration from query parameters
     *
     * Extracts and validates fps, quality, and grayscale parameters.
     * Invalid values are clamped to valid ranges.
     *
     * @param req The HTTP request containing query parameters
     * @return Validated VideoStreamConfig
     */
    static VideoStreamConfig parseConfig(const httplib::Request& req);

    /**
     * @brief Encode RGB frame to JPEG
     *
     * Uses Qt's QImage for JPEG encoding. This is convenient because
     * Qt is already a dependency and provides good compression quality.
     *
     * ## Performance
     *
     * JPEG encoding typically takes 2-5ms at quality 90 for 256x240.
     * This is the main bottleneck in the streaming pipeline, but it's
     * still fast enough for 60 FPS streaming.
     *
     * ## Grayscale Mode
     *
     * When grayscale=true, we convert RGB to grayscale using the
     * ITU-R BT.601 formula (same as OpenCV):
     *   Y = 0.299*R + 0.587*G + 0.114*B
     *
     * This reduces data size by ~66% (1 channel vs 3).
     *
     * @param rgb Pointer to RGB888 pixel data (256*240*3 bytes)
     * @param width Frame width (256)
     * @param height Frame height (240)
     * @param quality JPEG quality (1-100)
     * @param grayscale If true, convert to grayscale before encoding
     * @param output Vector to receive JPEG data (will be resized)
     * @return true on success, false on encoding failure
     */
    static bool encodeJpeg(
        const uint8_t* rgb,
        int width,
        int height,
        int quality,
        bool grayscale,
        std::vector<uint8_t>& output
    );
};


//=============================================================================
// Video Info Response
//=============================================================================

/**
 * @brief Response data for GET /api/video/info
 *
 * This struct holds information about the video stream that can be
 * returned to clients as JSON.
 */
struct VideoStreamInfoResult {
    /** Whether video streaming is available (game loaded) */
    bool available = false;

    /** Frame width in pixels (always 256 for NES) */
    int width = 256;

    /** Frame height in pixels (always 240 for NES) */
    int height = 240;

    /** Number of currently connected streaming clients */
    int activeClients = 0;

    /** Total frames captured since emulator started */
    uint64_t totalFrames = 0;

    /**
     * @brief Serialize to JSON string
     *
     * @return JSON representation of this struct
     */
    std::string toJson() const;
};


#endif // __VIDEO_STREAM_COMMANDS_H__
