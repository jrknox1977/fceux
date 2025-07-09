#ifndef __PPU_MEMORY_READ_COMMAND_H__
#define __PPU_MEMORY_READ_COMMAND_H__

#include "../RestApiCommands.h"
#include <string>
#include <cstdint>

/**
 * @brief Result structure for PPU memory read operations
 * 
 * Contains the address and value read from PPU memory,
 * along with region information and multiple value representations.
 */
struct PpuMemoryReadResult {
    uint16_t address;       ///< PPU memory address that was read
    uint8_t value;          ///< Value read from the address
    std::string region;     ///< PPU memory region (pattern_table, nametable, palette)
    std::string description;///< Human-readable description of the memory region
    
    /**
     * @brief Convert the result to JSON string
     * 
     * Returns a JSON object with the following fields:
     * - "address": Hex string with 0x prefix (e.g., "0x2000")
     * - "value": Hex string with 0x prefix (e.g., "0x42")
     * - "decimal": Decimal representation of value
     * - "binary": 8-bit binary string (e.g., "01000010")
     * - "region": PPU memory region name
     * - "description": Human-readable region description
     * 
     * @return JSON string representation
     */
    std::string toJson() const;
};

/**
 * @brief Command to read a byte from PPU memory
 * 
 * This command safely reads PPU memory using FFCEUX_PPURead(),
 * which accesses the PPU's internal memory space including pattern
 * tables, name tables, and palette RAM. The command is thread-safe
 * and checks that a game is loaded.
 * 
 * PPU Memory Map:
 * - 0x0000-0x1FFF: Pattern tables (CHR ROM/RAM)
 * - 0x2000-0x2FFF: Name tables
 * - 0x3000-0x3EFF: Mirror of 0x2000-0x2EFF
 * - 0x3F00-0x3FFF: Palette RAM
 * 
 * Example usage:
 * @code
 * auto cmd = std::make_shared<PpuMemoryReadCommand>(0x2000);
 * commandQueue->enqueue(cmd);
 * auto future = cmd->getResult();
 * auto result = future.get(); // Blocks until complete
 * std::string json = result.toJson();
 * @endcode
 */
class PpuMemoryReadCommand : public ApiCommandWithResult<PpuMemoryReadResult> {
private:
    uint16_t address;  ///< PPU address to read from (0x0000-0x3FFF)
    
    /**
     * @brief Get the PPU memory region name for an address
     * @param address PPU memory address
     * @return Region name (pattern_table, nametable, nametable_mirror, palette)
     */
    static std::string getPpuRegion(uint16_t address);
    
    /**
     * @brief Get a human-readable description of the PPU memory region
     * @param address PPU memory address
     * @return Description of the memory region
     */
    static std::string getPpuDescription(uint16_t address);
    
public:
    /**
     * @brief Construct a PPU memory read command
     * @param addr The 16-bit PPU address to read from (0x0000-0x3FFF)
     * @throws std::runtime_error if address is out of range
     */
    explicit PpuMemoryReadCommand(uint16_t addr);
    
    /**
     * @brief Execute the PPU memory read operation
     * 
     * This method:
     * 1. Acquires the emulator mutex
     * 2. Checks that GameInfo is not null
     * 3. Checks that FFCEUX_PPURead is not null
     * 4. Reads the memory value using FFCEUX_PPURead()
     * 5. Releases the mutex
     * 6. Sets the result promise with region information
     * 
     * @throws std::runtime_error if no game is loaded or PPU read function is unavailable
     */
    void execute() override;
    
    /**
     * @brief Get the command name for logging
     * @return "PpuMemoryReadCommand"
     */
    const char* name() const override { return "PpuMemoryReadCommand"; }
};

#endif // __PPU_MEMORY_READ_COMMAND_H__