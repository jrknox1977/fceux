// REST API Video Streaming Design
// This demonstrates WebSocket-based video streaming for the REST API

#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

// Simulated WebSocket connection
class WebSocketConnection {
public:
    bool isConnected = true;
    
    void send(const std::vector<uint8_t>& data) {
        std::cout << "Sending frame: " << data.size() << " bytes\n";
    }
};

// Video streaming manager for REST API
class VideoStreamManager {
private:
    struct Frame {
        std::vector<uint8_t> data;
        uint32_t frameNumber;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    std::queue<Frame> frameQueue;
    std::mutex queueMutex;
    std::condition_variable frameAvailable;
    
    bool streaming = false;
    int targetFPS = 30;
    int compressionQuality = 85;
    
    std::vector<WebSocketConnection*> activeConnections;
    std::mutex connectionsMutex;
    
public:
    // Called from emulator thread after each frame
    void captureFrame(uint8_t* XBuf, int width, int height) {
        if (!streaming || activeConnections.empty()) {
            return;
        }
        
        auto now = std::chrono::steady_clock::now();
        static auto lastCapture = now;
        
        // Limit capture rate to target FPS
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCapture);
        if (elapsed.count() < (1000 / targetFPS)) {
            return;
        }
        lastCapture = now;
        
        // Compress frame (simplified - would use real compression)
        Frame frame;
        frame.timestamp = now;
        frame.frameNumber = getFrameNumber();
        
        // Convert to RGB and compress
        frame.data = compressFrame(XBuf, width, height);
        
        // Add to queue
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            
            // Limit queue size to prevent memory growth
            const size_t MAX_QUEUE_SIZE = 10;
            if (frameQueue.size() >= MAX_QUEUE_SIZE) {
                frameQueue.pop(); // Drop oldest frame
            }
            
            frameQueue.push(std::move(frame));
        }
        
        frameAvailable.notify_all();
    }
    
    // Stream worker thread
    void streamWorker() {
        while (streaming) {
            Frame frame;
            
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                frameAvailable.wait(lock, [this] { 
                    return !frameQueue.empty() || !streaming; 
                });
                
                if (!streaming) break;
                
                frame = std::move(frameQueue.front());
                frameQueue.pop();
            }
            
            // Send to all connected clients
            broadcastFrame(frame);
        }
    }
    
    void broadcastFrame(const Frame& frame) {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        
        // Create frame packet
        std::vector<uint8_t> packet;
        
        // Header: frame number (4 bytes) + timestamp (8 bytes) + data size (4 bytes)
        packet.resize(16);
        
        *reinterpret_cast<uint32_t*>(&packet[0]) = frame.frameNumber;
        *reinterpret_cast<uint64_t*>(&packet[4]) = frame.timestamp.time_since_epoch().count();
        *reinterpret_cast<uint32_t*>(&packet[12]) = frame.data.size();
        
        // Append compressed frame data
        packet.insert(packet.end(), frame.data.begin(), frame.data.end());
        
        // Send to all connections
        auto it = activeConnections.begin();
        while (it != activeConnections.end()) {
            if ((*it)->isConnected) {
                (*it)->send(packet);
                ++it;
            } else {
                // Remove disconnected clients
                it = activeConnections.erase(it);
            }
        }
    }
    
    // REST API endpoint handlers
    void startStreaming(WebSocketConnection* conn) {
        {
            std::lock_guard<std::mutex> lock(connectionsMutex);
            activeConnections.push_back(conn);
        }
        
        if (!streaming) {
            streaming = true;
            std::thread(&VideoStreamManager::streamWorker, this).detach();
        }
    }
    
    void stopStreaming(WebSocketConnection* conn) {
        {
            std::lock_guard<std::mutex> lock(connectionsMutex);
            activeConnections.erase(
                std::remove(activeConnections.begin(), activeConnections.end(), conn),
                activeConnections.end()
            );
        }
        
        if (activeConnections.empty()) {
            streaming = false;
            frameAvailable.notify_all();
        }
    }
    
    void setStreamingOptions(int fps, int quality) {
        targetFPS = std::max(1, std::min(60, fps));
        compressionQuality = std::max(1, std::min(100, quality));
    }
    
private:
    uint32_t getFrameNumber() {
        static uint32_t frameNum = 0;
        return frameNum++;
    }
    
    std::vector<uint8_t> compressFrame(uint8_t* buf, int width, int height) {
        // In real implementation:
        // 1. Convert NES palette to RGB
        // 2. Compress using H.264 or VP8/VP9
        // 3. Or use JPEG for simplicity
        
        std::vector<uint8_t> compressed;
        compressed.resize(width * height * 3 / 10); // Simulate 10:1 compression
        
        // Fill with dummy data
        for (size_t i = 0; i < compressed.size(); i++) {
            compressed[i] = buf[i % (width * height)];
        }
        
        return compressed;
    }
};

// Integration with REST API server
class RestApiVideoEndpoints {
    VideoStreamManager streamManager;
    
public:
    // WebSocket endpoint: /api/video/stream
    void handleVideoStream(/* websocket connection */) {
        std::cout << "Client connected to video stream\n";
        
        auto conn = new WebSocketConnection();
        
        // Parse options from query parameters
        // int fps = req.get_param_value("fps", 30);
        // int quality = req.get_param_value("quality", 85);
        
        streamManager.setStreamingOptions(30, 85);
        streamManager.startStreaming(conn);
        
        // In real implementation, this would handle WebSocket lifecycle
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        streamManager.stopStreaming(conn);
        delete conn;
        
        std::cout << "Client disconnected from video stream\n";
    }
    
    // Called from emulator thread
    void onFrameComplete(uint8_t* XBuf, int width, int height) {
        streamManager.captureFrame(XBuf, width, height);
    }
};

// Example usage in fceuWrapperUpdate()
/*
int fceuWrapperUpdate(void) {
    fceuWrapperLock();
    
    // Process REST API commands
    processApiCommands();
    
    if (GameInfo) {
        DoFun(frameskip, periodic_saves);
        
        // Capture frame for video streaming
        if (restApi && restApi->isVideoStreamActive()) {
            restApi->onFrameComplete(XBuf, 256, 240);
        }
    }
    
    fceuWrapperUnLock();
}
*/

// JavaScript client example
const char* CLIENT_EXAMPLE = R"(
// JavaScript WebSocket client for video streaming

class FCEUXVideoClient {
    constructor(host = 'localhost', port = 8080) {
        this.ws = null;
        this.canvas = document.getElementById('fceux-video');
        this.ctx = this.canvas.getContext('2d');
        this.frameCount = 0;
    }
    
    connect() {
        this.ws = new WebSocket(`ws://${this.host}:${this.port}/api/video/stream`);
        this.ws.binaryType = 'arraybuffer';
        
        this.ws.onopen = () => {
            console.log('Connected to FCEUX video stream');
        };
        
        this.ws.onmessage = (event) => {
            this.handleFrame(event.data);
        };
        
        this.ws.onclose = () => {
            console.log('Disconnected from FCEUX video stream');
        };
    }
    
    handleFrame(data) {
        const view = new DataView(data);
        
        // Parse header
        const frameNumber = view.getUint32(0, true);
        const timestamp = view.getBigUint64(4, true);
        const dataSize = view.getUint32(12, true);
        
        // Decompress and display frame
        const frameData = new Uint8Array(data, 16, dataSize);
        this.displayFrame(frameData);
        
        this.frameCount++;
        if (this.frameCount % 30 === 0) {
            console.log(`Frame ${frameNumber} received`);
        }
    }
    
    displayFrame(compressedData) {
        // Decompress and convert to ImageData
        // Then draw to canvas
        // this.ctx.putImageData(imageData, 0, 0);
    }
}
)";

int main() {
    std::cout << "REST API Video Streaming Design\n";
    std::cout << "================================\n\n";
    
    RestApiVideoEndpoints api;
    
    // Simulate video streaming session
    std::cout << "Starting video stream simulation...\n";
    api.handleVideoStream();
    
    std::cout << "\nClient JavaScript Example:\n";
    std::cout << CLIENT_EXAMPLE << std::endl;
    
    return 0;
}