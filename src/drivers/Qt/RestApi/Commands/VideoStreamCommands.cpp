/**
 * @file VideoStreamCommands.cpp
 * @brief Implementation of MJPEG Streaming HTTP Handlers
 *
 * This file implements the MJPEG streaming endpoint for computer vision
 * integration. The key challenge is efficiently bridging the emulator's
 * frame-based rendering with HTTP streaming.
 *
 * ## Architecture
 *
 * ```
 *                      ┌─────────────────────────────────────────────┐
 *                      │           Emulator Thread (60 FPS)          │
 *                      │  ┌──────────┐    ┌──────────────────────┐   │
 *                      │  │   PPU    │───>│ VideoStreamManager:: │   │
 *                      │  │ (XBuf)   │    │ onFrameComplete()    │   │
 *                      │  └──────────┘    └──────────────────────┘   │
 *                      │                            │                │
 *                      └────────────────────────────│────────────────┘
 *                                                   │
 *                                                   ▼
 *                      ┌─────────────────────────────────────────────┐
 *                      │            VideoFrameBuffer                 │
 *                      │  ┌──────────┐    ┌──────────────────────┐   │
 *                      │  │ Buffer A │◄──►│       Buffer B       │   │
 *                      │  │  (RGB)   │    │        (RGB)         │   │
 *                      │  └──────────┘    └──────────────────────┘   │
 *                      │         │                  ▲                │
 *                      │         │ signal           │ wait           │
 *                      │         ▼                  │                │
 *                      └─────────│──────────────────│────────────────┘
 *                                │                  │
 *            ┌───────────────────┼──────────────────┼───────────────────┐
 *            │                   ▼                  │                   │
 *            │  ┌─────────────────────────────────────────────────────┐ │
 *            │  │              Streaming Thread (per client)         │ │
 *            │  │  ┌──────────┐  ┌──────────┐  ┌──────────────────┐  │ │
 *            │  │  │   Wait   │─>│  Encode  │─>│   HTTP Write     │  │ │
 *            │  │  │ forFrame │  │   JPEG   │  │ (MJPEG part)     │  │ │
 *            │  │  └──────────┘  └──────────┘  └──────────────────┘  │ │
 *            │  └─────────────────────────────────────────────────────┘ │
 *            │                   REST API / httplib Thread Pool         │
 *            └──────────────────────────────────────────────────────────┘
 * ```
 *
 * ## MJPEG Format Details
 *
 * MJPEG (Motion JPEG) is a simple streaming format where each frame is
 * sent as a separate JPEG image. The frames are delimited using MIME
 * multipart boundaries:
 *
 * ```
 * --boundary\r\n
 * Content-Type: image/jpeg\r\n
 * Content-Length: <size>\r\n
 * \r\n
 * <JPEG data>
 * \r\n
 * --boundary\r\n
 * ...
 * ```
 *
 * This format is natively supported by:
 * - OpenCV's cv2.VideoCapture
 * - Most web browsers (via <img> tag)
 * - VLC, ffplay, and other media players
 * - Python requests library (with streaming)
 */

#include "VideoStreamCommands.h"
#include "../VideoStream.h"
#include "../../../../lib/json.hpp"  // For JSON serialization
#include "../../../../fceu.h"         // For GameInfo

// Qt includes for JPEG encoding
#include <QImage>
#include <QBuffer>
#include <QByteArray>

// Standard library
#include <chrono>
#include <thread>
#include <sstream>

// JSON alias
using json = nlohmann::json;

//=============================================================================
// Constants
//=============================================================================

/**
 * MJPEG boundary marker
 *
 * This string separates individual JPEG frames in the multipart stream.
 * It should be unique enough that it won't appear in JPEG data.
 * The "fceux-" prefix makes it identifiable in debugging.
 */
static const char* MJPEG_BOUNDARY = "fceux-frame-boundary";

/**
 * Frame wait timeout in milliseconds
 *
 * How long to wait for a new frame before checking if the client
 * is still connected. This allows graceful handling of:
 * - Paused emulation
 * - Slow frame rate
 * - Client disconnection detection
 */
static const int FRAME_WAIT_TIMEOUT_MS = 100;


//=============================================================================
// MjpegStreamHandler Implementation
//=============================================================================

void MjpegStreamHandler::handle(const httplib::Request& req, httplib::Response& res)
{
    //-------------------------------------------------------------------------
    // Parse configuration from query parameters
    //-------------------------------------------------------------------------
    VideoStreamConfig config = parseConfig(req);

    //-------------------------------------------------------------------------
    // Check if a game is loaded
    //-------------------------------------------------------------------------
    // Without a game, there's no video to stream. GameInfo is a global
    // pointer that's non-null when a ROM is loaded.
    //
    extern FCEUGI* GameInfo;
    if (GameInfo == nullptr) {
        res.status = 503;  // Service Unavailable
        json error;
        error["error"] = "No game loaded";
        error["message"] = "Load a ROM before starting video stream";
        res.set_content(error.dump(), "application/json");
        return;
    }

    //-------------------------------------------------------------------------
    // Register this client with the stream manager
    //-------------------------------------------------------------------------
    // This enables frame capture (if this is the first client) and
    // gives us an ID for tracking.
    //
    VideoStreamManager& manager = getVideoStreamManager();
    int clientId = manager.addClient();

    //-------------------------------------------------------------------------
    // Calculate frame timing
    //-------------------------------------------------------------------------
    // We use std::chrono for precise timing. The interval is how long
    // we want between frames to achieve the target FPS.
    //
    auto frameInterval = std::chrono::milliseconds(1000 / config.targetFps);

    //-------------------------------------------------------------------------
    // Set up MJPEG multipart response
    //-------------------------------------------------------------------------
    // The Content-Type header tells clients this is an MJPEG stream.
    // The boundary parameter specifies where frames are separated.
    //
    std::string contentType = "multipart/x-mixed-replace; boundary=";
    contentType += MJPEG_BOUNDARY;

    //-------------------------------------------------------------------------
    // Set up streaming response using httplib's content provider
    //-------------------------------------------------------------------------
    // httplib's set_content_provider allows us to stream data without
    // knowing the total content length in advance. It calls our lambda
    // repeatedly until we return false.
    //
    // The lambda captures:
    // - clientId: For cleanup when stream ends
    // - config: Stream configuration (fps, quality, grayscale)
    // - frameInterval: Target time between frames
    // - &manager: Reference to the stream manager
    //
    res.set_content_provider(
        contentType,  // Content-Type header

        // Content provider lambda - called repeatedly to generate content
        [clientId, config, frameInterval, &manager](
            size_t offset,           // Byte offset (unused for streaming)
            httplib::DataSink& sink  // Where to write data
        ) -> bool {

            //------------------------------------------------------------------
            // Check if client is still connected
            //------------------------------------------------------------------
            // is_writable() returns false if:
            // - Client closed the connection
            // - Network error occurred
            // - Server is shutting down
            //
            if (!sink.is_writable()) {
                manager.removeClient(clientId);
                return false;  // End the stream
            }

            //------------------------------------------------------------------
            // Get the frame buffer
            //------------------------------------------------------------------
            VideoFrameBuffer& frameBuffer = manager.getFrameBuffer();

            //------------------------------------------------------------------
            // Track timing for rate limiting
            //------------------------------------------------------------------
            auto lastFrameTime = std::chrono::steady_clock::now();

            // Reusable buffer for JPEG data (avoids repeated allocations)
            std::vector<uint8_t> jpegData;
            jpegData.reserve(64 * 1024);  // Pre-allocate 64KB

            //------------------------------------------------------------------
            // Main streaming loop
            //------------------------------------------------------------------
            // This loop continues until the client disconnects or an error
            // occurs. Each iteration:
            // 1. Waits for a new frame
            // 2. Applies rate limiting
            // 3. Encodes to JPEG
            // 4. Writes the MJPEG part
            //
            while (sink.is_writable()) {

                //--------------------------------------------------------------
                // Wait for the next frame
                //--------------------------------------------------------------
                // This blocks until either:
                // - A new frame is available (returns true)
                // - The timeout expires (returns false)
                //
                // On timeout, we loop back to check is_writable(), which
                // allows us to detect client disconnection during paused
                // emulation.
                //
                if (!frameBuffer.waitForFrame(FRAME_WAIT_TIMEOUT_MS)) {
                    // Timeout - no new frame yet
                    // Check if client is still connected and continue waiting
                    continue;
                }

                //--------------------------------------------------------------
                // Apply rate limiting
                //--------------------------------------------------------------
                // If we're running faster than the target FPS, sleep to
                // avoid wasting bandwidth and CPU on frames the client
                // can't process anyway.
                //
                auto now = std::chrono::steady_clock::now();
                auto elapsed = now - lastFrameTime;

                if (elapsed < frameInterval) {
                    // Sleep for the remaining time
                    std::this_thread::sleep_for(frameInterval - elapsed);
                }

                lastFrameTime = std::chrono::steady_clock::now();

                //--------------------------------------------------------------
                // Get the frame data
                //--------------------------------------------------------------
                // This returns a pointer to 256x240x3 bytes of RGB data.
                // The pointer is valid until the next waitForFrame() call.
                //
                const uint8_t* rgb = frameBuffer.getFrameData();

                //--------------------------------------------------------------
                // Encode to JPEG
                //--------------------------------------------------------------
                // This is typically the most expensive operation in the loop,
                // taking 2-5ms depending on quality settings.
                //
                jpegData.clear();
                if (!encodeJpeg(
                        rgb,
                        VideoFrameBuffer::WIDTH,
                        VideoFrameBuffer::HEIGHT,
                        config.jpegQuality,
                        config.grayscale,
                        jpegData)) {
                    // Encoding failed - skip this frame and try the next one
                    // This shouldn't happen in normal operation
                    continue;
                }

                //--------------------------------------------------------------
                // Build the MJPEG part header
                //--------------------------------------------------------------
                // Format:
                // --boundary\r\n
                // Content-Type: image/jpeg\r\n
                // Content-Length: <size>\r\n
                // \r\n
                //
                std::ostringstream header;
                header << "--" << MJPEG_BOUNDARY << "\r\n";
                header << "Content-Type: image/jpeg\r\n";
                header << "Content-Length: " << jpegData.size() << "\r\n";
                header << "\r\n";

                std::string headerStr = header.str();

                //--------------------------------------------------------------
                // Write the header
                //--------------------------------------------------------------
                if (!sink.write(headerStr.c_str(), headerStr.size())) {
                    // Write failed - client probably disconnected
                    break;
                }

                //--------------------------------------------------------------
                // Write the JPEG data
                //--------------------------------------------------------------
                if (!sink.write(
                        reinterpret_cast<const char*>(jpegData.data()),
                        jpegData.size())) {
                    // Write failed - client probably disconnected
                    break;
                }

                //--------------------------------------------------------------
                // Write the part terminator
                //--------------------------------------------------------------
                // Each MJPEG part ends with \r\n before the next boundary
                //
                if (!sink.write("\r\n", 2)) {
                    // Write failed - client probably disconnected
                    break;
                }

            }  // End of streaming loop

            //------------------------------------------------------------------
            // Clean up on stream end
            //------------------------------------------------------------------
            manager.removeClient(clientId);
            return false;  // Signal that we're done streaming
        },

        // Resource releaser - called when the response is complete
        [clientId, &manager](bool success) {
            // Ensure client is removed even if something went wrong
            // This is a safety net - the main loop should already have
            // called removeClient()
            manager.removeClient(clientId);

            // 'success' indicates whether the stream ended normally (true)
            // or due to an error (false). We could log this for debugging.
            (void)success;
        }
    );
}

VideoStreamConfig MjpegStreamHandler::parseConfig(const httplib::Request& req)
{
    VideoStreamConfig config;

    //-------------------------------------------------------------------------
    // Parse 'fps' parameter (target frame rate)
    //-------------------------------------------------------------------------
    if (req.has_param("fps")) {
        try {
            int fps = std::stoi(req.get_param_value("fps"));
            // Clamp to valid range [1, 60]
            config.targetFps = std::max(1, std::min(60, fps));
        } catch (...) {
            // Invalid value - use default
        }
    }

    //-------------------------------------------------------------------------
    // Parse 'quality' parameter (JPEG quality)
    //-------------------------------------------------------------------------
    if (req.has_param("quality")) {
        try {
            int quality = std::stoi(req.get_param_value("quality"));
            // Clamp to valid range [1, 100]
            config.jpegQuality = std::max(1, std::min(100, quality));
        } catch (...) {
            // Invalid value - use default
        }
    }

    //-------------------------------------------------------------------------
    // Parse 'grayscale' parameter
    //-------------------------------------------------------------------------
    if (req.has_param("grayscale")) {
        std::string value = req.get_param_value("grayscale");
        config.grayscale = (value == "1" || value == "true");
    }

    return config;
}

bool MjpegStreamHandler::encodeJpeg(
    const uint8_t* rgb,
    int width,
    int height,
    int quality,
    bool grayscale,
    std::vector<uint8_t>& output)
{
    //-------------------------------------------------------------------------
    // Create QImage from pixel data
    //-------------------------------------------------------------------------
    // We need to handle two cases:
    // 1. Color mode: Use the RGB data directly
    // 2. Grayscale mode: Convert RGB to grayscale first
    //
    QImage image;

    if (grayscale) {
        //---------------------------------------------------------------------
        // Grayscale conversion
        //---------------------------------------------------------------------
        // We manually convert RGB to grayscale using the standard formula:
        //   Y = 0.299*R + 0.587*G + 0.114*B
        //
        // This is the ITU-R BT.601 luma formula, same as OpenCV uses.
        //
        image = QImage(width, height, QImage::Format_Grayscale8);

        for (int y = 0; y < height; ++y) {
            // Get pointer to the output scanline
            uint8_t* line = image.scanLine(y);

            for (int x = 0; x < width; ++x) {
                // Calculate input pixel offset
                int idx = (y * width + x) * 3;

                // Get RGB values
                uint8_t r = rgb[idx + 0];
                uint8_t g = rgb[idx + 1];
                uint8_t b = rgb[idx + 2];

                // Convert to grayscale using BT.601 formula
                // Using fixed-point arithmetic for speed:
                // Y = (77*R + 150*G + 29*B) / 256
                // This approximates: 0.299*R + 0.587*G + 0.114*B
                uint8_t gray = static_cast<uint8_t>(
                    (77 * r + 150 * g + 29 * b) >> 8
                );

                line[x] = gray;
            }
        }
    } else {
        //---------------------------------------------------------------------
        // Color mode - use RGB data directly
        //---------------------------------------------------------------------
        // QImage::Format_RGB888 matches our RGB byte order.
        // We provide the stride (bytes per row) to handle any padding.
        //
        // Note: QImage doesn't copy the data - it just references it.
        // This is fine because we encode immediately below.
        //
        image = QImage(
            rgb,                    // Pixel data pointer
            width,                  // Width in pixels
            height,                 // Height in pixels
            width * 3,              // Bytes per scanline (stride)
            QImage::Format_RGB888   // Pixel format
        );
    }

    //-------------------------------------------------------------------------
    // Encode to JPEG
    //-------------------------------------------------------------------------
    // We use QBuffer to capture the JPEG data in memory.
    //
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);

    if (!image.save(&buffer, "JPEG", quality)) {
        // Encoding failed - this shouldn't happen in normal operation
        return false;
    }

    //-------------------------------------------------------------------------
    // Copy to output vector
    //-------------------------------------------------------------------------
    // We copy from QByteArray to std::vector because the caller expects
    // a vector and we don't want to expose Qt types in the public API.
    //
    output.assign(ba.begin(), ba.end());

    return true;
}


//=============================================================================
// VideoStreamInfoResult Implementation
//=============================================================================

std::string VideoStreamInfoResult::toJson() const
{
    json j;

    j["available"] = available;
    j["width"] = width;
    j["height"] = height;
    j["active_clients"] = activeClients;
    j["total_frames"] = totalFrames;

    return j.dump();
}
