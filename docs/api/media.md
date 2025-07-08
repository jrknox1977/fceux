# Media Operations Endpoints

Media operations endpoints provide screenshot capture and save state management for game preservation and automation.

## POST /api/screenshot

**Description**: Capture a screenshot of the current emulator display

**Parameters**: None (all options specified in request body)

**Request Body** (All fields optional):
```json
{
  "format": "png",
  "encoding": "file",
  "path": "/custom/path/screenshot.png"
}
```

**Request Examples**:
```bash
# Default screenshot (PNG, base64)
curl -X POST http://localhost:8080/api/screenshot \
  -H "Content-Type: application/json"

# Custom format
curl -X POST http://localhost:8080/api/screenshot \
  -H "Content-Type: application/json" \
  -d '{
    "format": "jpg"
  }'

# Specify encoding explicitly
curl -X POST http://localhost:8080/api/screenshot \
  -H "Content-Type: application/json" \
  -d '{
    "format": "png",
    "encoding": "base64"
  }'
```

**Request Fields**:
- `format`: Image format ("png", "jpg", "bmp") - default: "png"
- `encoding`: Output mode (currently only "base64" supported)
- `path`: Custom file path (reserved for future file support)

**Response**:
```json
{
  "success": true,
  "format": "png", 
  "encoding": "base64",
  "data": "iVBORw0KGgoAAAANSUhEUgAAAQAAAAEACAIAAADTED8xAAADMElEQVR4nO..."
}
```

**Response Fields**:
- `success`: true if screenshot captured successfully
- `format`: Image format used
- `encoding`: Always "base64" in current implementation
- `data`: Base64-encoded image data

**Format Details**:
- **PNG**: Lossless compression, best quality, larger files
- **JPG**: Lossy compression, smaller files, good for sharing
- **BMP**: Uncompressed, largest files, perfect quality

**Status Codes**:
- `200 OK`: Screenshot captured successfully
- `400 Bad Request`: Invalid format or encoding parameters
- `500 Internal Server Error`: File system error or capture failure

**Error Response**:
```json
{
  "success": false,
  "error": "Unable to write to specified path"
}
```

**Notes**:
- Captures current frame buffer at time of request
- Auto-generated filenames use timestamp: "fceux-YYYYMMDD-HHMMSS.{ext}"
- Base64 encoding useful for web applications and APIs
- 2-second timeout for screenshot operations

---

## GET /api/screenshot/last

**Description**: Get information about the most recently captured screenshot

**Parameters**: None

**Request Example**:
```bash
curl -X GET http://localhost:8080/api/screenshot/last
```

**Response** (Recent screenshot exists):
```json
{
  "success": true,
  "format": "png",
  "encoding": "file", 
  "filename": "fceux-20250708-123456.png",
  "path": "/full/path/to/fceux-20250708-123456.png"
}
```

**Response** (No screenshots taken):
```json
{
  "success": false,
  "error": "No screenshots have been taken yet"
}
```

**Response Fields**:
- Same fields as screenshot endpoint
- Contains info about last successful screenshot
- Reset when FCEUX restarts

**Status Codes**:
- `200 OK`: Last screenshot info retrieved
- `500 Internal Server Error`: No previous screenshots or system error

**Notes**:
- Only tracks file-based screenshots (not base64)
- Information persists during session
- Useful for automation scripts tracking captures

---

## POST /api/savestate

**Description**: Save current emulation state to memory or file

**Parameters**: None (all options specified in request body)

**Request Body** (All fields optional):
```json
{
  "slot": 0,
  "path": "/custom/path/mario-level1.fc0"
}
```

**Request Examples**:
```bash
# Save to memory (quick save/load)
curl -X POST http://localhost:8080/api/savestate \
  -H "Content-Type: application/json" \
  -d '{"slot": -1}'

# Save to slot 3
curl -X POST http://localhost:8080/api/savestate \
  -H "Content-Type: application/json" \
  -d '{"slot": 3}'

# Save with custom filename
curl -X POST http://localhost:8080/api/savestate \
  -H "Content-Type: application/json" \
  -d '{"path": "/saves/mario-world1-1.fc0"}'

# Default save (slot 0)
curl -X POST http://localhost:8080/api/savestate \
  -H "Content-Type: application/json"
```

**Request Fields**:
- `slot`: Save slot number (-1 for memory, 0-9 for file slots) - default: 0
- `path`: Custom file path (overrides slot-based naming)

**Response**:
```json
{
  "success": true,
  "slot": 3,
  "filename": "mario.fc3", 
  "timestamp": "2025-07-08T12:34:56Z"
}
```

**Response Fields**:
- `success`: true if save state created successfully
- `slot`: Slot number used (0-9, or -1 for memory)
- `filename`: Generated or specified filename
- `timestamp`: ISO 8601 timestamp of save creation

**Slot System**:
- **-1**: Memory save (fastest, temporary)
- **0-9**: File slots (persistent, numbered)
- **Custom path**: User-specified location

**Status Codes**:
- `200 OK`: Save state created successfully
- `400 Bad Request`: Invalid slot number or path
- `503 Service Unavailable`: No game loaded
- `500 Internal Server Error`: File system error or save failure

**Error Responses**:
```json
// Invalid slot
{
  "success": false,
  "error": "Invalid slot number. Must be -1 (memory) or 0-9"
}

// No game loaded
{
  "success": false,
  "error": "No game loaded"
}
```

**Notes**:
- Requires ROM to be loaded
- Memory saves are faster but lost on restart
- File saves persist between sessions
- Save state includes complete CPU, PPU, APU, and memory state
- 2-second timeout for save operations

---

## POST /api/loadstate

**Description**: Load a previously saved emulation state

**Parameters**: None (all options specified in request body)

**Request Body** (Optional fields):
```json
{
  "slot": 3,
  "path": "/custom/path/mario-level1.fc0",
  "data": "base64_encoded_save_state_data"
}
```

**Request Examples**:
```bash
# Load from memory
curl -X POST http://localhost:8080/api/loadstate \
  -H "Content-Type: application/json" \
  -d '{"slot": -1}'

# Load from slot 3
curl -X POST http://localhost:8080/api/loadstate \
  -H "Content-Type: application/json" \
  -d '{"slot": 3}'

# Load from custom file
curl -X POST http://localhost:8080/api/loadstate \
  -H "Content-Type: application/json" \
  -d '{"path": "/saves/mario-world1-1.fc0"}'

# Load from base64 data
curl -X POST http://localhost:8080/api/loadstate \
  -H "Content-Type: application/json" \
  -d '{"data": "base64_encoded_save_data..."}'

# Default load (slot 0)
curl -X POST http://localhost:8080/api/loadstate \
  -H "Content-Type: application/json"
```

**Request Fields**:
- `slot`: Save slot number (-1 for memory, 0-9 for file slots) - default: 0
- `path`: Custom file path to load from
- `data`: Base64-encoded save state data

**Priority Order**:
1. `data` field (base64) - highest priority
2. `path` field (custom file)
3. `slot` field (slot-based file) - default behavior

**Response**:
```json
{
  "success": true,
  "slot": 3,
  "filename": "mario.fc3",
  "timestamp": "2025-07-08T12:34:56Z"
}
```

**Response Fields**:
- `success`: true if save state loaded successfully
- `slot`: Slot number used (if applicable)
- `filename`: Filename that was loaded
- `timestamp`: Original save timestamp

**Status Codes**:
- `200 OK`: Save state loaded successfully
- `400 Bad Request`: Invalid parameters or missing save state
- `503 Service Unavailable`: No game loaded
- `500 Internal Server Error`: File system error or load failure

**Error Responses**:
```json
// Save state not found
{
  "success": false,
  "error": "Save state slot 5 does not exist"
}

// Invalid base64 data
{
  "success": false,
  "error": "Invalid base64 save state data"
}
```

**Notes**:
- Requires ROM to be loaded
- Loading immediately restores complete emulation state
- Base64 data useful for network transfer and storage
- Memory saves only exist if previously created in session
- 2-second timeout for load operations

---

## GET /api/savestate/list

**Description**: List all available save states for the current ROM

**Parameters**: None

**Request Example**:
```bash
curl -X GET http://localhost:8080/api/savestate/list
```

**Response**:
```json
{
  "success": true,
  "slots": [
    {
      "slot": 0,
      "filename": "mario.fc0",
      "timestamp": "2025-07-08T12:30:00Z",
      "size": 16384,
      "exists": true
    },
    {
      "slot": 1,
      "filename": "mario.fc1", 
      "timestamp": "",
      "size": 0,
      "exists": false
    }
  ],
  "memory": {
    "exists": true,
    "timestamp": "2025-07-08T12:35:00Z"
  }
}
```

**Response Fields**:
- `success`: true if listing completed successfully
- `slots`: Array of slot information (0-9)
- `memory`: Information about memory save state

**Slot Information**:
- `slot`: Slot number (0-9)
- `filename`: Save state filename
- `timestamp`: Creation time (empty if doesn't exist)
- `size`: File size in bytes (0 if doesn't exist)
- `exists`: true if save state exists

**Status Codes**:
- `200 OK`: Save state list retrieved successfully
- `503 Service Unavailable`: No game loaded
- `500 Internal Server Error`: File system error

**Notes**:
- Only shows saves for currently loaded ROM
- Empty slots included with exists: false
- Memory save info included if present
- File sizes help estimate load times

## Save State File Format

### File Naming Convention
- **Slot saves**: `{rom_name}.fc{slot}` (e.g., "mario.fc0", "zelda.fc5")
- **Custom saves**: User-specified filename and extension
- **Memory saves**: Stored in RAM, no file created

### File Contents
Save states contain complete emulator state:
- CPU registers and state
- PPU (graphics) registers and VRAM
- APU (audio) registers and state
- Main RAM (2KB) and SRAM if present
- Mapper state and bank selections
- Input device states
- Frame counter and timing information

### Compatibility
- Save states are ROM-specific
- Compatible between FCEUX versions (generally)
- Platform-independent (can transfer between OS)
- Include ROM checksum for validation

## Usage Examples

### Automated Screenshot Capture
```bash
#!/bin/bash

# Capture screenshots every 10 seconds
while true; do
    TIMESTAMP=$(date +%Y%m%d-%H%M%S)
    
    curl -s -X POST http://localhost:8080/api/screenshot \
      -H "Content-Type: application/json" \
      -d "{\"path\": \"/screenshots/auto-$TIMESTAMP.png\"}"
    
    echo "Screenshot captured: auto-$TIMESTAMP.png"
    sleep 10
done
```

### Quick Save/Load Workflow
```bash
#!/bin/bash

# Quick save before attempting difficult section
echo "Creating quick save..."
curl -s -X POST http://localhost:8080/api/savestate \
  -H "Content-Type: application/json" \
  -d '{"slot": -1}'

# Simulate playing/testing
echo "Playing game section..."
sleep 30

# Quick load to retry
echo "Loading quick save..."
curl -s -X POST http://localhost:8080/api/loadstate \
  -H "Content-Type: application/json" \
  -d '{"slot": -1}'
```

### Progressive Save System
```bash
#!/bin/bash

# Save to multiple slots for backup
CURRENT_SLOT=0
MAX_SLOTS=5

while true; do
    # Save to current slot
    echo "Saving to slot $CURRENT_SLOT..."
    curl -s -X POST http://localhost:8080/api/savestate \
      -H "Content-Type: application/json" \
      -d "{\"slot\": $CURRENT_SLOT}"
    
    # Advance to next slot (cycling)
    CURRENT_SLOT=$(( (CURRENT_SLOT + 1) % MAX_SLOTS ))
    
    # Wait for significant progress
    sleep 300  # 5 minutes
done
```

### Screenshot Comparison Tool
```bash
#!/bin/bash

# Capture before and after screenshots
echo "Capturing before screenshot..."
curl -s -X POST http://localhost:8080/api/screenshot \
  -d '{"path": "/tmp/before.png", "format": "png"}'

# Perform some action (input, state change, etc.)
curl -s -X POST http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["A"], "duration_ms": 100}'

sleep 1

echo "Capturing after screenshot..."
curl -s -X POST http://localhost:8080/api/screenshot \
  -d '{"path": "/tmp/after.png", "format": "png"}'

# Compare using imagemagick (if available)
if command -v compare &> /dev/null; then
    compare /tmp/before.png /tmp/after.png /tmp/diff.png
    echo "Difference image saved to /tmp/diff.png"
fi
```

### Save State Analysis
```bash
#!/bin/bash

# List all save states and show statistics
SAVES=$(curl -s http://localhost:8080/api/savestate/list)

echo "Save State Analysis:"
echo "==================="

# Count existing saves
EXISTING=$(echo $SAVES | jq '[.slots[] | select(.exists == true)] | length')
echo "Existing saves: $EXISTING/10 slots"

# Show total storage used
TOTAL_SIZE=$(echo $SAVES | jq '[.slots[] | select(.exists == true) | .size] | add')
echo "Total storage: $TOTAL_SIZE bytes"

# List saves with timestamps
echo
echo "Save Details:"
echo $SAVES | jq -r '.slots[] | select(.exists == true) | "Slot \(.slot): \(.timestamp) (\(.size) bytes)"'

# Check memory save
MEMORY_EXISTS=$(echo $SAVES | jq -r '.memory.exists')
if [ "$MEMORY_EXISTS" = "true" ]; then
    MEMORY_TIME=$(echo $SAVES | jq -r '.memory.timestamp')
    echo "Memory save: $MEMORY_TIME"
fi
```

### Screenshot to Base64 for Web
```bash
#!/bin/bash

# Capture screenshot as base64 for web embedding
SCREENSHOT=$(curl -s -X POST http://localhost:8080/api/screenshot \
  -H "Content-Type: application/json" \
  -d '{"encoding": "base64", "format": "png"}')

# Extract base64 data
BASE64_DATA=$(echo $SCREENSHOT | jq -r '.data')

# Create HTML with embedded image
cat > /tmp/screenshot.html << EOF
<!DOCTYPE html>
<html>
<head><title>FCEUX Screenshot</title></head>
<body>
<h1>Game Screenshot</h1>
<img src="data:image/png;base64,$BASE64_DATA" alt="FCEUX Screenshot">
</body>
</html>
EOF

echo "HTML with embedded screenshot saved to /tmp/screenshot.html"
```

## Performance Characteristics

### Screenshot Operations
- **PNG**: ~50-100ms (depends on screen complexity)
- **JPG**: ~30-80ms (faster compression)
- **BMP**: ~20-40ms (no compression)
- **Base64 encoding**: Additional ~10-20ms overhead

### Save State Operations
- **Memory save**: ~5-10ms (RAM operation)
- **File save**: ~20-50ms (disk I/O dependent)
- **Save size**: ~16-20KB typical (depends on mapper)
- **Load time**: Similar to save time

### Storage Requirements
- **Screenshots**: 2-50KB (format dependent)
- **Save states**: 16-20KB each
- **10 save slots**: ~200KB total per ROM

## Error Handling

### Common Issues
- **No game loaded**: Most operations require ROM
- **Disk space**: Large screenshots and many saves consume storage
- **File permissions**: Custom paths need write access
- **Invalid slots**: Slot numbers must be -1 to 9

### Best Practices
- Check game loaded status before media operations
- Use memory saves for temporary checkpoints
- Regular cleanup of old screenshots and saves
- Error handling for file system operations
- Validate custom paths before use