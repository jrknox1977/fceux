# FCEUX Memory Access Functions Analysis

## Overview
This document summarizes the memory access functions available in FCEUX for reading and writing NES memory safely.

## Core Memory Access Functions

### 1. **FCEU_CheatGetByte / FCEU_CheatSetByte** (Recommended)
Located in: `src/cheat.cpp` and `src/cheat.h`

```cpp
// Read a byte from NES memory
int FCEU_CheatGetByte(uint32 A)
{
    if(A < 0x10000) {
        uint32 ret;
        fceuindbg=1;
        ret = ARead[A](A);
        fceuindbg=0;
        return ret;
    } else
        return 0;
}

// Write a byte to NES memory
void FCEU_CheatSetByte(uint32 A, uint8 V)
{
   if(CheatRPtrs[A>>10])
    CheatRPtrs[A>>10][A]=V;
   else if(A < 0x10000)
    BWrite[A](A, V);
}
```

These functions:
- Handle memory access safely with bounds checking
- Use the proper read/write handlers (ARead/BWrite)
- Set the `fceuindbg` flag for proper debugging context
- Are used by the cheat system and Lua scripting

### 2. **GetMem** (Debug-specific)
Located in: `src/debug.cpp`

```cpp
uint8 GetMem(uint16 A)
```

This function:
- Provides special handling for PPU registers ($2000-$3FFF)
- Handles APU registers ($4000-$4017)
- Uses ARead for general memory access
- Is primarily used by the debugger

### 3. **Direct Memory Arrays**
Located in: `src/fceu.h` and `src/fceu.cpp`

```cpp
extern uint8 *RAM;                    // Main 2KB RAM at $0000-$07FF
extern readfunc ARead[0x10000];      // Read function pointers for each address
extern writefunc BWrite[0x10000];    // Write function pointers for each address
```

### 4. **Memory Handlers**
Located in: `src/fceu.h`

```cpp
void SetReadHandler(int32 start, int32 end, readfunc func);
void SetWriteHandler(int32 start, int32 end, writefunc func);
writefunc GetWriteHandler(int32 a);
readfunc GetReadHandler(int32 a);
```

## Memory Map Overview

- **$0000-$07FF**: Main RAM (2KB, mirrored to $1FFF)
- **$2000-$2007**: PPU registers (mirrored every 8 bytes to $3FFF)
- **$4000-$4017**: APU and I/O registers
- **$4018-$5FFF**: Cartridge expansion area
- **$6000-$7FFF**: Battery-backed save RAM (SRAM)
- **$8000-$FFFF**: Program ROM

## Recommended Usage

For safe memory access in new features:

1. **For general memory reading/writing**: Use `FCEU_CheatGetByte()` and `FCEU_CheatSetByte()`
   - They handle all the necessary safety checks
   - They work with the existing memory mapping system
   - They're already used by cheats and Lua scripting

2. **For debugger-specific features**: Use `GetMem()`
   - It has special handling for hardware registers
   - It's designed for debugging contexts

3. **For low-level access**: Use `ARead[addr](addr)` and `BWrite[addr](addr, value)`
   - Only when you need direct access to memory handlers
   - Remember to set `fceuindbg=1` before and `fceuindbg=0` after

## Example Usage

```cpp
// Read a byte from memory
uint8 value = FCEU_CheatGetByte(0x0042);

// Write a byte to memory
FCEU_CheatSetByte(0x0042, 0xFF);

// Read with debug context
fceuindbg = 1;
uint8 debugValue = ARead[0x2002](0x2002);  // Read PPU status
fceuindbg = 0;
```

## Notes

- The `fceuindbg` flag is important for preventing side effects during debugging
- Memory access through these functions respects mapper implementations
- The cheat system uses `CheatRPtrs` for RAM mapping optimization
- Always check address bounds when accessing memory directly