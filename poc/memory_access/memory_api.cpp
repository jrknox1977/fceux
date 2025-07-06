#include "memory_api.h"
#include "../../fceu.h"
#include "../../cheat.h"
#include "../../debug.h"
#include <algorithm>
#include <cstring>

namespace RestApi {

MemoryAPI* g_memoryAPI = nullptr;

MemoryAPI::MemoryAPI() {
    // Initialize if needed
}

MemoryAPI::~MemoryAPI() {
    // Clean up pending commands
}

std::future<MemoryResult> MemoryAPI::readByte(uint32_t address) {
    auto cmd = std::make_unique<MemoryCommand>();
    cmd->type = MemoryCommandType::READ_BYTE;
    cmd->address = address;
    cmd->length = 1;
    
    auto future = cmd->promise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        commandQueue.push(std::move(cmd));
    }
    
    return future;
}

std::future<MemoryResult> MemoryAPI::writeByte(uint32_t address, uint8_t value) {
    auto cmd = std::make_unique<MemoryCommand>();
    cmd->type = MemoryCommandType::WRITE_BYTE;
    cmd->address = address;
    cmd->data = {value};
    
    auto future = cmd->promise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        commandQueue.push(std::move(cmd));
    }
    
    return future;
}

std::future<MemoryResult> MemoryAPI::readRange(uint32_t address, uint32_t length) {
    auto cmd = std::make_unique<MemoryCommand>();
    cmd->type = MemoryCommandType::READ_RANGE;
    cmd->address = address;
    cmd->length = length;
    
    auto future = cmd->promise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        commandQueue.push(std::move(cmd));
    }
    
    return future;
}

std::future<MemoryResult> MemoryAPI::writeRange(uint32_t address, const std::vector<uint8_t>& data) {
    auto cmd = std::make_unique<MemoryCommand>();
    cmd->type = MemoryCommandType::WRITE_RANGE;
    cmd->address = address;
    cmd->data = data;
    
    auto future = cmd->promise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        commandQueue.push(std::move(cmd));
    }
    
    return future;
}

std::future<MemoryResult> MemoryAPI::searchPattern(const std::vector<uint8_t>& pattern) {
    auto cmd = std::make_unique<MemoryCommand>();
    cmd->type = MemoryCommandType::SEARCH_PATTERN;
    cmd->data = pattern;
    
    auto future = cmd->promise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        commandQueue.push(std::move(cmd));
    }
    
    return future;
}

std::future<MemoryResult> MemoryAPI::getRAMSnapshot() {
    auto cmd = std::make_unique<MemoryCommand>();
    cmd->type = MemoryCommandType::GET_RAM_SNAPSHOT;
    
    auto future = cmd->promise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        commandQueue.push(std::move(cmd));
    }
    
    return future;
}

void MemoryAPI::processCommands() {
    std::vector<std::unique_ptr<MemoryCommand>> commands;
    
    // Get all pending commands
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!commandQueue.empty()) {
            commands.push_back(std::move(commandQueue.front()));
            commandQueue.pop();
        }
    }
    
    // Process each command
    for (auto& cmd : commands) {
        MemoryResult result = executeCommand(*cmd);
        cmd->promise.set_value(std::move(result));
    }
}

bool MemoryAPI::isAddressValid(uint32_t address) const {
    return address <= 0xFFFF;  // NES has 16-bit address space
}

bool MemoryAPI::isWriteSafe(uint32_t address, uint32_t length) const {
    uint32_t end = address + length - 1;
    
    // RAM is always safe (0x0000-0x07FF and mirrors)
    if (address < 0x2000) return true;
    
    // SRAM/Work RAM (0x6000-0x7FFF) - check if battery-backed
    if (address >= 0x6000 && end <= 0x7FFF) {
        return GameInfo && GameInfo->battery;
    }
    
    // PPU/APU registers and ROM areas are not safe for general writes
    return false;
}

MemoryResult MemoryAPI::executeCommand(const MemoryCommand& cmd) {
    MemoryResult result;
    result.success = false;
    
    // Check if emulation is running
    if (!GameInfo) {
        result.error = "No game loaded";
        return result;
    }
    
    switch (cmd.type) {
        case MemoryCommandType::READ_BYTE: {
            if (!isAddressValid(cmd.address)) {
                result.error = "Invalid address";
                return result;
            }
            
            // Set debug flag to prevent side effects
            int oldDebug = fceuindbg;
            fceuindbg = 1;
            
            uint8_t value = FCEU_CheatGetByte(cmd.address);
            result.data = {value};
            result.success = true;
            
            fceuindbg = oldDebug;
            break;
        }
        
        case MemoryCommandType::WRITE_BYTE: {
            if (!isAddressValid(cmd.address)) {
                result.error = "Invalid address";
                return result;
            }
            
            if (!isWriteSafe(cmd.address, 1)) {
                result.error = "Address not writable";
                return result;
            }
            
            FCEU_CheatSetByte(cmd.address, cmd.data[0]);
            result.success = true;
            break;
        }
        
        case MemoryCommandType::READ_RANGE: {
            if (!isAddressValid(cmd.address) || 
                !isAddressValid(cmd.address + cmd.length - 1)) {
                result.error = "Invalid address range";
                return result;
            }
            
            // Set debug flag to prevent side effects
            int oldDebug = fceuindbg;
            fceuindbg = 1;
            
            result.data.reserve(cmd.length);
            for (uint32_t i = 0; i < cmd.length; i++) {
                result.data.push_back(FCEU_CheatGetByte(cmd.address + i));
            }
            result.success = true;
            
            fceuindbg = oldDebug;
            break;
        }
        
        case MemoryCommandType::WRITE_RANGE: {
            if (!isAddressValid(cmd.address) || 
                !isAddressValid(cmd.address + cmd.data.size() - 1)) {
                result.error = "Invalid address range";
                return result;
            }
            
            if (!isWriteSafe(cmd.address, cmd.data.size())) {
                result.error = "Address range not writable";
                return result;
            }
            
            for (size_t i = 0; i < cmd.data.size(); i++) {
                FCEU_CheatSetByte(cmd.address + i, cmd.data[i]);
            }
            result.success = true;
            break;
        }
        
        case MemoryCommandType::SEARCH_PATTERN: {
            if (cmd.data.empty()) {
                result.error = "Empty pattern";
                return result;
            }
            
            // Set debug flag to prevent side effects
            int oldDebug = fceuindbg;
            fceuindbg = 1;
            
            // Search in RAM (0x0000-0x07FF)
            for (uint32_t addr = 0; addr <= 0x07FF - cmd.data.size() + 1; addr++) {
                bool match = true;
                for (size_t i = 0; i < cmd.data.size(); i++) {
                    if (FCEU_CheatGetByte(addr + i) != cmd.data[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    // Return address where pattern was found
                    result.data.push_back(addr & 0xFF);
                    result.data.push_back((addr >> 8) & 0xFF);
                }
            }
            
            result.success = true;
            fceuindbg = oldDebug;
            break;
        }
        
        case MemoryCommandType::GET_RAM_SNAPSHOT: {
            // Set debug flag to prevent side effects
            int oldDebug = fceuindbg;
            fceuindbg = 1;
            
            // Get entire 2KB RAM
            result.data.reserve(0x800);
            for (uint32_t addr = 0; addr < 0x800; addr++) {
                result.data.push_back(FCEU_CheatGetByte(addr));
            }
            
            result.success = true;
            fceuindbg = oldDebug;
            break;
        }
    }
    
    return result;
}

} // namespace RestApi