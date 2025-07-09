#include "PpuMemoryRangeCommand.h"
#include "../../../../lib/json.hpp"
#include "../../fceuWrapper.h"
#include "../../../../fceu.h"
#include "../../../../ppu.h"
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

std::string PpuMemoryRangeResult::toJson() const {
    json result;
    
    // Format start address as hex
    std::stringstream startStream;
    startStream << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << start;
    
    result["start"] = startStream.str();
    result["length"] = length;
    
    // Build values array
    json valuesArray = json::array();
    for (const auto& val : values) {
        json valueObj;
        
        // Format address and value as hex
        std::stringstream addrStream, valStream;
        addrStream << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << val.address;
        valStream << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (int)val.value;
        
        valueObj["address"] = addrStream.str();
        valueObj["value"] = valStream.str();
        valueObj["decimal"] = val.decimal;
        
        valuesArray.push_back(valueObj);
    }
    
    result["values"] = valuesArray;
    result["region"] = region;
    result["description"] = description;
    
    return result.dump();
}

std::string PpuMemoryRangeCommand::getPpuRegion(uint16_t address) {
    if (address < 0x2000) return "pattern_table";
    else if (address < 0x3000) return "nametable";
    else if (address < 0x3F00) return "nametable_mirror";
    else return "palette";
}

std::string PpuMemoryRangeCommand::getPpuDescription(uint16_t address) {
    if (address < 0x1000) return "Pattern Table 0";
    else if (address < 0x2000) return "Pattern Table 1";
    else if (address < 0x2400) return "Name Table 0";
    else if (address < 0x2800) return "Name Table 1";
    else if (address < 0x2C00) return "Name Table 2";
    else if (address < 0x3000) return "Name Table 3";
    else if (address < 0x3F00) return "Name Table Mirror";
    else if (address < 0x3F20) return "Palette RAM";
    else return "Palette Mirror";
}

PpuMemoryRangeCommand::PpuMemoryRangeCommand(uint16_t start, uint16_t len) 
    : startAddress(start), length(len) {
    
    // Validate start address
    if (start > 0x3FFF) {
        throw std::runtime_error("PPU start address out of range. Valid range: 0x0000-0x3FFF");
    }
    
    // Validate length
    if (len == 0) {
        throw std::runtime_error("Length must be greater than 0");
    }
    
    if (len > MAX_LENGTH) {
        throw std::runtime_error("Length exceeds maximum of 4096 bytes");
    }
    
    // Check if range would exceed PPU memory bounds
    if (start + len > 0x4000) {
        throw std::runtime_error("Range exceeds PPU memory bounds (0x0000-0x3FFF)");
    }
}

void PpuMemoryRangeCommand::execute() {
    FCEU_WRAPPER_LOCK();
    
    try {
        // Check if a game is loaded
        if (!GameInfo) {
            FCEU_WRAPPER_UNLOCK();
            throw std::runtime_error("No game loaded");
        }
        
        // Check if PPU read function is available
        if (!FFCEUX_PPURead) {
            FCEU_WRAPPER_UNLOCK();
            throw std::runtime_error("PPU read function not available");
        }
        
        // Create result structure
        PpuMemoryRangeResult result;
        result.start = startAddress;
        result.length = length;
        result.region = getPpuRegion(startAddress);
        result.description = getPpuDescription(startAddress);
        
        // Reserve space for efficiency
        result.values.reserve(length);
        
        // Read the PPU memory values
        for (uint16_t i = 0; i < length; i++) {
            uint16_t addr = startAddress + i;
            uint8_t value = FFCEUX_PPURead(addr);
            
            PpuMemoryValue memVal;
            memVal.address = addr;
            memVal.value = value;
            memVal.decimal = value;
            
            result.values.push_back(memVal);
        }
        
        FCEU_WRAPPER_UNLOCK();
        
        // Set the result
        resultPromise.set_value(result);
        
    } catch (...) {
        FCEU_WRAPPER_UNLOCK();
        throw;
    }
}