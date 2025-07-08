#include "MemoryReadCommand.h"
#include "../../fceuWrapper.h"
#include "../../../../cheat.h"
#include "../../../../fceu.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>

// MemoryReadResult implementation

std::string MemoryReadResult::toJson() const {
    std::ostringstream json;
    json << "{";
    
    // Address as hex with 0x prefix (4 digits)
    json << "\"address\":\"0x" 
         << std::hex << std::setfill('0') << std::setw(4) 
         << address << "\",";
    
    // Value as hex with 0x prefix (2 digits)
    json << "\"value\":\"0x" 
         << std::hex << std::setfill('0') << std::setw(2) 
         << static_cast<int>(value) << "\",";
    
    // Decimal representation
    json << "\"decimal\":" << std::dec << static_cast<int>(value) << ",";
    
    // Binary representation (8 bits)
    json << "\"binary\":\"";
    for (int i = 7; i >= 0; i--) {
        json << ((value >> i) & 1);
    }
    json << "\"";
    
    json << "}";
    return json.str();
}

// MemoryReadCommand implementation

MemoryReadCommand::MemoryReadCommand(uint16_t addr) : address(addr) {
}

void MemoryReadCommand::execute() {
    // Acquire emulator mutex for thread safety
    FCEU_WRAPPER_LOCK();
    
    // Check if a game is loaded
    if (GameInfo == nullptr) {
        // Release mutex before throwing
        FCEU_WRAPPER_UNLOCK();
        throw std::runtime_error("No game loaded");
    }
    
    // Prepare result structure
    MemoryReadResult result;
    result.address = address;
    
    // Read memory using the safe function
    // FCEU_CheatGetByte automatically:
    // - Sets fceuindbg flag to prevent side effects
    // - Handles bounds checking (returns 0 for addresses > 0xFFFF)
    // - Uses proper memory handlers
    result.value = FCEU_CheatGetByte(address);
    
    // Release mutex
    FCEU_WRAPPER_UNLOCK();
    
    // Set the promise with the result
    // This will make the future ready for the REST endpoint
    resultPromise.set_value(result);
}