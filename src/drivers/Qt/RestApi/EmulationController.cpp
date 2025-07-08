#include "EmulationController.h"
#include "EmulationCommands.h"
#include "CommandQueue.h"
#include "CommandExecution.h"
#include "../../../lib/httplib.h"
#include <memory>
#include <sstream>

// Timeout for command execution (2 seconds)
static constexpr unsigned int COMMAND_TIMEOUT_MS = 2000;

std::string EmulationController::createErrorResponse(const std::string& error) {
    std::ostringstream json;
    json << "{";
    json << "\"success\":false,";
    json << "\"error\":\"" << error << "\"";
    json << "}";
    return json.str();
}

std::string EmulationController::createSuccessResponse(const std::string& state) {
    std::ostringstream json;
    json << "{";
    json << "\"success\":true,";
    json << "\"state\":\"" << state << "\"";
    json << "}";
    return json.str();
}

void EmulationController::handlePause(const httplib::Request& req, httplib::Response& res) {
    try {
        // Create and execute pause command
        auto cmd = std::unique_ptr<ApiCommandWithResult<bool>>(new PauseCommand());
        auto future = executeCommand(std::move(cmd), COMMAND_TIMEOUT_MS);
        
        // Wait for result
        bool stateChanged = waitForResult(future, COMMAND_TIMEOUT_MS);
        
        // Return success response
        res.set_content(createSuccessResponse("paused"), "application/json");
        res.status = 200;
        
    } catch (const std::runtime_error& e) {
        // Handle specific errors
        std::string errorMsg = e.what();
        if (errorMsg == "No ROM loaded") {
            res.status = 400;  // Bad Request
        } else {
            res.status = 500;  // Internal Server Error
        }
        res.set_content(createErrorResponse(errorMsg), "application/json");
        
    } catch (const std::exception& e) {
        // Handle any other exceptions
        res.status = 500;
        res.set_content(createErrorResponse(e.what()), "application/json");
    }
}

void EmulationController::handleResume(const httplib::Request& req, httplib::Response& res) {
    try {
        // Create and execute resume command
        auto cmd = std::unique_ptr<ApiCommandWithResult<bool>>(new ResumeCommand());
        auto future = executeCommand(std::move(cmd), COMMAND_TIMEOUT_MS);
        
        // Wait for result
        bool stateChanged = waitForResult(future, COMMAND_TIMEOUT_MS);
        
        // Return success response
        res.set_content(createSuccessResponse("resumed"), "application/json");
        res.status = 200;
        
    } catch (const std::runtime_error& e) {
        // Handle specific errors
        std::string errorMsg = e.what();
        if (errorMsg == "No ROM loaded") {
            res.status = 400;  // Bad Request
        } else {
            res.status = 500;  // Internal Server Error
        }
        res.set_content(createErrorResponse(errorMsg), "application/json");
        
    } catch (const std::exception& e) {
        // Handle any other exceptions
        res.status = 500;
        res.set_content(createErrorResponse(e.what()), "application/json");
    }
}

void EmulationController::handleStatus(const httplib::Request& req, httplib::Response& res) {
    try {
        // Create and execute status command
        auto cmd = std::unique_ptr<ApiCommandWithResult<EmulationStatus>>(new StatusCommand());
        auto future = executeCommand(std::move(cmd), COMMAND_TIMEOUT_MS);
        
        // Wait for result
        EmulationStatus status = waitForResult(future, COMMAND_TIMEOUT_MS);
        
        // Return status as JSON
        res.set_content(status.toJson(), "application/json");
        res.status = 200;
        
    } catch (const std::exception& e) {
        // Handle any exceptions
        res.status = 500;
        res.set_content(createErrorResponse(e.what()), "application/json");
    }
}