#ifndef __MEDIA_COMMANDS_H__
#define __MEDIA_COMMANDS_H__

#include "../RestApiCommands.h"
#include "../../../../types.h"
#include "../../../../fceu.h"
#include "../../../../lib/json.hpp"
#include <string>
#include <vector>
#include <cstddef>
#include <unordered_map>

using json = nlohmann::json;

// Forward declarations
extern FCEUGI* GameInfo;
extern int currFrameCounter;

/**
 * @brief Base class for media-related API results
 * 
 * Provides common JSON serialization interface
 */
struct MediaResult {
    bool success;
    std::string error;
    
    MediaResult() : success(false) {}
    
    virtual std::string toJson() const = 0;
    virtual ~MediaResult() = default;
    
protected:
    /**
     * @brief Add common fields to JSON object
     */
    void addCommonFields(json& j) const {
        j["success"] = success;
        if (!error.empty()) {
            j["error"] = error;
        }
    }
};

/**
 * @brief Result structure for screenshot operations
 */
struct ScreenshotResult : public MediaResult {
    std::string format;     // png, jpg, bmp
    std::string encoding;   // file, base64
    std::string filename;   // For file encoding
    std::string path;       // Full path for file encoding
    std::string data;       // Base64 data for base64 encoding
    
    std::string toJson() const override {
        json j;
        addCommonFields(j);
        
        if (success) {
            j["format"] = format;
            j["encoding"] = encoding;
            
            if (encoding == "file") {
                j["filename"] = filename;
                j["path"] = path;
            } else if (encoding == "base64") {
                j["data"] = data;
            }
        }
        
        return j.dump();
    }
};

/**
 * @brief Result structure for save state operations
 */
struct SaveStateResult : public MediaResult {
    int slot;          // -1 for custom names
    std::string name;       // Custom name if not using slot
    std::string filename;
    std::string timestamp;
    
    SaveStateResult() : slot(-1) {}
    
    std::string toJson() const override {
        json j;
        addCommonFields(j);
        
        if (success) {
            if (slot >= 0) {
                j["slot"] = slot;
            } else if (!name.empty()) {
                j["name"] = name;
            }
            j["filename"] = filename;
            j["timestamp"] = timestamp;
        }
        
        return j.dump();
    }
};

/**
 * @brief Info about a save state
 */
struct SaveStateInfo {
    int slot;
    std::string name;
    std::string filename;
    std::string timestamp;
    size_t size;
    bool exists;
    
    SaveStateInfo() : slot(-1), size(0), exists(false) {}
};

/**
 * @brief Result structure for save state listing
 */
struct SaveStateListResult : public MediaResult {
    // TODO: Fix compilation issue with vector members
    // std::vector<SaveStateInfo> slots;
    // std::vector<SaveStateInfo> custom;
    
    std::string toJson() const override {
        json j;
        addCommonFields(j);
        
        // TODO: Implement listing
        
        return j.dump();
    }
};

/**
 * @brief Result structure for frame advance operations
 */
struct FrameAdvanceResult : public MediaResult {
    int framesAdvanced;
    int currentFrame;
    
    FrameAdvanceResult() : framesAdvanced(0), currentFrame(0) {}
    
    std::string toJson() const override {
        json j;
        addCommonFields(j);
        
        if (success) {
            j["frames_advanced"] = framesAdvanced;
            j["current_frame"] = currentFrame;
        }
        
        return j.dump();
    }
};

/**
 * @brief Result structure for frame info queries
 */
struct FrameInfoResult : public MediaResult {
    int frameCount;
    int lagCount;
    double fps;
    int emulationSpeed;
    
    FrameInfoResult() : frameCount(0), lagCount(0), fps(0.0), emulationSpeed(100) {}
    
    std::string toJson() const override {
        json j;
        addCommonFields(j);
        
        if (success) {
            j["frame_count"] = frameCount;
            j["lag_count"] = lagCount;
            j["fps"] = fps;
            j["emulation_speed"] = emulationSpeed;
        }
        
        return j.dump();
    }
};

/**
 * @brief Result structure for pixel queries
 */
struct PixelResult : public MediaResult {
    int x;
    int y;
    struct {
        int r;
        int g;
        int b;
    } rgb;
    std::string hex;
    int paletteIndex;
    
    PixelResult() : x(0), y(0), paletteIndex(0) {
        rgb.r = 0;
        rgb.g = 0;
        rgb.b = 0;
    }
    
    std::string toJson() const override {
        json j;
        addCommonFields(j);
        
        if (success) {
            j["x"] = x;
            j["y"] = y;
            json rgbObj;
            rgbObj["r"] = rgb.r;
            rgbObj["g"] = rgb.g;
            rgbObj["b"] = rgb.b;
            j["rgb"] = rgbObj;
            j["hex"] = hex;
            j["palette_index"] = paletteIndex;
        }
        
        return j.dump();
    }
};

/**
 * @brief Base template for media commands
 * 
 * Provides common error handling and mutex management
 */
template<typename TResult>
class BaseMediaCommand : public ApiCommandWithResult<TResult> {
protected:
    /**
     * @brief Check if game is loaded and handle error if not
     * @return true if game is loaded, false otherwise
     */
    bool ensureGameLoaded() {
        if (GameInfo == NULL) {
            TResult result;
            result.success = false;
            result.error = "No game loaded";
            this->resultPromise.set_value(result);
            return false;
        }
        return true;
    }
    
    /**
     * @brief Set error result and complete the promise
     * @param error Error message
     */
    void setError(const std::string& error) {
        TResult result;
        result.success = false;
        result.error = error;
        this->resultPromise.set_value(result);
    }
};

#endif // __MEDIA_COMMANDS_H__