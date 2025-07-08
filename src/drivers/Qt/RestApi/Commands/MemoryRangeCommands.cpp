#include "MemoryRangeCommands.h"
#include "../../fceuWrapper.h"
#include "../../../../cheat.h"
#include "../../../../fceu.h"
#include <QByteArray>
#include <sstream>
#include <iomanip>
#include <stdexcept>

// MemoryRangeResult implementation

std::string MemoryRangeResult::toJson() const {
    std::ostringstream json;
    json << "{";
    
    // Starting address as hex
    json << "\"start\":\"0x" 
         << std::hex << std::setfill('0') << std::setw(4) 
         << start << "\",";
    
    // Length as decimal
    json << "\"length\":" << std::dec << length << ",";
    
    // Base64 encoded data
    if (!data.empty()) {
        QByteArray byteArray(reinterpret_cast<const char*>(data.data()), data.size());
        std::string base64 = byteArray.toBase64().toStdString();
        json << "\"data\":\"" << base64 << "\",";
        
        // Hex preview (first 64 bytes or less)
        json << "\"hex\":\"";
        size_t hexLength = std::min(static_cast<size_t>(64), data.size());
        for (size_t i = 0; i < hexLength; i++) {
            json << std::hex << std::setfill('0') << std::setw(2) 
                 << static_cast<int>(data[i]);
        }
        if (data.size() > 64) {
            json << "...";
        }
        json << "\",";
        
        // XOR checksum
        uint8_t checksum = 0;
        for (uint8_t byte : data) {
            checksum ^= byte;
        }
        json << "\"checksum\":\"0x" 
             << std::hex << std::setfill('0') << std::setw(2) 
             << static_cast<int>(checksum) << "\"";
    } else {
        json << "\"data\":\"\",";
        json << "\"hex\":\"\",";
        json << "\"checksum\":\"0x00\"";
    }
    
    json << "}";
    return json.str();
}

// MemoryWriteResult implementation

std::string MemoryWriteResult::toJson() const {
    std::ostringstream json;
    json << "{";
    
    json << "\"success\":" << (success ? "true" : "false") << ",";
    
    // Starting address as hex
    json << "\"start\":\"0x" 
         << std::hex << std::setfill('0') << std::setw(4) 
         << start << "\",";
    
    // Bytes written as decimal
    json << "\"bytes_written\":" << std::dec << bytesWritten;
    
    // Error message if failed
    if (!error.empty()) {
        json << ",\"error\":\"" << error << "\"";
    }
    
    json << "}";
    return json.str();
}

// BatchOperationResult implementation

std::string BatchOperationResult::toJson() const {
    std::ostringstream json;
    json << "{";
    
    json << "\"type\":\"" << type << "\",";
    json << "\"success\":" << (success ? "true" : "false") << ",";
    json << "\"address\":\"0x" 
         << std::hex << std::setfill('0') << std::setw(4) 
         << address << "\"";
    
    if (type == "read" && success && !data.empty()) {
        QByteArray byteArray(reinterpret_cast<const char*>(data.data()), data.size());
        std::string base64 = byteArray.toBase64().toStdString();
        json << ",\"data\":\"" << base64 << "\"";
    } else if (type == "write" && success) {
        json << ",\"bytes_written\":" << std::dec << bytesWritten;
    }
    
    if (!error.empty()) {
        json << ",\"error\":\"" << error << "\"";
    }
    
    json << "}";
    return json.str();
}

// MemoryBatchResult implementation

std::string MemoryBatchResult::toJson() const {
    std::ostringstream json;
    json << "{\"results\":[";
    
    for (size_t i = 0; i < results.size(); i++) {
        if (i > 0) json << ",";
        json << results[i].toJson();
    }
    
    json << "]}";
    return json.str();
}

// MemoryRangeReadCommand implementation

MemoryRangeReadCommand::MemoryRangeReadCommand(uint16_t start, uint16_t len)
    : startAddress(start), length(len) {
}

void MemoryRangeReadCommand::execute() {
    // Validate length
    if (length == 0) {
        throw std::runtime_error("Length must be greater than 0");
    }
    if (length > MAX_MEMORY_RANGE_LENGTH) {
        throw std::runtime_error("Length exceeds maximum allowed (4096 bytes)");
    }
    
    // Check for address range overflow
    uint32_t endAddress = static_cast<uint32_t>(startAddress) + length;
    if (endAddress > 0x10000) {
        throw std::runtime_error("Address range exceeds memory bounds");
    }
    
    // Acquire emulator mutex
    FCEU_WRAPPER_LOCK();
    
    // Check if game is loaded
    if (GameInfo == nullptr) {
        FCEU_WRAPPER_UNLOCK();
        throw std::runtime_error("No game loaded");
    }
    
    // Prepare result
    MemoryRangeResult result;
    result.start = startAddress;
    result.length = length;
    result.data.reserve(length);
    
    // Read memory efficiently
    for (uint16_t i = 0; i < length; i++) {
        uint8_t value = FCEU_CheatGetByte(startAddress + i);
        result.data.push_back(value);
    }
    
    // Release mutex
    FCEU_WRAPPER_UNLOCK();
    
    // Set the result
    resultPromise.set_value(result);
}

// MemoryRangeWriteCommand implementation

MemoryRangeWriteCommand::MemoryRangeWriteCommand(uint16_t start, const std::vector<uint8_t>& writeData)
    : startAddress(start), data(writeData) {
}

bool MemoryRangeWriteCommand::isWriteSafe(uint16_t start, uint16_t length) const {
    // Check for overflow
    uint32_t end = static_cast<uint32_t>(start) + length;
    if (end > 0x10000) return false;
    
    uint16_t lastAddress = start + length - 1;
    
    // RAM is always safe (0x0000-0x07FF)
    if (lastAddress <= 0x07FF) return true;
    
    // TODO: Add SRAM support (0x6000-0x7FFF) once we determine how to check if battery-backed
    // For now, only allow RAM writes for safety
    
    return false;
}

void MemoryRangeWriteCommand::execute() {
    // Validate data
    if (data.empty()) {
        throw std::runtime_error("No data to write");
    }
    if (data.size() > MAX_MEMORY_RANGE_LENGTH) {
        throw std::runtime_error("Data size exceeds maximum allowed (4096 bytes)");
    }
    
    // Acquire emulator mutex
    FCEU_WRAPPER_LOCK();
    
    // Check if game is loaded
    if (GameInfo == nullptr) {
        FCEU_WRAPPER_UNLOCK();
        throw std::runtime_error("No game loaded");
    }
    
    // Validate write safety
    if (!isWriteSafe(startAddress, data.size())) {
        FCEU_WRAPPER_UNLOCK();
        throw std::runtime_error("Memory range is not safe to write");
    }
    
    // Prepare result
    MemoryWriteResult result;
    result.start = startAddress;
    result.bytesWritten = 0;
    result.success = true;
    
    // Write memory
    for (size_t i = 0; i < data.size(); i++) {
        FCEU_CheatSetByte(startAddress + i, data[i]);
        result.bytesWritten++;
    }
    
    // Release mutex
    FCEU_WRAPPER_UNLOCK();
    
    // Set the result
    resultPromise.set_value(result);
}

// MemoryBatchCommand implementation

MemoryBatchCommand::MemoryBatchCommand(const std::vector<BatchOperation>& ops)
    : operations(ops) {
}

BatchOperationResult MemoryBatchCommand::executeRead(const BatchOperation& op) {
    BatchOperationResult result;
    result.type = "read";
    result.address = op.address;
    
    try {
        // Validate length
        if (op.length == 0) {
            throw std::runtime_error("Length must be greater than 0");
        }
        if (op.length > MAX_MEMORY_RANGE_LENGTH) {
            throw std::runtime_error("Length exceeds maximum allowed");
        }
        
        // Check bounds
        uint32_t endAddress = static_cast<uint32_t>(op.address) + op.length;
        if (endAddress > 0x10000) {
            throw std::runtime_error("Address range exceeds memory bounds");
        }
        
        // Read memory
        result.data.reserve(op.length);
        for (uint16_t i = 0; i < op.length; i++) {
            uint8_t value = FCEU_CheatGetByte(op.address + i);
            result.data.push_back(value);
        }
        
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
    }
    
    return result;
}

BatchOperationResult MemoryBatchCommand::executeWrite(const BatchOperation& op) {
    BatchOperationResult result;
    result.type = "write";
    result.address = op.address;
    result.bytesWritten = 0;
    
    try {
        // Create temporary write command to reuse validation
        MemoryRangeWriteCommand writeCmd(op.address, op.data);
        
        // Validate
        if (op.data.empty()) {
            throw std::runtime_error("No data to write");
        }
        if (op.data.size() > MAX_MEMORY_RANGE_LENGTH) {
            throw std::runtime_error("Data size exceeds maximum allowed");
        }
        if (!writeCmd.isWriteSafe(op.address, op.data.size())) {
            throw std::runtime_error("Memory range is not safe to write");
        }
        
        // Write memory
        for (size_t i = 0; i < op.data.size(); i++) {
            FCEU_CheatSetByte(op.address + i, op.data[i]);
            result.bytesWritten++;
        }
        
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
    }
    
    return result;
}

void MemoryBatchCommand::execute() {
    // Validate operation count
    if (operations.empty()) {
        throw std::runtime_error("No operations provided");
    }
    if (operations.size() > 100) {
        throw std::runtime_error("Too many operations (maximum 100)");
    }
    
    // Acquire emulator mutex
    FCEU_WRAPPER_LOCK();
    
    // Check if game is loaded
    if (GameInfo == nullptr) {
        FCEU_WRAPPER_UNLOCK();
        throw std::runtime_error("No game loaded");
    }
    
    // Prepare result
    MemoryBatchResult result;
    result.results.reserve(operations.size());
    
    // Execute all operations
    for (const auto& op : operations) {
        if (op.type == "read") {
            result.results.push_back(executeRead(op));
        } else if (op.type == "write") {
            result.results.push_back(executeWrite(op));
        } else {
            BatchOperationResult opResult;
            opResult.type = op.type;
            opResult.address = op.address;
            opResult.success = false;
            opResult.error = "Unknown operation type";
            result.results.push_back(opResult);
        }
    }
    
    // Release mutex
    FCEU_WRAPPER_UNLOCK();
    
    // Set the result
    resultPromise.set_value(result);
}