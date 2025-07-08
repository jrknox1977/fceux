#include "FceuxApiServer.h"
#include "../../../lib/httplib.h"
#include "../../../lib/json.hpp"
#include "../../../version.h"
#include "EmulationController.h"
#include "RomInfoController.h"
#include "CommandQueue.h"
#include "CommandExecution.h"
#include "Commands/MemoryReadCommand.h"
#include "Utils/AddressParser.h"
#include <QDateTime>
#include <QtGlobal>
#include <memory>
#include <stdexcept>

using json = nlohmann::json;

FceuxApiServer::FceuxApiServer(QObject* parent)
    : RestApiServer(parent)
{
}

void FceuxApiServer::registerRoutes()
{
    // System information endpoints
    addGetRoute("/api/system/info", 
        [this](const httplib::Request& req, httplib::Response& res) {
            handleSystemInfo(req, res);
        });

    addGetRoute("/api/system/ping",
        [this](const httplib::Request& req, httplib::Response& res) {
            handleSystemPing(req, res);
        });

    addGetRoute("/api/system/capabilities",
        [this](const httplib::Request& req, httplib::Response& res) {
            handleSystemCapabilities(req, res);
        });
    
    // Emulation control endpoints
    addPostRoute("/api/emulation/pause", EmulationController::handlePause);
    addPostRoute("/api/emulation/resume", EmulationController::handleResume);
    addGetRoute("/api/emulation/status", EmulationController::handleStatus);
    
    // ROM information endpoint
    addGetRoute("/api/rom/info", RomInfoController::handleRomInfo);
    
    // Memory access endpoints
    addGetRoute("/api/memory/([0-9a-fA-Fx]+)", 
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Extract address from URL parameter
                std::string addressStr = req.matches[1];
                
                // Parse address using utility
                uint16_t address = parseAddress(QString::fromStdString(addressStr));
                
                // Create command
                auto cmd = std::unique_ptr<ApiCommandWithResult<MemoryReadResult>>(new MemoryReadCommand(address));
                
                // Execute command with 1 second timeout
                auto future = executeCommand(std::move(cmd), 1000);
                
                // Wait for result
                MemoryReadResult result = waitForResult(future, 1000);
                
                // Return success response
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                // Handle specific errors
                std::string errorMsg = e.what();
                json error;
                error["error"] = errorMsg;
                
                // Map error messages to appropriate HTTP status codes
                if (errorMsg.find("Invalid address") != std::string::npos ||
                    errorMsg.find("Address out of range") != std::string::npos ||
                    errorMsg.find("Invalid hex format") != std::string::npos) {
                    res.status = 400;  // Bad Request
                } else if (errorMsg == "No game loaded" || 
                          errorMsg == "No ROM loaded") {
                    res.status = 503;  // Service Unavailable
                } else if (errorMsg == "Command execution timeout") {
                    res.status = 504;  // Gateway Timeout
                } else {
                    res.status = 500;  // Internal Server Error
                }
                
                res.set_content(error.dump(), "application/json");
                
            } catch (const std::exception& e) {
                // Handle any other exceptions
                res.status = 500;
                json error;
                error["error"] = e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    // TODO: Add input validation framework for future POST/PUT endpoints
}

void FceuxApiServer::handleSystemInfo(const httplib::Request& req, httplib::Response& res)
{
    json response;
    
    // FCEUX version information
    response["version"] = FCEU_VERSION_STRING;
    
    // Build date from compile time
    response["build_date"] = __DATE__;
    
    // Qt version
    response["qt_version"] = qVersion();
    
    // API version (hardcoded for now, can be made configurable later)
    response["api_version"] = "1.0.0";
    
    // Platform
#ifdef __linux__
    response["platform"] = "linux";
#elif defined(_WIN32)
    response["platform"] = "windows";
#elif defined(__APPLE__)
    response["platform"] = "macos";
#else
    response["platform"] = "unknown";
#endif

    res.set_content(response.dump(), "application/json");
    res.status = 200;
}

void FceuxApiServer::handleSystemPing(const httplib::Request& req, httplib::Response& res)
{
    json response;
    response["status"] = "ok";
    response["timestamp"] = getCurrentTimestamp().toStdString();
    
    res.set_content(response.dump(), "application/json");
    res.status = 200;
}

void FceuxApiServer::handleSystemCapabilities(const httplib::Request& req, httplib::Response& res)
{
    json response;
    
    // List of available endpoints
    response["endpoints"] = json::array({
        "/api/system/info",
        "/api/system/ping",
        "/api/system/capabilities",
        "/api/emulation/pause",
        "/api/emulation/resume",
        "/api/emulation/status",
        "/api/rom/info",
        "/api/memory/{address}"
    });
    
    // Feature flags
    response["features"] = {
        {"emulation_control", true},
        {"memory_access", true},
        {"save_states", false},
        {"screenshots", false}
    };
    
    res.set_content(response.dump(), "application/json");
    res.status = 200;
}

QString FceuxApiServer::getCurrentTimestamp() const
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}