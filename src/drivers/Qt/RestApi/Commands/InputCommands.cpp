#include "InputCommands.h"
#include "../Utils/AddressParser.h"
#include "../../../../fceu.h"
#include "../../fceuWrapper.h"
#include "../../../../lib/json.hpp"
#include "../InputApi.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

// Static member definitions
std::vector<PendingRelease> InputReleaseManager::pendingReleases;
std::mutex InputReleaseManager::releaseMutex;

// Button name mapping
static const std::unordered_map<std::string, uint8_t> buttonNameMap = {
    {"A", JOY_A},
    {"B", JOY_B},
    {"Select", JOY_SELECT},
    {"Start", JOY_START},
    {"Up", JOY_UP},
    {"Down", JOY_DOWN},
    {"Left", JOY_LEFT},
    {"Right", JOY_RIGHT}
};

uint8_t buttonNamesToBitmask(const std::vector<std::string>& buttonNames) {
    uint8_t mask = 0;
    for (const auto& name : buttonNames) {
        auto it = buttonNameMap.find(name);
        if (it == buttonNameMap.end()) {
            throw std::runtime_error("Invalid button name: " + name);
        }
        mask |= it->second;
    }
    return mask;
}

std::vector<std::string> bitmaskToButtonNames(uint8_t mask) {
    std::vector<std::string> names;
    for (const auto& pair : buttonNameMap) {
        if (mask & pair.second) {
            names.push_back(pair.first);
        }
    }
    return names;
}

// InputReleaseManager implementation
void InputReleaseManager::addPendingRelease(uint8_t port, uint8_t buttonMask, int releaseFrame) {
    std::lock_guard<std::mutex> lock(releaseMutex);
    pendingReleases.push_back({port, buttonMask, releaseFrame});
}

void InputReleaseManager::processPendingReleases() {
    std::lock_guard<std::mutex> lock(releaseMutex);
    
    if (pendingReleases.empty()) {
        return;
    }
    
    int currentFrame = currFrameCounter;
    auto it = pendingReleases.begin();
    
    while (it != pendingReleases.end()) {
        if (currentFrame >= it->releaseFrame) {
            // Clear the API overlay for these buttons
            // This will stop forcing them on
            uint8_t currentMask2 = apiJoypadMask2[it->port];
            apiJoypadMask2[it->port] = currentMask2 & ~it->buttonMask;
            it = pendingReleases.erase(it);
        } else {
            ++it;
        }
    }
}

void InputReleaseManager::clearAll() {
    std::lock_guard<std::mutex> lock(releaseMutex);
    pendingReleases.clear();
}

// Result structure implementations
std::string InputStatusResult::toJson() const {
    json result;
    
    // Port 1
    result["port1"]["connected"] = port1.connected;
    if (port1.connected) {
        json buttons;
        for (const auto& pair : port1.buttons) {
            buttons[pair.first] = pair.second;
        }
        result["port1"]["buttons"] = buttons;
    } else {
        result["port1"]["buttons"] = nullptr;
    }
    
    // Port 2
    result["port2"]["connected"] = port2.connected;
    if (port2.connected) {
        json buttons;
        for (const auto& pair : port2.buttons) {
            buttons[pair.first] = pair.second;
        }
        result["port2"]["buttons"] = buttons;
    } else {
        result["port2"]["buttons"] = nullptr;
    }
    
    return result.dump();
}

std::string InputPressResult::toJson() const {
    json result;
    result["success"] = success;
    result["port"] = port + 1; // Convert 0-based to 1-based for API
    result["buttons_pressed"] = buttonsPressed;
    result["duration_ms"] = durationMs;
    return result.dump();
}

std::string InputReleaseResult::toJson() const {
    json result;
    result["success"] = success;
    result["port"] = port + 1; // Convert 0-based to 1-based for API
    result["buttons_released"] = buttonsReleased;
    return result.dump();
}

std::string InputStateResult::toJson() const {
    json result;
    result["success"] = success;
    result["port"] = port + 1; // Convert 0-based to 1-based for API
    
    // Include current state as button map
    json buttons;
    for (const auto& pair : buttonNameMap) {
        buttons[pair.first] = (state & pair.second) != 0;
    }
    result["state"] = buttons;
    
    return result.dump();
}

// Command implementations
void InputStatusCommand::execute() {
    InputStatusResult result;
    
    FCEU_WRAPPER_LOCK();
    
    if (GameInfo == NULL) {
        FCEU_WRAPPER_UNLOCK();
        throw std::runtime_error("No game loaded");
    }
    
    // Read controller states
    // Port 1 (controller 0)
    result.port1.connected = true;
    uint8_t state0 = joy[0];
    for (const auto& pair : buttonNameMap) {
        result.port1.buttons[pair.first] = (state0 & pair.second) != 0;
    }
    
    // Port 2 (controller 1) 
    result.port2.connected = true;
    uint8_t state1 = joy[1];
    for (const auto& pair : buttonNameMap) {
        result.port2.buttons[pair.first] = (state1 & pair.second) != 0;
    }
    
    // TODO: Check actual connection status based on input configuration
    
    FCEU_WRAPPER_UNLOCK();
    
    resultPromise.set_value(result);
}

InputPressCommand::InputPressCommand(int portNum, const std::vector<std::string>& btns, int duration)
    : port(portNum - 1), buttons(btns), durationMs(duration) {
    if (port > 3) {
        throw std::runtime_error("Invalid port number");
    }
}

void InputPressCommand::execute() {
    InputPressResult result;
    result.port = port;
    result.durationMs = durationMs;
    
    FCEU_WRAPPER_LOCK();
    
    if (GameInfo == NULL) {
        FCEU_WRAPPER_UNLOCK();
        throw std::runtime_error("No game loaded");
    }
    
    try {
        // Convert button names to bitmask
        uint8_t buttonMask = buttonNamesToBitmask(buttons);
        
        // Use the API overlay system to press buttons
        // This sets bits in the OR mask to force buttons on
        FCEU_ApiSetJoypad(port, buttonMask, true);
        
        // Schedule release if duration specified
        if (durationMs > 0) {
            // Calculate release frame
            // NES runs at ~60 FPS (NTSC), so 1 frame ≈ 16.67ms
            int frames = (durationMs + 16) / 17; // Round up
            if (frames < 1) frames = 1;
            
            int releaseFrame = currFrameCounter + frames;
            InputReleaseManager::addPendingRelease(port, buttonMask, releaseFrame);
        } else {
            // If no duration, press for one frame only
            InputReleaseManager::addPendingRelease(port, buttonMask, currFrameCounter + 1);
        }
        
        result.success = true;
        result.buttonsPressed = buttons;
        
    } catch (const std::exception& e) {
        FCEU_WRAPPER_UNLOCK();
        throw;
    }
    
    FCEU_WRAPPER_UNLOCK();
    
    resultPromise.set_value(result);
}

InputReleaseCommand::InputReleaseCommand(int portNum, const std::vector<std::string>& btns)
    : port(portNum - 1), buttons(btns) {
    if (port > 3) {
        throw std::runtime_error("Invalid port number");
    }
}

void InputReleaseCommand::execute() {
    InputReleaseResult result;
    result.port = port;
    
    FCEU_WRAPPER_LOCK();
    
    if (GameInfo == NULL) {
        FCEU_WRAPPER_UNLOCK();
        throw std::runtime_error("No game loaded");
    }
    
    try {
        if (buttons.empty()) {
            // Release all buttons - clear all API control
            FCEU_ApiClearJoypad(port);
            result.buttonsReleased = bitmaskToButtonNames(0xFF);
        } else {
            // Release specific buttons
            uint8_t buttonMask = buttonNamesToBitmask(buttons);
            // Clear these buttons from the OR mask
            apiJoypadMask2[port] &= ~buttonMask;
            result.buttonsReleased = buttons;
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        FCEU_WRAPPER_UNLOCK();
        throw;
    }
    
    FCEU_WRAPPER_UNLOCK();
    
    resultPromise.set_value(result);
}

InputStateCommand::InputStateCommand(int portNum, const std::unordered_map<std::string, bool>& newState)
    : port(portNum - 1), state(newState) {
    if (port > 3) {
        throw std::runtime_error("Invalid port number");
    }
}

void InputStateCommand::execute() {
    InputStateResult result;
    result.port = port;
    
    FCEU_WRAPPER_LOCK();
    
    if (GameInfo == NULL) {
        FCEU_WRAPPER_UNLOCK();
        throw std::runtime_error("No game loaded");
    }
    
    // Build new state from button map
    uint8_t buttonsToPress = 0;
    uint8_t buttonsToClear = 0;
    
    // First, determine which buttons should be on/off
    for (const auto& pair : buttonNameMap) {
        auto stateIt = state.find(pair.first);
        if (stateIt != state.end() && stateIt->second) {
            // Button should be pressed
            buttonsToPress |= pair.second;
        } else {
            // Button should be released
            buttonsToClear |= pair.second;
        }
    }
    
    // Clear all API control first
    FCEU_ApiClearJoypad(port);
    
    // Force buttons on using OR mask
    if (buttonsToPress) {
        FCEU_ApiSetJoypad(port, buttonsToPress, true);
    }
    
    // Force buttons off using AND mask
    if (buttonsToClear) {
        FCEU_ApiSetJoypad(port, buttonsToClear, false);
    }
    
    result.success = true;
    result.state = buttonsToPress;  // Return what buttons are being pressed
    
    FCEU_WRAPPER_UNLOCK();
    
    resultPromise.set_value(result);
}