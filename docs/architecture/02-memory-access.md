# FCEUX Memory Access Patterns & Thread Safety

## Overview
FCEUX implements a sophisticated memory mapping system that accurately emulates the NES's 16-bit address space. This document describes the canonical functions for memory access and their thread safety characteristics.

## Memory Architecture

### NES Memory Map
```
0x0000-0x07FF: 2KB RAM (mirrored to 0x0800-0x1FFF)
0x2000-0x2007: PPU registers (mirrored every 8 bytes to 0x3FFF)
0x4000-0x4017: APU and I/O registers
0x4018-0x5FFF: Cartridge expansion ROM (rarely used)
0x6000-0x7FFF: Cartridge SRAM/Work RAM
0x8000-0xFFFF: Cartridge PRG-ROM (32KB)
```

### Memory Access Arrays
FCEUX uses function pointer arrays for efficient memory access:
- `ARead[0x10000]` - Array of read function pointers
- `BWrite[0x10000]` - Array of write function pointers
- `AReadG[0x10000]` - Debug read handlers (side-effect free)
- `BWriteG[0x10000]` - Debug write handlers

## Canonical Memory Access Functions

### 1. Recommended: Cheat System Functions

```cpp
// src/cheat.h
uint8 FCEU_CheatGetByte(uint32 A);
void FCEU_CheatSetByte(uint32 A, uint8 V);
```

**Advantages:**
- Thread-safe when emulator mutex is held
- Proper bounds checking
- Respects memory mapping
- Used by Lua scripting and cheat system
- No side effects when `fceuindbg` is set

**Usage Example:**
```cpp
// Read from address 0x0300
uint8 value = FCEU_CheatGetByte(0x0300);

// Write to address 0x0301
FCEU_CheatSetByte(0x0301, 0x42);
```

### 2. Debug-Specific Function

```cpp
// src/debug.cpp
uint8 GetMem(uint16 A);
```

**Features:**
- Sets `fceuindbg` flag automatically
- Prevents side effects from hardware register reads
- Primarily for debugger use

### 3. Low-Level Direct Access (Not Recommended)

```cpp
// Direct array access
uint8 value = ARead[address](address);
BWrite[address](address, value);

// Direct RAM access (0x0000-0x07FF only)
uint8 value = RAM[address & 0x7FF];
```

**Warning:** Direct access bypasses safety checks and can cause side effects.

## Thread Safety Analysis

### Current State
- **NOT inherently thread-safe** - All memory access functions modify global state
- **Requires mutex protection** - Must hold emulator mutex during access

### Critical Global Variables
```cpp
extern uint8 *RAM;              // 2KB main RAM
extern readfunc ARead[0x10000]; // Read handlers
extern writefunc BWrite[0x10000]; // Write handlers
extern int fceuindbg;           // Debug mode flag
```

### Thread-Safe Access Pattern
```cpp
// From REST API handler thread
uint8 readMemorySafe(uint16 address) {
    uint8 value;
    
    FCEU_WRAPPER_LOCK();
    value = FCEU_CheatGetByte(address);
    FCEU_WRAPPER_UNLOCK();
    
    return value;
}

void writeMemorySafe(uint16 address, uint8 value) {
    FCEU_WRAPPER_LOCK();
    FCEU_CheatSetByte(address, value);
    FCEU_WRAPPER_UNLOCK();
}
```

## Memory Safety Guidelines

### Safe Memory Ranges

1. **RAM (0x0000-0x07FF)**
   - Always safe to read/write
   - Mirrored to 0x0800-0x1FFF

2. **Cartridge SRAM (0x6000-0x7FFF)**
   - Safe if battery-backed RAM exists
   - Check `GameInfo->battery` flag

3. **PPU Memory (via PPU functions)**
   - Use `FCEUPPU_PeekAddress()` for PPU memory
   - Addresses 0x0000-0x3FFF in PPU space

### Potentially Dangerous Ranges

1. **PPU Registers (0x2000-0x3FFF)**
   - Reads can affect PPU state
   - Use debug flag to prevent side effects

2. **APU/IO Registers (0x4000-0x4017)**
   - Controller reads at 0x4016/0x4017 consume data
   - Sound registers affect audio state

3. **Mapper Registers (varies)**
   - Writes to ROM areas may trigger mapper functions
   - Can switch banks or change memory mapping

## Best Practices for REST API

1. **Always use FCEU_CheatGetByte/SetByte**
   - Safest option for general use
   - Already battle-tested by Lua API

2. **Hold emulator mutex during access**
   - Prevents race conditions
   - Ensures consistent state

3. **Batch operations when possible**
   - Minimize mutex lock/unlock overhead
   - Read/write multiple bytes in one lock

4. **Set debug flag for read-only operations**
   ```cpp
   FCEU_WRAPPER_LOCK();
   int oldDebug = fceuindbg;
   fceuindbg = 1;
   // Perform reads without side effects
   fceuindbg = oldDebug;
   FCEU_WRAPPER_UNLOCK();
   ```

5. **Validate addresses before access**
   - Check against known safe ranges
   - Return error for invalid addresses

## Implementation Example

```cpp
class MemoryAPI {
public:
    struct MemoryRange {
        uint16_t start;
        uint16_t length;
        std::vector<uint8_t> data;
    };
    
    // Read memory range with validation
    bool readRange(uint16_t start, uint16_t length, MemoryRange& out) {
        if (start + length > 0x10000) return false;
        
        FCEU_WRAPPER_LOCK();
        out.start = start;
        out.length = length;
        out.data.reserve(length);
        
        for (uint16_t i = 0; i < length; i++) {
            out.data.push_back(FCEU_CheatGetByte(start + i));
        }
        FCEU_WRAPPER_UNLOCK();
        
        return true;
    }
    
    // Write with safety checks
    bool writeBytes(uint16_t start, const std::vector<uint8_t>& data) {
        // Validate safe write ranges
        if (!isWriteSafe(start, data.size())) return false;
        
        FCEU_WRAPPER_LOCK();
        for (size_t i = 0; i < data.size(); i++) {
            FCEU_CheatSetByte(start + i, data[i]);
        }
        FCEU_WRAPPER_UNLOCK();
        
        return true;
    }
    
private:
    bool isWriteSafe(uint16_t start, size_t length) {
        uint16_t end = start + length - 1;
        
        // RAM is always safe
        if (end <= 0x07FF) return true;
        
        // SRAM if battery present
        if (start >= 0x6000 && end <= 0x7FFF) {
            return GameInfo && GameInfo->battery;
        }
        
        return false;
    }
};
```

## Performance Considerations

- Memory access is fast (direct function pointer call)
- Mutex overhead is the primary bottleneck
- Batch operations to amortize locking cost
- Consider caching for frequently accessed ranges

## Testing Recommendations

1. **Unit tests with various mappers**
   - Different mappers have different memory layouts
   - Test boundary conditions

2. **Thread safety testing**
   - Concurrent reads/writes with emulation running
   - Verify no corruption or crashes

3. **Side effect validation**
   - Ensure debug flag prevents register side effects
   - Test PPU/APU register behavior