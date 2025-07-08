#ifndef __MEMORY_RANGE_COMMANDS_H__
#define __MEMORY_RANGE_COMMANDS_H__

#include "../RestApiCommands.h"
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Maximum allowed length for memory range operations
 * 
 * Prevents excessive memory allocation and ensures reasonable response times
 */
const uint16_t MAX_MEMORY_RANGE_LENGTH = 4096;

/**
 * @brief Result structure for memory range read operations
 * 
 * Contains the memory range data with base64 encoding, hex preview,
 * and checksum for validation.
 */
struct MemoryRangeResult {
    uint16_t start;              ///< Starting address
    uint16_t length;             ///< Number of bytes read
    std::vector<uint8_t> data;   ///< Raw memory data
    
    /**
     * @brief Convert the result to JSON string
     * 
     * Returns a JSON object with:
     * - "start": Hex string with 0x prefix (e.g., "0x0000")
     * - "length": Decimal number of bytes
     * - "data": Base64 encoded memory data
     * - "hex": Hex string of first 64 bytes (or less if shorter)
     * - "checksum": XOR checksum of all bytes as hex string
     * 
     * @return JSON string representation
     */
    std::string toJson() const;
};

/**
 * @brief Result structure for memory write operations
 * 
 * Contains success status and details about the write operation.
 */
struct MemoryWriteResult {
    bool success;           ///< Whether the write succeeded
    uint16_t start;         ///< Starting address
    uint16_t bytesWritten;  ///< Number of bytes written
    std::string error;      ///< Error message if failed
    
    /**
     * @brief Convert the result to JSON string
     * @return JSON string representation
     */
    std::string toJson() const;
};

/**
 * @brief Single operation in a batch request
 */
struct BatchOperation {
    std::string type;            ///< "read" or "write"
    uint16_t address;            ///< Memory address
    uint16_t length;             ///< Length for read operations
    std::vector<uint8_t> data;   ///< Data for write operations
};

/**
 * @brief Result for a single batch operation
 */
struct BatchOperationResult {
    std::string type;            ///< "read" or "write"
    bool success;                ///< Whether the operation succeeded
    uint16_t address;            ///< Memory address
    std::vector<uint8_t> data;   ///< Data read (for read operations)
    uint16_t bytesWritten;       ///< Bytes written (for write operations)
    std::string error;           ///< Error message if failed
    
    /**
     * @brief Convert to JSON object representation
     * @return JSON string for this operation result
     */
    std::string toJson() const;
};

/**
 * @brief Result structure for batch memory operations
 */
struct MemoryBatchResult {
    std::vector<BatchOperationResult> results;  ///< Results for each operation
    
    /**
     * @brief Convert the result to JSON string
     * @return JSON string representation
     */
    std::string toJson() const;
};

/**
 * @brief Command to read a range of bytes from NES memory
 * 
 * This command safely reads memory using FCEU_CheatGetByte() in a loop,
 * which sets the fceuindbg flag to prevent side effects.
 * The command is thread-safe and checks that a game is loaded.
 * 
 * Maximum range length is limited to MAX_MEMORY_RANGE_LENGTH (4096 bytes).
 */
class MemoryRangeReadCommand : public ApiCommandWithResult<MemoryRangeResult> {
private:
    uint16_t startAddress;  ///< Starting address to read from
    uint16_t length;        ///< Number of bytes to read
    
public:
    /**
     * @brief Construct a memory range read command
     * @param start The starting 16-bit address
     * @param len The number of bytes to read
     */
    MemoryRangeReadCommand(uint16_t start, uint16_t len);
    
    /**
     * @brief Execute the memory range read operation
     * 
     * @throws std::runtime_error if no game loaded, invalid range, or length > 4096
     */
    void execute() override;
    
    /**
     * @brief Get the command name for logging
     * @return "MemoryRangeReadCommand"
     */
    const char* name() const override { return "MemoryRangeReadCommand"; }
};

/**
 * @brief Command to write a range of bytes to NES memory
 * 
 * This command validates write safety before modifying memory.
 * Only allows writes to:
 * - RAM (0x0000-0x07FF)
 * - Battery-backed SRAM (0x6000-0x7FFF) if available
 */
class MemoryRangeWriteCommand : public ApiCommandWithResult<MemoryWriteResult> {
private:
    uint16_t startAddress;       ///< Starting address to write to
    std::vector<uint8_t> data;   ///< Data to write
    
public:
    /**
     * @brief Check if a memory range is safe to write
     * @param start Starting address
     * @param length Number of bytes
     * @return true if safe to write, false otherwise
     */
    bool isWriteSafe(uint16_t start, uint16_t length) const;
    /**
     * @brief Construct a memory range write command
     * @param start The starting 16-bit address
     * @param writeData The data to write
     */
    MemoryRangeWriteCommand(uint16_t start, const std::vector<uint8_t>& writeData);
    
    /**
     * @brief Execute the memory range write operation
     * 
     * @throws std::runtime_error if no game loaded or write unsafe
     */
    void execute() override;
    
    /**
     * @brief Get the command name for logging
     * @return "MemoryRangeWriteCommand"
     */
    const char* name() const override { return "MemoryRangeWriteCommand"; }
};

/**
 * @brief Command to execute multiple memory operations atomically
 * 
 * Executes a batch of read and write operations in sequence.
 * All operations are executed within a single mutex lock for atomicity.
 * Maximum 100 operations per batch.
 */
class MemoryBatchCommand : public ApiCommandWithResult<MemoryBatchResult> {
private:
    std::vector<BatchOperation> operations;  ///< Operations to execute
    
    /**
     * @brief Execute a single read operation
     * @param op The operation to execute
     * @return Result of the operation
     */
    BatchOperationResult executeRead(const BatchOperation& op);
    
    /**
     * @brief Execute a single write operation
     * @param op The operation to execute
     * @return Result of the operation
     */
    BatchOperationResult executeWrite(const BatchOperation& op);
    
public:
    /**
     * @brief Construct a memory batch command
     * @param ops The operations to execute
     */
    explicit MemoryBatchCommand(const std::vector<BatchOperation>& ops);
    
    /**
     * @brief Execute all batch operations
     * 
     * @throws std::runtime_error if no game loaded or > 100 operations
     */
    void execute() override;
    
    /**
     * @brief Get the command name for logging
     * @return "MemoryBatchCommand"
     */
    const char* name() const override { return "MemoryBatchCommand"; }
};

#endif // __MEMORY_RANGE_COMMANDS_H__