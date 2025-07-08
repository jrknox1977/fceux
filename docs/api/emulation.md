# Emulation Control Endpoints

Emulation control endpoints allow pausing, resuming, and monitoring the NES emulator state.

## POST /api/emulation/pause

**Description**: Pause NES emulation

**Parameters**: None

**Request Example**:
```bash
curl -X POST http://localhost:8080/api/emulation/pause \
  -H "Content-Type: application/json"
```

**Response** (Success):
```json
{
  "success": true,
  "state": "paused"
}
```

**Response Fields**:
- `success`: Always true for successful requests
- `state`: Always "paused" after successful pause

**Status Codes**:
- `200 OK`: Emulation paused successfully
- `400 Bad Request`: No ROM loaded
- `500 Internal Server Error`: Command execution failed

**Error Response**:
```json
{
  "success": false,
  "error": "No ROM loaded"
}
```

**Notes**:
- Idempotent operation (safe to call multiple times)
- Requires a ROM to be loaded
- Does not affect actual pause state if already paused
- 2-second timeout for command execution

---

## POST /api/emulation/resume

**Description**: Resume NES emulation

**Parameters**: None

**Request Example**:
```bash
curl -X POST http://localhost:8080/api/emulation/resume \
  -H "Content-Type: application/json"
```

**Response** (Success):
```json
{
  "success": true,
  "state": "resumed"
}
```

**Response Fields**:
- `success`: Always true for successful requests
- `state`: Always "resumed" after successful resume

**Status Codes**:
- `200 OK`: Emulation resumed successfully
- `400 Bad Request`: No ROM loaded
- `500 Internal Server Error`: Command execution failed

**Error Response**:
```json
{
  "success": false,
  "error": "No ROM loaded"
}
```

**Notes**:
- Idempotent operation (safe to call multiple times)
- Requires a ROM to be loaded
- Does not affect actual pause state if already running
- 2-second timeout for command execution

---

## GET /api/emulation/status

**Description**: Get current emulation status and statistics

**Parameters**: None

**Request Example**:
```bash
curl -X GET http://localhost:8080/api/emulation/status
```

**Response** (With ROM loaded):
```json
{
  "running": true,
  "paused": false,
  "rom_loaded": true,
  "fps": 60.098814,
  "frame_count": 3245
}
```

**Response** (No ROM loaded):
```json
{
  "running": false,
  "paused": false,
  "rom_loaded": false,
  "fps": 0.0,
  "frame_count": 0
}
```

**Response Fields**:
- `running`: true if ROM loaded and not paused
- `paused`: true if emulation is currently paused
- `rom_loaded`: true if a ROM file is loaded
- `fps`: Target framerate (60.098814 for NTSC, 50.0 for PAL)
- `frame_count`: Total frames executed since ROM load

**Status Codes**:
- `200 OK`: Status retrieved successfully
- `500 Internal Server Error`: Command execution failed

**Notes**:
- No ROM requirement (works without loaded ROM)
- FPS is target rate, not actual performance
- Frame count resets when loading new ROM
- Running state = rom_loaded AND NOT paused

## Error Handling

### Common Error Scenarios

**No ROM Loaded** (400 Bad Request):
```json
{
  "success": false,
  "error": "No ROM loaded"
}
```
- Affects: pause and resume operations
- Solution: Load a ROM file before attempting control

**Command Timeout** (500 Internal Server Error):
- Rare occurrence when emulator thread is unresponsive
- Commands have 2-second timeout
- May indicate system under heavy load

**Internal Errors** (500 Internal Server Error):
- Unexpected Qt or system-level failures
- Should be reported as bugs if reproducible

## State Relationships

### Emulation States
```
No ROM Loaded
├─ running: false
├─ paused: false
├─ rom_loaded: false
└─ fps: 0.0

ROM Loaded + Running
├─ running: true
├─ paused: false
├─ rom_loaded: true
└─ fps: 60.098814 (NTSC) / 50.0 (PAL)

ROM Loaded + Paused
├─ running: false
├─ paused: true
├─ rom_loaded: true
└─ fps: 60.098814 (NTSC) / 50.0 (PAL)
```

### State Transitions
- **Load ROM**: No ROM → Paused (default state after load)
- **Resume**: Paused → Running
- **Pause**: Running → Paused
- **Unload ROM**: Any → No ROM

## Usage Examples

### Basic Pause/Resume Cycle
```bash
#!/bin/bash

# Pause emulation
curl -X POST http://localhost:8080/api/emulation/pause

# Check status
curl -X GET http://localhost:8080/api/emulation/status

# Resume after 5 seconds
sleep 5
curl -X POST http://localhost:8080/api/emulation/resume
```

### Status Monitoring
```bash
#!/bin/bash

# Monitor emulation state every second
while true; do
    STATUS=$(curl -s http://localhost:8080/api/emulation/status)
    RUNNING=$(echo $STATUS | jq -r '.running')
    FRAMES=$(echo $STATUS | jq -r '.frame_count')
    
    echo "$(date): Running=$RUNNING, Frames=$FRAMES"
    sleep 1
done
```

### Conditional Control
```bash
#!/bin/bash

# Only pause if currently running
STATUS=$(curl -s http://localhost:8080/api/emulation/status)
RUNNING=$(echo $STATUS | jq -r '.running')

if [ "$RUNNING" = "true" ]; then
    echo "Pausing emulation..."
    curl -X POST http://localhost:8080/api/emulation/pause
else
    echo "Emulation not running"
fi
```

### Frame-Based Automation
```bash
#!/bin/bash

# Run for exactly 1000 frames
curl -X POST http://localhost:8080/api/emulation/resume

START_FRAMES=$(curl -s http://localhost:8080/api/emulation/status | jq -r '.frame_count')
TARGET_FRAMES=$((START_FRAMES + 1000))

while true; do
    CURRENT_FRAMES=$(curl -s http://localhost:8080/api/emulation/status | jq -r '.frame_count')
    if [ $CURRENT_FRAMES -ge $TARGET_FRAMES ]; then
        curl -X POST http://localhost:8080/api/emulation/pause
        echo "Completed 1000 frames"
        break
    fi
    sleep 0.1
done
```

## Performance Considerations

- Pause/resume operations are lightweight (< 1ms typical)
- Status queries have minimal overhead
- Frame count monitoring useful for precise timing
- Commands processed in emulator thread context for safety