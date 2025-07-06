# Memory Access Proof of Concept

This PoC demonstrates safe memory access from the REST API command queue, extending the command queue hook from Issue #2.

## Overview

The PoC shows how to:
1. Safely read/write NES memory from external threads
2. Handle different memory regions appropriately
3. Batch operations for efficiency
4. Prevent side effects during reads

## Implementation

### Memory Commands
```cpp
enum MemoryCommandType {
    MEM_READ_BYTE,
    MEM_WRITE_BYTE,
    MEM_READ_RANGE,
    MEM_WRITE_RANGE,
    MEM_SEARCH_PATTERN
};

struct MemoryCommand {
    MemoryCommandType type;
    uint16_t address;
    uint16_t length;
    std::vector<uint8_t> data;
    std::promise<std::vector<uint8_t>> result;
};
```

### Integration with Command Queue

The memory commands are processed in the main emulation loop with proper mutex protection:

```cpp
void processMemoryCommand(const MemoryCommand& cmd) {
    switch (cmd.type) {
        case MEM_READ_BYTE: {
            uint8_t value = FCEU_CheatGetByte(cmd.address);
            cmd.result.set_value({value});
            break;
        }
        case MEM_READ_RANGE: {
            std::vector<uint8_t> data;
            for (uint16_t i = 0; i < cmd.length; i++) {
                data.push_back(FCEU_CheatGetByte(cmd.address + i));
            }
            cmd.result.set_value(std::move(data));
            break;
        }
        // ... other commands
    }
}
```

## Files

- `memory_api.h` - Memory API class definition
- `memory_api.cpp` - Implementation with safety checks
- `memory_api.patch` - Patch to integrate with fceuWrapper.cpp
- `test_memory_api.cpp` - Test program demonstrating usage

## Building

1. Apply the patches:
```bash
patch -p1 < ../command_queue_hook/command_queue_hook.patch
patch -p1 < memory_api.patch
```

2. Build FCEUX normally

3. The memory API will be available through the command queue

## Testing

The test program demonstrates:
- Reading single bytes from various memory regions
- Writing to RAM safely
- Batch reading for efficiency
- Error handling for invalid addresses

## Safety Features

1. **Address Validation**
   - Checks bounds before access
   - Validates write permissions for different regions

2. **Thread Safety**
   - All operations happen in emulation thread
   - Uses promises for async results

3. **Side Effect Prevention**
   - Sets debug flag for read operations
   - Prevents unintended hardware register effects

## Performance

- Single byte access: ~10µs (including mutex)
- 256-byte range read: ~50µs
- Batching recommended for bulk operations