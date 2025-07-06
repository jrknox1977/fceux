// REST API Screenshot Integration Test
// This demonstrates how the REST API will capture screenshots

#include <iostream>
#include <vector>
#include <cstring>
#include <fstream>

// Simulated FCEUX structures
struct FSettings_t {
    int FirstSLine = 0;
    int LastSLine = 239;
};

FSettings_t FSettings;

// Simulate 256x240 NES screen
uint8_t XBuf[256 * 240];

// Simulate color conversion (simplified)
uint32_t ModernDeemphColorMap(uint8_t* src, uint8_t* buf, int scale) {
    // In real implementation, this converts NES palette index to RGB
    // For now, just create a simple RGB value
    uint8_t palIndex = *src;
    
    // Simple palette simulation
    uint32_t r = (palIndex * 8) & 0xFF;
    uint32_t g = (palIndex * 4) & 0xFF;
    uint32_t b = (palIndex * 2) & 0xFF;
    
    return (r << 16) | (g << 8) | b;
}

// REST API Screenshot Generator
class ScreenshotGenerator {
public:
    struct PNGData {
        std::vector<uint8_t> data;
        bool success;
    };
    
    // Generate PNG data for REST API response
    static PNGData captureScreenshot() {
        PNGData result;
        result.success = false;
        
        int totalLines = FSettings.LastSLine - FSettings.FirstSLine + 1;
        int width = 256;
        int height = totalLines;
        
        // Allocate RGB buffer
        std::vector<uint8_t> rgbData;
        rgbData.reserve(width * height * 3);
        
        // Convert NES buffer to RGB
        uint8_t* src = XBuf + FSettings.FirstSLine * 256;
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                uint32_t color = ModernDeemphColorMap(src, XBuf, 1);
                
                rgbData.push_back((color >> 16) & 0xFF); // R
                rgbData.push_back((color >> 8) & 0xFF);  // G
                rgbData.push_back((color >> 0) & 0xFF);  // B
                
                src++;
            }
        }
        
        // In real implementation, compress to PNG using zlib
        // For now, just store raw RGB data
        result.data = std::move(rgbData);
        result.success = true;
        
        return result;
    }
    
    // Generate thumbnail with scaling
    static PNGData captureThumbnail(int targetWidth) {
        PNGData result;
        result.success = false;
        
        int totalLines = FSettings.LastSLine - FSettings.FirstSLine + 1;
        int srcWidth = 256;
        int srcHeight = totalLines;
        
        // Calculate target dimensions maintaining aspect ratio
        float scale = (float)targetWidth / srcWidth;
        int targetHeight = (int)(srcHeight * scale);
        
        // Simple nearest-neighbor scaling
        std::vector<uint8_t> rgbData;
        rgbData.reserve(targetWidth * targetHeight * 3);
        
        for (int y = 0; y < targetHeight; y++) {
            for (int x = 0; x < targetWidth; x++) {
                int srcX = (int)(x / scale);
                int srcY = (int)(y / scale);
                
                uint8_t* src = XBuf + (FSettings.FirstSLine + srcY) * 256 + srcX;
                uint32_t color = ModernDeemphColorMap(src, XBuf, 1);
                
                rgbData.push_back((color >> 16) & 0xFF); // R
                rgbData.push_back((color >> 8) & 0xFF);  // G
                rgbData.push_back((color >> 0) & 0xFF);  // B
            }
        }
        
        result.data = std::move(rgbData);
        result.success = true;
        
        return result;
    }
    
    // Get raw framebuffer data
    static std::vector<uint8_t> getRawFramebuffer() {
        int totalLines = FSettings.LastSLine - FSettings.FirstSLine + 1;
        int size = 256 * totalLines;
        
        std::vector<uint8_t> data;
        data.reserve(size);
        
        uint8_t* src = XBuf + FSettings.FirstSLine * 256;
        data.insert(data.end(), src, src + size);
        
        return data;
    }
};

// REST API endpoint handlers (pseudo-code)
void handleScreenshotEndpoint(/* httplib::Request& req, httplib::Response& res */) {
    // This would be called from the REST API server thread
    
    // Queue command to emulator thread
    // auto cmd = std::make_unique<ScreenshotCommand>();
    // commandQueue.push(std::move(cmd));
    
    // In emulator thread during fceuWrapperUpdate():
    auto screenshot = ScreenshotGenerator::captureScreenshot();
    
    if (screenshot.success) {
        // res.set_content(screenshot.data.data(), screenshot.data.size(), "image/png");
        std::cout << "Screenshot captured: " << screenshot.data.size() << " bytes" << std::endl;
    }
}

void handleThumbnailEndpoint(/* httplib::Request& req, httplib::Response& res */) {
    // Parse width parameter
    int width = 128; // default
    // if (req.has_param("width")) {
    //     width = std::stoi(req.get_param_value("width"));
    // }
    
    auto thumbnail = ScreenshotGenerator::captureThumbnail(width);
    
    if (thumbnail.success) {
        // res.set_content(thumbnail.data.data(), thumbnail.data.size(), "image/png");
        std::cout << "Thumbnail captured: " << width << "x" 
                  << (int)(240 * ((float)width / 256)) << " pixels" << std::endl;
    }
}

void handleFramebufferEndpoint(/* httplib::Request& req, httplib::Response& res */) {
    auto data = ScreenshotGenerator::getRawFramebuffer();
    
    // Return as binary data or base64
    // res.set_content(data.data(), data.size(), "application/octet-stream");
    std::cout << "Framebuffer captured: " << data.size() << " bytes" << std::endl;
}

int main() {
    // Initialize test pattern in XBuf
    for (int i = 0; i < 256 * 240; i++) {
        XBuf[i] = i % 64; // Simple test pattern
    }
    
    std::cout << "REST API Screenshot Integration Test\n";
    std::cout << "====================================\n\n";
    
    // Test screenshot capture
    std::cout << "1. Testing screenshot capture...\n";
    handleScreenshotEndpoint();
    
    // Test thumbnail generation
    std::cout << "\n2. Testing thumbnail generation...\n";
    handleThumbnailEndpoint();
    
    // Test raw framebuffer access
    std::cout << "\n3. Testing raw framebuffer access...\n";
    handleFramebufferEndpoint();
    
    return 0;
}