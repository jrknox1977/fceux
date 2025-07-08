# Add Memory Range Read/Write REST API Endpoints

## Description
Currently, the REST API only supports reading single bytes from memory via `/api/memory/{address}`. The Lua scripting engine already has `memory.readbyterange()` functionality that can efficiently read multiple bytes in a single operation. We should expose similar functionality through the REST API to improve performance when reading/writing blocks of memory.

## Current State
- Single byte read: `GET /api/memory/{address}` (implemented)
- No bulk read/write endpoints available
- Lua has `memory.readbyterange(start, size)` which returns a string of bytes

## Proposed Endpoints

### 1. Read Memory Range
```
GET /api/memory/range/{start}/{length}
```

Response:
```json
{
  "start": "0x0000",
  "length": 256,
  "data": "base64_encoded_data",
  "hex": "00112233...",  // optional, first 64 bytes
  "checksum": "0x1234"   // simple checksum for validation
}
```

### 2. Write Memory Range  
```
POST /api/memory/range/{start}
```

Request body:
```json
{
  "data": "base64_encoded_data"
}
```

Response:
```json
{
  "success": true,
  "start": "0x0000", 
  "bytes_written": 256
}
```

### 3. Batch Memory Operations
```
POST /api/memory/batch
```

Request body:
```json
{
  "operations": [
    {"type": "read", "address": "0x0000", "length": 16},
    {"type": "write", "address": "0x0100", "data": "base64..."},
    {"type": "read", "address": "0x0200", "length": 32}
  ]
}
```

## Implementation Details

### Reference Lua Implementation
From `src/lua-engine.cpp`:
```cpp
static int memory_readbyterange(lua_State *L) {
    int range_start = luaL_checkinteger(L,1);
    int range_size = luaL_checkinteger(L,2);
    if(range_size < 0)
        return 0;

    char* buf = (char*)alloca(range_size);
    for(int i=0;i<range_size;i++) {
        buf[i] = GetMem(range_start+i);
    }
    
    lua_pushlstring(L,buf,range_size);
    return 1;
}
```

### Thread-Safe Implementation Pattern
Based on architecture docs (`docs/architecture/02-memory-access.md`):
```cpp
class MemoryRangeReadCommand : public ApiCommandWithResult<MemoryRangeResult> {
private:
    uint16_t startAddress;
    uint16_t length;
    
public:
    void execute() override {
        // Validate parameters
        if (startAddress + length > 0x10000) {
            throw std::runtime_error("Address range exceeds memory bounds");
        }
        
        MemoryRangeResult result;
        result.start = startAddress;
        result.length = length;
        
        FCEU_WRAPPER_LOCK();
        
        // Use FCEU_CheatGetByte for safe access
        for (uint16_t i = 0; i < length; i++) {
            result.data.push_back(FCEU_CheatGetByte(startAddress + i));
        }
        
        FCEU_WRAPPER_UNLOCK();
        
        resultPromise.set_value(result);
    }
};
```

## Benefits
1. **Performance**: Reduces API call overhead for bulk operations
2. **Atomicity**: Ensures consistent state when reading multiple bytes
3. **Efficiency**: Single mutex lock/unlock for entire range
4. **Parity**: Matches existing Lua scripting capabilities

## Validation & Safety
- Maximum range size limit (e.g., 4KB) to prevent excessive memory allocation
- Address bounds checking (0x0000-0xFFFF)
- Write operations restricted to safe ranges (RAM: 0x0000-0x07FF, SRAM: 0x6000-0x7FFF if battery-backed)
- Proper error responses for invalid ranges

## Testing Requirements
1. Read entire RAM (0x0000-0x07FF)
2. Write patterns and verify
3. Boundary testing (edge addresses)
4. Error cases (out of bounds, no game loaded)
5. Performance comparison vs. single byte reads
6. Concurrent access testing

## Future Enhancements
- PPU memory range access (using `FCEUPPU_PeekAddress()`)
- Memory watch/breakpoint notifications
- Compression options for large transfers
- Memory diff/compare operations