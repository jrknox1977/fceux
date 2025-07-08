#include "RomInfoController.h"
#include "RomInfoCommands.h"
#include "CommandQueue.h"
#include "CommandExecution.h"
#include "../../../lib/httplib.h"
#include <memory>
#include <sstream>

// Timeout for command execution (2 seconds)
static constexpr unsigned int COMMAND_TIMEOUT_MS = 2000;

void RomInfoController::handleRomInfo(const httplib::Request& req, httplib::Response& res) {
    try {
        // Create and execute ROM info command
        auto cmd = std::unique_ptr<ApiCommandWithResult<RomInfo>>(new RomInfoCommand());
        auto future = executeCommand(std::move(cmd), COMMAND_TIMEOUT_MS);
        
        // Wait for result
        RomInfo info = waitForResult(future, COMMAND_TIMEOUT_MS);
        
        // Return ROM info as JSON
        res.set_content(info.toJson(), "application/json");
        res.status = 200;
        
    } catch (const std::exception& e) {
        // Handle any exceptions
        std::ostringstream json;
        json << "{";
        json << "\"success\":false,";
        json << "\"error\":\"" << e.what() << "\"";
        json << "}";
        
        res.status = 500;
        res.set_content(json.str(), "application/json");
    }
}