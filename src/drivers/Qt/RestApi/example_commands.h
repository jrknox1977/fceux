#ifndef __EXAMPLE_COMMANDS_H__
#define __EXAMPLE_COMMANDS_H__

#include "RestApiCommands.h"
#include "../../../fceu.h"
#include "../../../state.h"

/**
 * Example command to pause emulation
 */
class PauseCommand : public ApiCommandWithResult<bool> {
public:
    void execute() override {
        bool wasPaused = FCEUI_EmulationPaused();
        FCEUI_SetEmulationPaused(true);
        resultPromise.set_value(!wasPaused);  // Return true if we changed state
    }
    
    const char* name() const override {
        return "PauseCommand";
    }
};

/**
 * Example command to resume emulation
 */
class ResumeCommand : public ApiCommandWithResult<bool> {
public:
    void execute() override {
        bool wasPaused = FCEUI_EmulationPaused();
        FCEUI_SetEmulationPaused(false);
        resultPromise.set_value(wasPaused);  // Return true if we changed state
    }
    
    const char* name() const override {
        return "ResumeCommand";
    }
};

/**
 * Example command to get emulation status
 */
struct EmulationStatus {
    bool running;
    bool paused;
    bool romLoaded;
    int frameCount;
};

class StatusCommand : public ApiCommandWithResult<EmulationStatus> {
public:
    void execute() override {
        EmulationStatus status;
        status.romLoaded = (GameInfo != nullptr);
        status.paused = FCEUI_EmulationPaused();
        status.running = status.romLoaded && !status.paused;
        status.frameCount = status.romLoaded ? currFrameCounter : 0;
        
        resultPromise.set_value(status);
    }
    
    const char* name() const override {
        return "StatusCommand";
    }
};

/**
 * Example of how to use commands in REST endpoints with error handling
 * 
 * void handlePauseEndpoint(const httplib::Request& req, httplib::Response& res) {
 *     try {
 *         auto cmd = std::make_unique<PauseCommand>();
 *         auto future = executeCommand(std::move(cmd));
 *         
 *         // Wait for result with timeout
 *         bool changed = waitForResult(future, 2000);  // 2 second timeout
 *         
 *         // Return success response
 *         nlohmann::json response;
 *         response["success"] = true;
 *         response["state_changed"] = changed;
 *         res.set_content(response.dump(), "application/json");
 *         
 *     } catch (const std::exception& e) {
 *         // Command failed - return error response
 *         nlohmann::json error;
 *         error["success"] = false;
 *         error["error"] = e.what();
 *         res.status = 500;
 *         res.set_content(error.dump(), "application/json");
 *     }
 * }
 */

#endif // __EXAMPLE_COMMANDS_H__