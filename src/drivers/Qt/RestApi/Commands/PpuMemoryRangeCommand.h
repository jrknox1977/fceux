#ifndef __PPU_MEMORY_RANGE_COMMAND_H__
#define __PPU_MEMORY_RANGE_COMMAND_H__

#include "../RestApiCommands.h"
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Structure for a single PPU memory value
 */
struct PpuMemoryValue {
    uint16_t address;  ///< PPU memory address
    uint8_t value;     ///< Value at that address
    uint8_t decimal;   ///< Same as value (for JSON consistency)
};

/**
 * @brief Result structure for PPU memory range read operations
 * 
 * Contains the starting address, values read, and region information
 * for a range of PPU memory addresses.
 */
struct PpuMemoryRangeResult {
    uint16_t start;                      ///< Starting PPU address
    uint16_t length;                     ///< Number of bytes read
    std::vector<PpuMemoryValue> values;  ///< Array of address/value pairs
    std::string region;                  ///< Primary PPU memory region
    std::string description;             ///< Human-readable description
    
    /**
     * @brief Convert the result to JSON string
     * 
     * Returns a JSON object with the following fields:
     * - "start": Starting address as hex string (e.g., "0x0000")
     * - "length": Number of bytes read
     * - "values": Array of objects with address, value (hex), and decimal
     * - "region": PPU memory region name
     * - "description": Human-readable region description
     * 
     * @return JSON string representation
     */
    std::string toJson() const;
};

/**
 * @brief Command to read a range of bytes from PPU memory
 * 
 * This command safely reads PPU memory using FFCEUX_PPURead() in a loop,
 * accessing multiple consecutive addresses in the PPU's memory space.
 * The command is thread-safe and validates the range before reading.
 * 
 * PPU Memory Map:
 * - 0x0000-0x1FFF: Pattern tables (CHR ROM/RAM)
 * - 0x2000-0x2FFF: Name tables
 * - 0x3000-0x3EFF: Mirror of 0x2000-0x2EFF
 * - 0x3F00-0x3FFF: Palette RAM
 * 
 * Example usage:
 * @code
 * auto cmd = std::make_shared<PpuMemoryRangeCommand>(0x0000, 256);
 * commandQueue->enqueue(cmd);
 * auto future = cmd->getResult();
 * auto result = future.get(); // Blocks until complete
 * std::string json = result.toJson();
 * @endcode
 */
class PpuMemoryRangeCommand : public ApiCommandWithResult<PpuMemoryRangeResult> {
private:
    uint16_t startAddress;  ///< Starting PPU address
    uint16_t length;        ///< Number of bytes to read
    
    static constexpr uint16_t MAX_LENGTH = 4096;  ///< Maximum bytes per request
    
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
     * @brief Construct a PPU memory range read command
     * @param start The starting PPU address (0x0000-0x3FFF)
     * @param len The number of bytes to read (max 4096)
     * @throws std::runtime_error if parameters are invalid
     */
    PpuMemoryRangeCommand(uint16_t start, uint16_t len);
    
    /**
     * @brief Execute the PPU memory range read operation
     * 
     * This method:
     * 1. Acquires the emulator mutex
     * 2. Checks that GameInfo is not null
     * 3. Checks that FFCEUX_PPURead is not null
     * 4. Reads values using FFCEUX_PPURead() in a loop
     * 5. Releases the mutex
     * 6. Sets the result promise with values and region info
     * 
     * @throws std::runtime_error if no game is loaded or PPU read function is unavailable
     */
    void execute() override;
    
    /**
     * @brief Get the command name for logging
     * @return "PpuMemoryRangeCommand"
     */
    const char* name() const override { return "PpuMemoryRangeCommand"; }
};

#endif // __PPU_MEMORY_RANGE_COMMAND_H__