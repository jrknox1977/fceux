#ifndef __MEMORY_READ_COMMAND_H__
#define __MEMORY_READ_COMMAND_H__

#include "../RestApiCommands.h"
#include <string>
#include <cstdint>

/**
 * @brief Result structure for memory read operations
 * 
 * Contains the address and value read from NES memory,
 * with a method to convert to JSON format.
 */
struct MemoryReadResult {
    uint16_t address;  ///< Memory address that was read
    uint8_t value;     ///< Value read from the address
    
    /**
     * @brief Convert the result to JSON string
     * 
     * Returns a JSON object with the following fields:
     * - "address": Hex string with 0x prefix (e.g., "0x0300")
     * - "value": Hex string with 0x prefix (e.g., "0x42")
     * - "decimal": Decimal representation of value
     * - "binary": 8-bit binary string (e.g., "01000010")
     * 
     * @return JSON string representation
     */
    std::string toJson() const;
};

/**
 * @brief Command to read a byte from NES memory
 * 
 * This command safely reads memory using FCEU_CheatGetByte(),
 * which sets the fceuindbg flag to prevent side effects.
 * The command is thread-safe and checks that a game is loaded.
 * 
 * Example usage:
 * @code
 * auto cmd = std::make_shared<MemoryReadCommand>(0x0300);
 * commandQueue->enqueue(cmd);
 * auto future = cmd->getResult();
 * auto result = future.get(); // Blocks until complete
 * std::string json = result.toJson();
 * @endcode
 */
class MemoryReadCommand : public ApiCommandWithResult<MemoryReadResult> {
private:
    uint16_t address;  ///< Address to read from
    
public:
    /**
     * @brief Construct a memory read command
     * @param addr The 16-bit address to read from
     */
    explicit MemoryReadCommand(uint16_t addr);
    
    /**
     * @brief Execute the memory read operation
     * 
     * This method:
     * 1. Acquires the emulator mutex
     * 2. Checks that GameInfo is not null
     * 3. Reads the memory value using FCEU_CheatGetByte()
     * 4. Releases the mutex
     * 5. Sets the result promise
     * 
     * @throws std::runtime_error if no game is loaded
     */
    void execute() override;
    
    /**
     * @brief Get the command name for logging
     * @return "MemoryReadCommand"
     */
    const char* name() const override { return "MemoryReadCommand"; }
};

#endif // __MEMORY_READ_COMMAND_H__