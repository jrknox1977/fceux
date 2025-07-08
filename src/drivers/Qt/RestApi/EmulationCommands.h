#ifndef __EMULATION_COMMANDS_H__
#define __EMULATION_COMMANDS_H__

#include "RestApiCommands.h"
#include "../../../fceu.h"
#include "../../../movie.h"
#include "../../../driver.h"
#include <sstream>

/**
 * @brief Emulation status information
 */
struct EmulationStatus {
    bool running;
    bool paused;
    bool romLoaded;
    double fps;
    int frameCount;
    
    /**
     * @brief Convert to JSON string
     * @return JSON representation of the status
     */
    std::string toJson() const {
        std::ostringstream json;
        json << "{";
        json << "\"running\":" << (running ? "true" : "false") << ",";
        json << "\"paused\":" << (paused ? "true" : "false") << ",";
        json << "\"rom_loaded\":" << (romLoaded ? "true" : "false") << ",";
        json << "\"fps\":" << fps << ",";
        json << "\"frame_count\":" << frameCount;
        json << "}";
        return json.str();
    }
};

/**
 * @brief Command to pause emulation
 * 
 * Returns true if the state was changed (i.e., emulation was running)
 */
class PauseCommand : public ApiCommandWithResult<bool> {
public:
    void execute() override {
        // Check if ROM is loaded
        if (!GameInfo) {
            throw std::runtime_error("No ROM loaded");
        }
        
        // Check current state
        bool wasPaused = FCEUI_EmulationPaused();
        
        // Set paused state
        FCEUI_SetEmulationPaused(1);
        
        // Return true if we changed the state
        resultPromise.set_value(!wasPaused);
    }
    
    const char* name() const override {
        return "PauseCommand";
    }
};

/**
 * @brief Command to resume emulation
 * 
 * Returns true if the state was changed (i.e., emulation was paused)
 */
class ResumeCommand : public ApiCommandWithResult<bool> {
public:
    void execute() override {
        // Check if ROM is loaded
        if (!GameInfo) {
            throw std::runtime_error("No ROM loaded");
        }
        
        // Check current state
        bool wasPaused = FCEUI_EmulationPaused();
        
        // Set unpaused state
        FCEUI_SetEmulationPaused(0);
        
        // Return true if we changed the state
        resultPromise.set_value(wasPaused);
    }
    
    const char* name() const override {
        return "ResumeCommand";
    }
};

/**
 * @brief Command to get emulation status
 */
class StatusCommand : public ApiCommandWithResult<EmulationStatus> {
public:
    void execute() override {
        EmulationStatus status;
        
        // Check if ROM is loaded
        status.romLoaded = (GameInfo != nullptr);
        
        // Get pause state
        status.paused = FCEUI_EmulationPaused();
        
        // Running = ROM loaded and not paused
        status.running = status.romLoaded && !status.paused;
        
        // Get FPS
        if (status.romLoaded) {
            // FCEUI_GetDesiredFPS returns fixed-point value
            // Convert to double: value / (2^24)
            int32 fpsFixed = FCEUI_GetDesiredFPS();
            status.fps = fpsFixed / 16777216.0;
        } else {
            status.fps = 0.0;
        }
        
        // Get frame count
        status.frameCount = status.romLoaded ? currFrameCounter : 0;
        
        resultPromise.set_value(status);
    }
    
    const char* name() const override {
        return "StatusCommand";
    }
};

#endif // __EMULATION_COMMANDS_H__