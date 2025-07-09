#include "PpuMemoryReadCommand.h"
#include "../../../../lib/json.hpp"
#include "../../fceuWrapper.h"
#include "../../../../fceu.h"
#include "../../../../ppu.h"
#include <sstream>
#include <iomanip>
#include <bitset>

using json = nlohmann::json;

std::string PpuMemoryReadResult::toJson() const {
    json result;
    
    // Format address and value as hex with 0x prefix
    std::stringstream addrStream, valStream;
    addrStream << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << address;
    valStream << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (int)value;
    
    result["address"] = addrStream.str();
    result["value"] = valStream.str();
    result["decimal"] = value;
    result["binary"] = std::bitset<8>(value).to_string();
    result["region"] = region;
    result["description"] = description;
    
    return result.dump();
}

std::string PpuMemoryReadCommand::getPpuRegion(uint16_t address) {
    if (address < 0x2000) return "pattern_table";
    else if (address < 0x3000) return "nametable";
    else if (address < 0x3F00) return "nametable_mirror";
    else return "palette";
}

std::string PpuMemoryReadCommand::getPpuDescription(uint16_t address) {
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

PpuMemoryReadCommand::PpuMemoryReadCommand(uint16_t addr) : address(addr) {
    if (addr > 0x3FFF) {
        throw std::runtime_error("PPU address out of range. Valid range: 0x0000-0x3FFF");
    }
}

void PpuMemoryReadCommand::execute() {
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
        
        // Read the PPU memory value
        uint8_t value = FFCEUX_PPURead(address);
        
        FCEU_WRAPPER_UNLOCK();
        
        // Create and set the result
        PpuMemoryReadResult result;
        result.address = address;
        result.value = value;
        result.region = getPpuRegion(address);
        result.description = getPpuDescription(address);
        
        resultPromise.set_value(result);
        
    } catch (...) {
        FCEU_WRAPPER_UNLOCK();
        throw;
    }
}