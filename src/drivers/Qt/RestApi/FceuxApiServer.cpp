#include "FceuxApiServer.h"
#include "../../../lib/httplib.h"
#include "../../../lib/json.hpp"
#include "../../../version.h"
#include "EmulationController.h"
#include "RomInfoController.h"
#include "CommandQueue.h"
#include "CommandExecution.h"
#include "Commands/MemoryReadCommand.h"
#include "Commands/InputCommands.h"
#include "Commands/ScreenshotCommands.h"
#include "Commands/SaveStateCommands.h"
#include "Commands/MemoryRangeCommands.h"
#include "InputApi.h"
#include "Utils/AddressParser.h"
#include <QDateTime>
#include <QtGlobal>
#include <memory>
#include <stdexcept>

using json = nlohmann::json;

FceuxApiServer::FceuxApiServer(QObject* parent)
    : RestApiServer(parent)
{
    // Initialize the API input system
    FCEU_ApiInputInit();
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
    
    // Memory range read endpoint
    addGetRoute("/api/memory/range/([0-9a-fA-Fx]+)/([0-9]+)",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Extract parameters from URL
                std::string startStr = req.matches[1];
                std::string lengthStr = req.matches[2];
                
                // Parse start address
                uint16_t startAddress = parseAddress(QString::fromStdString(startStr));
                
                // Parse length
                uint16_t length = std::stoi(lengthStr);
                
                // Create command
                auto cmd = std::unique_ptr<ApiCommandWithResult<MemoryRangeResult>>(
                    new MemoryRangeReadCommand(startAddress, length));
                
                // Execute with 2 second timeout for larger reads
                auto future = executeCommand(std::move(cmd), 2000);
                MemoryRangeResult result = waitForResult(future, 2000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                std::string errorMsg = e.what();
                json error;
                error["error"] = errorMsg;
                
                if (errorMsg.find("Invalid address") != std::string::npos ||
                    errorMsg.find("Address range exceeds") != std::string::npos ||
                    errorMsg.find("Length must be") != std::string::npos ||
                    errorMsg.find("Length exceeds maximum") != std::string::npos) {
                    res.status = 400;  // Bad Request
                } else if (errorMsg == "No game loaded") {
                    res.status = 503;  // Service Unavailable
                } else if (errorMsg == "Command execution timeout") {
                    res.status = 504;  // Gateway Timeout
                } else {
                    res.status = 500;  // Internal Server Error
                }
                
                res.set_content(error.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                json error;
                error["error"] = e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    // Memory range write endpoint
    addPostRoute("/api/memory/range/([0-9a-fA-Fx]+)",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Extract start address from URL
                std::string startStr = req.matches[1];
                uint16_t startAddress = parseAddress(QString::fromStdString(startStr));
                
                // Parse JSON body for base64 data
                json body = json::parse(req.body);
                if (!body.contains("data") || !body["data"].is_string()) {
                    throw std::runtime_error("Missing or invalid 'data' field");
                }
                
                // Decode base64 data
                std::string base64Data = body["data"];
                QByteArray encodedData = QByteArray::fromStdString(base64Data);
                QByteArray decodedData = QByteArray::fromBase64(encodedData);
                
                // Convert to vector
                std::vector<uint8_t> data;
                data.reserve(decodedData.size());
                for (int i = 0; i < decodedData.size(); i++) {
                    data.push_back(static_cast<uint8_t>(decodedData[i]));
                }
                
                // Create command
                auto cmd = std::unique_ptr<ApiCommandWithResult<MemoryWriteResult>>(
                    new MemoryRangeWriteCommand(startAddress, data));
                
                // Execute with 2 second timeout
                auto future = executeCommand(std::move(cmd), 2000);
                MemoryWriteResult result = waitForResult(future, 2000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                std::string errorMsg = e.what();
                json error;
                error["error"] = errorMsg;
                
                if (errorMsg.find("Invalid address") != std::string::npos ||
                    errorMsg.find("Address range exceeds") != std::string::npos ||
                    errorMsg.find("Length must be") != std::string::npos ||
                    errorMsg.find("Length exceeds maximum") != std::string::npos) {
                    res.status = 400;  // Bad Request
                } else if (errorMsg == "No game loaded") {
                    res.status = 503;  // Service Unavailable
                } else if (errorMsg == "Command execution timeout") {
                    res.status = 504;  // Gateway Timeout
                } else {
                    res.status = 500;  // Internal Server Error
                }
                
                res.set_content(error.dump(), "application/json");
            } catch (const json::exception& e) {
                res.status = 400;
                json error;
                error["error"] = std::string("Invalid JSON: ") + e.what();
                res.set_content(error.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                json error;
                error["error"] = e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    // Memory batch operations endpoint
    addPostRoute("/api/memory/batch",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Parse JSON body
                json body = json::parse(req.body);
                if (!body.contains("operations") || !body["operations"].is_array()) {
                    throw std::runtime_error("Missing or invalid 'operations' array");
                }
                
                // Parse operations
                std::vector<BatchOperation> operations;
                for (const auto& op : body["operations"]) {
                    BatchOperation batchOp;
                    
                    // Get operation type
                    if (!op.contains("type") || !op["type"].is_string()) {
                        throw std::runtime_error("Missing or invalid operation 'type'");
                    }
                    batchOp.type = op["type"];
                    
                    // Get address
                    if (!op.contains("address") || !op["address"].is_string()) {
                        throw std::runtime_error("Missing or invalid 'address'");
                    }
                    std::string addrStr = op["address"];
                    batchOp.address = parseAddress(QString::fromStdString(addrStr));
                    
                    // Handle type-specific fields
                    if (batchOp.type == "read") {
                        if (!op.contains("length") || !op["length"].is_number_integer()) {
                            throw std::runtime_error("Read operation missing 'length'");
                        }
                        batchOp.length = op["length"];
                    } else if (batchOp.type == "write") {
                        if (!op.contains("data") || !op["data"].is_string()) {
                            throw std::runtime_error("Write operation missing 'data'");
                        }
                        
                        // Decode base64 data
                        std::string base64Data = op["data"];
                        QByteArray encodedData = QByteArray::fromStdString(base64Data);
                        QByteArray decodedData = QByteArray::fromBase64(encodedData);
                        
                        // Convert to vector
                        batchOp.data.reserve(decodedData.size());
                        for (int i = 0; i < decodedData.size(); i++) {
                            batchOp.data.push_back(static_cast<uint8_t>(decodedData[i]));
                        }
                    }
                    
                    operations.push_back(batchOp);
                }
                
                // Create command
                auto cmd = std::unique_ptr<ApiCommandWithResult<MemoryBatchResult>>(
                    new MemoryBatchCommand(operations));
                
                // Execute with 5 second timeout for batch operations
                auto future = executeCommand(std::move(cmd), 5000);
                MemoryBatchResult result = waitForResult(future, 5000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                std::string errorMsg = e.what();
                json error;
                error["error"] = errorMsg;
                
                if (errorMsg.find("Invalid address") != std::string::npos ||
                    errorMsg.find("Address range exceeds") != std::string::npos ||
                    errorMsg.find("Length must be") != std::string::npos ||
                    errorMsg.find("Length exceeds maximum") != std::string::npos) {
                    res.status = 400;  // Bad Request
                } else if (errorMsg == "No game loaded") {
                    res.status = 503;  // Service Unavailable
                } else if (errorMsg == "Command execution timeout") {
                    res.status = 504;  // Gateway Timeout
                } else {
                    res.status = 500;  // Internal Server Error
                }
                
                res.set_content(error.dump(), "application/json");
            } catch (const json::exception& e) {
                res.status = 400;
                json error;
                error["error"] = std::string("Invalid JSON: ") + e.what();
                res.set_content(error.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                json error;
                error["error"] = e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    // Input control endpoints
    addGetRoute("/api/input/status",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto cmd = std::unique_ptr<ApiCommandWithResult<InputStatusResult>>(new InputStatusCommand());
                auto future = executeCommand(std::move(cmd), 1000);
                InputStatusResult result = waitForResult(future, 1000);
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
            } catch (const std::runtime_error& e) {
                handleInputError(e, res);
            }
        });
    
    addPostRoute("/api/input/port/([12])/press",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Parse port number
                int port = std::stoi(req.matches[1]);
                
                // Parse JSON body
                json body = json::parse(req.body);
                
                // Extract buttons array
                if (!body.contains("buttons") || !body["buttons"].is_array()) {
                    throw std::runtime_error("Missing or invalid 'buttons' array");
                }
                
                std::vector<std::string> buttons;
                for (const auto& btn : body["buttons"]) {
                    if (!btn.is_string()) {
                        throw std::runtime_error("Button names must be strings");
                    }
                    buttons.push_back(btn.get<std::string>());
                }
                
                // Get optional duration
                int duration = body.value("duration_ms", 16);
                
                // Create and execute command
                auto cmd = std::unique_ptr<ApiCommandWithResult<InputPressResult>>(
                    new InputPressCommand(port, buttons, duration));
                auto future = executeCommand(std::move(cmd), 1000);
                InputPressResult result = waitForResult(future, 1000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                handleInputError(e, res);
            } catch (const json::exception& e) {
                res.status = 400;
                json error;
                error["error"] = std::string("Invalid JSON: ") + e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    addPostRoute("/api/input/port/([12])/release",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Parse port number
                int port = std::stoi(req.matches[1]);
                
                // Parse JSON body
                std::vector<std::string> buttons;
                if (!req.body.empty()) {
                    json body = json::parse(req.body);
                    if (body.contains("buttons") && body["buttons"].is_array()) {
                        for (const auto& btn : body["buttons"]) {
                            if (!btn.is_string()) {
                                throw std::runtime_error("Button names must be strings");
                            }
                            buttons.push_back(btn.get<std::string>());
                        }
                    }
                }
                
                // Create and execute command
                auto cmd = std::unique_ptr<ApiCommandWithResult<InputReleaseResult>>(
                    new InputReleaseCommand(port, buttons));
                auto future = executeCommand(std::move(cmd), 1000);
                InputReleaseResult result = waitForResult(future, 1000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                handleInputError(e, res);
            } catch (const json::exception& e) {
                res.status = 400;
                json error;
                error["error"] = std::string("Invalid JSON: ") + e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    addPostRoute("/api/input/port/([12])/state",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Parse port number
                int port = std::stoi(req.matches[1]);
                
                // Parse JSON body
                json body = json::parse(req.body);
                
                // Convert JSON to state map
                std::unordered_map<std::string, bool> state;
                for (auto it = body.begin(); it != body.end(); ++it) {
                    if (!it.value().is_boolean()) {
                        throw std::runtime_error("Button states must be boolean values");
                    }
                    state[it.key()] = it.value().get<bool>();
                }
                
                // Create and execute command
                auto cmd = std::unique_ptr<ApiCommandWithResult<InputStateResult>>(
                    new InputStateCommand(port, state));
                auto future = executeCommand(std::move(cmd), 1000);
                InputStateResult result = waitForResult(future, 1000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                handleInputError(e, res);
            } catch (const json::exception& e) {
                res.status = 400;
                json error;
                error["error"] = std::string("Invalid JSON: ") + e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    // Screenshot endpoints
    addPostRoute("/api/screenshot",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Default values
                std::string format = "png";
                std::string encoding = "file";
                std::string path = "";
                
                // Parse optional JSON body
                if (!req.body.empty()) {
                    json body = json::parse(req.body);
                    format = body.value("format", "png");
                    encoding = body.value("encoding", "file");
                    path = body.value("path", "");
                    
                    // Debug
                    printf("FceuxApiServer: screenshot request - format=%s, encoding=%s, path=%s\n", 
                           format.c_str(), encoding.c_str(), path.c_str());
                }
                
                // Create and execute command
                auto cmd = std::unique_ptr<ApiCommandWithResult<ScreenshotResult>>(
                    new ScreenshotCommand(format, encoding, path));
                auto future = executeCommand(std::move(cmd), 2000); // 2 second timeout for screenshots
                ScreenshotResult result = waitForResult(future, 2000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                res.status = 500;
                json error;
                error["error"] = e.what();
                res.set_content(error.dump(), "application/json");
            } catch (const json::exception& e) {
                res.status = 400;
                json error;
                error["error"] = std::string("Invalid JSON: ") + e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    addGetRoute("/api/screenshot/last",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto cmd = std::unique_ptr<ApiCommandWithResult<ScreenshotResult>>(
                    new LastScreenshotCommand());
                auto future = executeCommand(std::move(cmd), 1000);
                ScreenshotResult result = waitForResult(future, 1000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                res.status = 500;
                json error;
                error["error"] = e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    // Save state endpoints
    addPostRoute("/api/savestate",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Default values
                int slot = 0;
                std::string path = "";
                
                // Parse optional JSON body
                if (!req.body.empty()) {
                    json body = json::parse(req.body);
                    slot = body.value("slot", 0);
                    path = body.value("path", "");
                    
                    // Validate slot
                    if (slot < -1 || slot > 9) {
                        res.status = 400;
                        json error;
                        error["error"] = "Invalid slot number. Must be -1 (memory) or 0-9";
                        res.set_content(error.dump(), "application/json");
                        return;
                    }
                }
                
                // Create and execute command
                auto cmd = std::unique_ptr<ApiCommandWithResult<SaveStateResult>>(
                    new SaveStateCommand(slot, path));
                auto future = executeCommand(std::move(cmd), 2000);
                SaveStateResult result = waitForResult(future, 2000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                res.status = 500;
                json error;
                error["error"] = e.what();
                res.set_content(error.dump(), "application/json");
            } catch (const json::exception& e) {
                res.status = 400;
                json error;
                error["error"] = std::string("Invalid JSON: ") + e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    addPostRoute("/api/loadstate",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Default values
                int slot = 0;
                std::string path = "";
                std::string data = "";
                
                // Parse optional JSON body
                if (!req.body.empty()) {
                    json body = json::parse(req.body);
                    slot = body.value("slot", 0);
                    path = body.value("path", "");
                    data = body.value("data", "");
                    
                    // Validate slot
                    if (slot < -1 || slot > 9) {
                        res.status = 400;
                        json error;
                        error["error"] = "Invalid slot number. Must be -1 (memory) or 0-9";
                        res.set_content(error.dump(), "application/json");
                        return;
                    }
                }
                
                // Create and execute command
                auto cmd = std::unique_ptr<ApiCommandWithResult<SaveStateResult>>(
                    new LoadStateCommand(slot, path, data));
                auto future = executeCommand(std::move(cmd), 2000);
                SaveStateResult result = waitForResult(future, 2000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
                res.status = 500;
                json error;
                error["error"] = e.what();
                res.set_content(error.dump(), "application/json");
            } catch (const json::exception& e) {
                res.status = 400;
                json error;
                error["error"] = std::string("Invalid JSON: ") + e.what();
                res.set_content(error.dump(), "application/json");
            }
        });
    
    addGetRoute("/api/savestate/list",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto cmd = std::unique_ptr<ApiCommandWithResult<SaveStateListResult>>(
                    new ListSaveStatesCommand());
                auto future = executeCommand(std::move(cmd), 1000);
                SaveStateListResult result = waitForResult(future, 1000);
                
                res.status = 200;
                res.set_content(result.toJson(), "application/json");
                
            } catch (const std::runtime_error& e) {
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
        "/api/memory/{address}",
        "/api/memory/range/{start}/{length}",
        "/api/memory/range/{start}",
        "/api/memory/batch",
        "/api/input/status",
        "/api/input/port/{port}/press",
        "/api/input/port/{port}/release",
        "/api/input/port/{port}/state",
        "/api/screenshot",
        "/api/screenshot/last",
        "/api/savestate",
        "/api/loadstate",
        "/api/savestate/list"
    });
    
    // Feature flags
    response["features"] = {
        {"emulation_control", true},
        {"memory_access", true},
        {"memory_range_access", true},
        {"input_control", true},
        {"save_states", true},
        {"screenshots", true}
    };
    
    res.set_content(response.dump(), "application/json");
    res.status = 200;
}

QString FceuxApiServer::getCurrentTimestamp() const
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

void FceuxApiServer::handleInputError(const std::runtime_error& e, httplib::Response& res)
{
    std::string errorMsg = e.what();
    json error;
    error["error"] = errorMsg;
    
    // Map error messages to appropriate HTTP status codes
    if (errorMsg.find("Invalid button name") != std::string::npos ||
        errorMsg.find("Invalid port number") != std::string::npos ||
        errorMsg.find("Missing or invalid") != std::string::npos) {
        res.status = 400;  // Bad Request
    } else if (errorMsg == "No game loaded") {
        res.status = 503;  // Service Unavailable
    } else if (errorMsg == "Command execution timeout") {
        res.status = 504;  // Gateway Timeout
    } else {
        res.status = 500;  // Internal Server Error
    }
    
    res.set_content(error.dump(), "application/json");
}