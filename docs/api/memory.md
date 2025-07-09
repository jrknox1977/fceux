# Memory Access Endpoints

Memory access endpoints provide safe read and write operations on NES memory with validation and performance optimizations.

## GET /api/memory/{address}

**Description**: Read a single byte from NES memory

**Parameters**:
- `address` (path): Memory address in hex (0x0000-0xFFFF) or decimal (0-65535)

**Request Examples**:
```bash
# Hex address format
curl -X GET http://localhost:8080/api/memory/0x0000

# Decimal address format  
curl -X GET http://localhost:8080/api/memory/1024
```

**Response**:
```json
{
  "address": "0x0000",
  "value": 255,
  "hex": "0xFF"
}
```

**Response Fields**:
- `address`: Address in hex format with 0x prefix
- `value`: Byte value as decimal number (0-255)
- `hex`: Byte value as hex string with 0x prefix

**Status Codes**:
- `200 OK`: Memory read successful
- `400 Bad Request`: Invalid address format or out of range
- `503 Service Unavailable`: No game loaded
- `504 Gateway Timeout`: Command execution timeout

**Error Responses**:
```json
// Invalid address
{
  "error": "Invalid hex format"
}

// No ROM loaded
{
  "error": "No game loaded"
}
```

**Notes**:
- Uses safe `FCEU_CheatGetByte()` function to prevent side effects
- 1-second timeout for command execution
- Address range validated (0x0000-0xFFFF)

---

## GET /api/memory/range/{start}/{length}

**Description**: Read multiple bytes from NES memory efficiently (up to 4096 bytes)

**Parameters**:
- `start` (path): Starting address in hex (0x0000-0xFFFF) or decimal
- `length` (path): Number of bytes to read (1-4096)

**Request Examples**:
```bash
# Read 256 bytes from start of RAM
curl -X GET http://localhost:8080/api/memory/range/0x0000/256

# Read 16 bytes from sprite RAM
curl -X GET http://localhost:8080/api/memory/range/0x0200/16
```

**Response**:
```json
{
  "start": "0x0000",
  "length": 256,
  "data": "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=",
  "hex": "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
  "checksum": "0x20"
}
```

**Response Fields**:
- `start`: Starting address in hex format
- `length`: Number of bytes read
- `data`: Base64-encoded memory data
- `hex`: First 64 bytes as hex string (for preview)
- `checksum`: XOR checksum of all bytes as hex value

**Performance Benefits**:
- ~332x faster than individual byte reads
- Single mutex lock for entire range
- Atomic operation ensures consistent state

**Status Codes**:
- `200 OK`: Memory range read successful
- `400 Bad Request`: Invalid parameters or length exceeds maximum
- `503 Service Unavailable`: No game loaded
- `504 Gateway Timeout`: Command execution timeout

**Error Responses**:
```json
// Length too large
{
  "error": "Length exceeds maximum of 4096 bytes"
}

// Address range invalid
{
  "error": "Address range exceeds memory bounds"
}
```

**Notes**:
- Maximum 4096 bytes per request
- 2-second timeout for larger reads
- Range validated to not exceed 0xFFFF

---

## POST /api/memory/range/{start}

**Description**: Write multiple bytes to NES memory with safety validation

**Parameters**:
- `start` (path): Starting address in hex (0x0000-0xFFFF) or decimal

**Request Body**:
```json
{
  "data": "AAECAwQFBgcICQoLDA0ODw=="
}
```

**Request Example**:
```bash
curl -X POST http://localhost:8080/api/memory/range/0x0000 \
  -H "Content-Type: application/json" \
  -d '{"data": "AAECAwQFBgcICQoLDA0ODw=="}'
```

**Response**:
```json
{
  "success": true,
  "start": "0x0000",
  "bytes_written": 16
}
```

**Response Fields**:
- `success`: Always true for successful writes
- `start`: Starting address in hex format
- `bytes_written`: Number of bytes actually written

**Write Safety Restrictions**:
- **Allowed**: RAM region (0x0000-0x07FF) - Always safe
- **Forbidden**: All other regions for safety

Future versions may allow:
- Battery-backed SRAM (0x6000-0x7FFF) when available
- Configurable safety levels

**Status Codes**:
- `200 OK`: Memory write successful
- `400 Bad Request`: Invalid parameters, unsafe write range, or malformed JSON
- `503 Service Unavailable`: No game loaded
- `504 Gateway Timeout`: Command execution timeout

**Error Responses**:
```json
// Unsafe write location
{
  "error": "Write to address 0x8000 not allowed - outside safe range"
}

// Invalid base64 data
{
  "error": "Invalid base64 encoding"
}

// Missing data field
{
  "error": "Missing or invalid 'data' field"
}
```

**Notes**:
- Data must be valid base64 encoding
- Maximum 4096 bytes per write
- 2-second timeout for large writes
- Write safety enforced for emulator stability

---

## POST /api/memory/batch

**Description**: Execute multiple memory operations atomically

**Request Body**:
```json
{
  "operations": [
    {
      "type": "read",
      "address": "0x0000",
      "length": 16
    },
    {
      "type": "write", 
      "address": "0x0100",
      "data": "AAECAwQFBgcICQoLDA0ODw=="
    },
    {
      "type": "read",
      "address": "0x0200", 
      "length": 32
    }
  ]
}
```

**Request Example**:
```bash
curl -X POST http://localhost:8080/api/memory/batch \
  -H "Content-Type: application/json" \
  -d '{
    "operations": [
      {"type": "read", "address": "0x0000", "length": 16},
      {"type": "write", "address": "0x0100", "data": "AAECAwQFBgc="}
    ]
  }'
```

**Response**:
```json
{
  "results": [
    {
      "type": "read",
      "success": true,
      "address": "0x0000",
      "data": "AAECAwQFBgcICQoLDA0ODw==",
      "error": ""
    },
    {
      "type": "write",
      "success": true, 
      "address": "0x0100",
      "bytes_written": 8,
      "error": ""
    }
  ]
}
```

**Operation Types**:

### Read Operation
```json
{
  "type": "read",
  "address": "0x0000",
  "length": 16
}
```

### Write Operation
```json
{
  "type": "write",
  "address": "0x0100", 
  "data": "base64_encoded_data"
}
```

**Result Fields**:
- `type`: Operation type ("read" or "write")
- `success`: true if operation succeeded
- `address`: Memory address operated on
- `data`: Base64 data (read operations only)
- `bytes_written`: Number of bytes written (write operations only)
- `error`: Error message if operation failed

**Status Codes**:
- `200 OK`: All operations completed (check individual success flags)
- `400 Bad Request`: Invalid JSON or operation parameters
- `503 Service Unavailable`: No game loaded
- `504 Gateway Timeout`: Command execution timeout

**Notes**:
- All operations execute atomically under single mutex lock
- Individual operations can fail while others succeed
- Maximum 100 operations per batch
- 5-second timeout for complex batches
- Same safety restrictions apply to write operations

## Memory Map Reference

### NES Memory Layout
```
0x0000-0x07FF  RAM (2KB, mirrored to 0x1FFF)
0x0800-0x0FFF  RAM mirror 1
0x1000-0x17FF  RAM mirror 2  
0x1800-0x1FFF  RAM mirror 3
0x2000-0x2007  PPU registers (mirrored to 0x3FFF)
0x4000-0x4017  APU and I/O registers
0x4020-0x5FFF  Expansion ROM area
0x6000-0x7FFF  SRAM (battery-backed save RAM)
0x8000-0xFFFF  PRG ROM (32KB)
```

### Safe Write Regions
- **0x0000-0x07FF**: RAM - Always safe for writes
- **Future**: SRAM (0x6000-0x7FFF) when battery-backed

### Unsafe Write Regions
- **0x2000-0x2007**: PPU registers (can affect graphics)
- **0x4000-0x4017**: APU/I/O registers (can affect sound/input)
- **0x8000-0xFFFF**: ROM (read-only, writes ignored or crash)

## Base64 Encoding Examples

### Encoding Data for Write
```bash
# Bash: Encode hex bytes to base64
echo -n -e '\x00\x01\x02\x03' | base64
# Output: AAECAw==

# Python: Encode bytes to base64
import base64
data = bytes([0x00, 0x01, 0x02, 0x03])
encoded = base64.b64encode(data).decode('ascii')
# Output: AAECAw==
```

### Decoding Read Response
```bash
# Bash: Decode base64 to hex
echo "AAECAw==" | base64 -d | xxd
# Output: 00000000: 0001 0203

# Python: Decode base64 to bytes
import base64
decoded = base64.b64decode("AAECAw==")
# Output: b'\x00\x01\x02\x03'
```

## Usage Examples

### Read Entire RAM
```bash
#!/bin/bash

# Read all 2KB of NES RAM
RAM_DATA=$(curl -s http://localhost:8080/api/memory/range/0x0000/2048)
echo $RAM_DATA | jq -r '.data' | base64 -d > ram_dump.bin
echo "RAM dumped to ram_dump.bin"
```

### Pattern Fill Memory
```bash
#!/bin/bash

# Fill first 256 bytes with pattern 0xAA
PATTERN=$(python3 -c "import base64; print(base64.b64encode(b'\\xAA' * 256).decode())")

curl -X POST http://localhost:8080/api/memory/range/0x0000 \
  -H "Content-Type: application/json" \
  -d "{\"data\": \"$PATTERN\"}"
```

### Memory Search
```bash
#!/bin/bash

# Search for specific byte pattern in RAM
TARGET_BYTES="ABCD"  # Looking for 0xAB, 0xCD sequence

for addr in $(seq 0 2046); do
    DATA=$(curl -s http://localhost:8080/api/memory/range/$addr/2)
    HEX=$(echo $DATA | jq -r '.hex')
    
    if [[ "$HEX" == *"abcd"* ]]; then
        printf "Found pattern at address 0x%04X\n" $addr
    fi
done
```

### Atomic Memory Swap
```bash
#!/bin/bash

# Atomically read from one location and write to another
curl -X POST http://localhost:8080/api/memory/batch \
  -H "Content-Type: application/json" \
  -d '{
    "operations": [
      {"type": "read", "address": "0x0000", "length": 1},
      {"type": "read", "address": "0x0001", "length": 1},
      {"type": "write", "address": "0x0000", "data": "Ag=="},
      {"type": "write", "address": "0x0001", "data": "AQ=="}
    ]
  }'
```

## Performance Characteristics

### Single Byte Access
- Individual reads: ~1ms each
- Good for occasional access
- Use when reading < 10 bytes

### Range Access  
- Bulk reads: ~0.003ms per byte
- 332x performance improvement
- Use when reading >= 10 bytes
- Optimal for memory dumps and analysis

### Batch Operations
- Multiple operations under single lock
- Ensures memory state consistency
- Use for complex read-modify-write operations