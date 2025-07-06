#ifndef MEMORY_API_H
#define MEMORY_API_H

#include <cstdint>
#include <vector>
#include <future>
#include <mutex>
#include <queue>

namespace RestApi {

// Memory access command types
enum class MemoryCommandType {
    READ_BYTE,
    WRITE_BYTE,
    READ_RANGE,
    WRITE_RANGE,
    SEARCH_PATTERN,
    GET_RAM_SNAPSHOT
};

// Memory access result
struct MemoryResult {
    bool success;
    std::string error;
    std::vector<uint8_t> data;
};

// Memory command structure
struct MemoryCommand {
    MemoryCommandType type;
    uint32_t address;
    uint32_t length;
    std::vector<uint8_t> data;
    std::promise<MemoryResult> promise;
};

// Thread-safe memory API
class MemoryAPI {
public:
    MemoryAPI();
    ~MemoryAPI();
    
    // Async memory operations (can be called from any thread)
    std::future<MemoryResult> readByte(uint32_t address);
    std::future<MemoryResult> writeByte(uint32_t address, uint8_t value);
    std::future<MemoryResult> readRange(uint32_t address, uint32_t length);
    std::future<MemoryResult> writeRange(uint32_t address, const std::vector<uint8_t>& data);
    std::future<MemoryResult> searchPattern(const std::vector<uint8_t>& pattern);
    std::future<MemoryResult> getRAMSnapshot();
    
    // Process commands (called from emulation thread)
    void processCommands();
    
private:
    std::queue<std::unique_ptr<MemoryCommand>> commandQueue;
    std::mutex queueMutex;
    
    // Helper functions
    bool isAddressValid(uint32_t address) const;
    bool isWriteSafe(uint32_t address, uint32_t length) const;
    MemoryResult executeCommand(const MemoryCommand& cmd);
};

// Global instance (initialized in fceuWrapper.cpp)
extern MemoryAPI* g_memoryAPI;

} // namespace RestApi

#endif // MEMORY_API_H