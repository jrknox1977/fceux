# Input Control Endpoints

Input control endpoints allow programmatic simulation of NES controller input for automated testing, AI training, and game automation.

## GET /api/input/status

**Description**: Get current state of all controller buttons

**Parameters**: None

**Request Example**:
```bash
curl -X GET http://localhost:8080/api/input/status
```

**Response**:
```json
{
  "port1": {
    "connected": true,
    "buttons": {
      "A": false,
      "B": false,
      "SELECT": false,
      "START": false,
      "UP": false,
      "DOWN": false,
      "LEFT": false,
      "RIGHT": false
    }
  },
  "port2": {
    "connected": true,
    "buttons": {
      "A": false,
      "B": false,
      "SELECT": false,
      "START": false,
      "UP": false,
      "DOWN": false,
      "LEFT": false,
      "RIGHT": false
    }
  }
}
```

**Response Fields**:
- `port1`, `port2`: Controller states for ports 1 and 2
- `connected`: Always true (NES controllers are always logically connected)
- `buttons`: Object with button name/state pairs

**Button Names**:
- `"A"`: A button (primary action)
- `"B"`: B button (secondary action/run)
- `"SELECT"`: Select button (menu navigation)
- `"START"`: Start button (pause/menu)
- `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`: D-pad directions

**Status Codes**:
- `200 OK`: Input status retrieved successfully
- `500 Internal Server Error`: Command execution failed

**Notes**:
- No ROM loading requirement
- Reflects current virtual controller state
- Updated in real-time during emulation

---

## POST /api/input/port/{port}/press

**Description**: Press specific buttons with optional hold duration

**Parameters**:
- `port` (path): Controller port number (1 or 2)

**Request Body**:
```json
{
  "buttons": ["A", "B"],
  "duration_ms": 16
}
```

**Request Examples**:
```bash
# Press A button for default duration (16ms)
curl -X POST http://localhost:8080/api/input/port/1/press \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["A"]}'

# Press multiple buttons for 100ms
curl -X POST http://localhost:8080/api/input/port/1/press \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["A", "UP"], "duration_ms": 100}'

# Jump (A + B) for 2 frames
curl -X POST http://localhost:8080/api/input/port/1/press \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["A", "B"], "duration_ms": 33}'
```

**Request Fields**:
- `buttons`: Array of button names to press
- `duration_ms`: Hold duration in milliseconds (optional, default: 16)

**Response**:
```json
{
  "success": true,
  "port": 1,
  "buttons_pressed": ["A", "B"],
  "duration_ms": 16
}
```

**Response Fields**:
- `success`: Always true for successful operations
- `port`: Controller port used
- `buttons_pressed`: Array of buttons that were pressed
- `duration_ms`: Actual hold duration applied

**Duration Guidelines**:
- **16ms** (1 frame @ 60 FPS): Quick tap, menu selection
- **33ms** (2 frames): Standard button press
- **100ms** (6 frames): Held action, jumping
- **500ms+**: Long hold, charging attacks

**Status Codes**:
- `200 OK`: Buttons pressed successfully
- `400 Bad Request`: Invalid port, button names, or JSON format
- `503 Service Unavailable`: No game loaded
- `504 Gateway Timeout`: Command execution timeout

**Error Responses**:
```json
// Invalid button name
{
  "error": "Invalid button name: X"
}

// Invalid port
{
  "error": "Invalid port number. Must be 1 or 2"
}

// Missing buttons field
{
  "error": "Missing or invalid 'buttons' array"
}
```

**Notes**:
- Buttons pressed immediately, released automatically after duration
- Multiple buttons can be pressed simultaneously
- Precise timing using emulator frame counter
- Default duration equals one 60 FPS frame

---

## POST /api/input/port/{port}/release

**Description**: Release specific buttons or all buttons

**Parameters**:
- `port` (path): Controller port number (1 or 2)

**Request Body** (Optional):
```json
{
  "buttons": ["A", "B"]
}
```

**Request Examples**:
```bash
# Release specific buttons
curl -X POST http://localhost:8080/api/input/port/1/release \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["A", "B"]}'

# Release all buttons (empty body)
curl -X POST http://localhost:8080/api/input/port/1/release \
  -H "Content-Type: application/json" \
  -d '{}'

# Release all buttons (no body)
curl -X POST http://localhost:8080/api/input/port/1/release
```

**Request Fields**:
- `buttons`: Array of button names to release (optional)
- If `buttons` omitted or empty: releases all pressed buttons

**Response**:
```json
{
  "success": true,
  "port": 1,
  "buttons_released": ["A", "B"]
}
```

**Response Fields**:
- `success`: Always true for successful operations
- `port`: Controller port used
- `buttons_released`: Array of buttons that were released

**Status Codes**:
- `200 OK`: Buttons released successfully
- `400 Bad Request`: Invalid port, button names, or JSON format
- `503 Service Unavailable`: No game loaded
- `504 Gateway Timeout`: Command execution timeout

**Notes**:
- Safe to call on already-released buttons (no-op)
- Immediately cancels any pending timed releases
- Use for manual control override
- Empty request body releases all buttons

---

## POST /api/input/port/{port}/state

**Description**: Set complete controller state atomically

**Parameters**:
- `port` (path): Controller port number (1 or 2)

**Request Body**:
```json
{
  "A": true,
  "B": false,
  "SELECT": false,
  "START": false,
  "UP": true,
  "DOWN": false,
  "LEFT": false,
  "RIGHT": false
}
```

**Request Examples**:
```bash
# Set specific state (A + UP pressed)
curl -X POST http://localhost:8080/api/input/port/1/state \
  -H "Content-Type: application/json" \
  -d '{
    "A": true,
    "B": false,
    "UP": true,
    "DOWN": false,
    "LEFT": false,
    "RIGHT": false,
    "SELECT": false,
    "START": false
  }'

# Clear all buttons
curl -X POST http://localhost:8080/api/input/port/1/state \
  -H "Content-Type: application/json" \
  -d '{
    "A": false,
    "B": false,
    "UP": false,
    "DOWN": false,
    "LEFT": false,
    "RIGHT": false,
    "SELECT": false,
    "START": false
  }'
```

**Request Fields**:
- Button names as keys with boolean values
- Can specify any subset of buttons
- Unspecified buttons retain current state

**Response**:
```json
{
  "success": true,
  "port": 1,
  "state": 145
}
```

**Response Fields**:
- `success`: Always true for successful operations
- `port`: Controller port used
- `state`: Final button state as 8-bit value (for debugging)

**State Bitmask** (for reference):
```
Bit 0 (0x01): A
Bit 1 (0x02): B
Bit 2 (0x04): SELECT
Bit 3 (0x08): START
Bit 4 (0x10): UP
Bit 5 (0x20): DOWN
Bit 6 (0x40): LEFT
Bit 7 (0x80): RIGHT
```

**Status Codes**:
- `200 OK`: State set successfully
- `400 Bad Request`: Invalid port, button names, or values
- `503 Service Unavailable`: No game loaded
- `504 Gateway Timeout`: Command execution timeout

**Notes**:
- Atomic operation - all buttons updated simultaneously
- Overrides any pending timed releases
- Most efficient for complex input patterns
- Use for frame-perfect input sequences

## Input Timing and Synchronization

### Frame-Based Timing
- NES runs at ~60.098814 FPS (NTSC) or 50 FPS (PAL)
- Frame duration: ~16.639ms (NTSC) or 20ms (PAL)
- Input processed once per frame during emulation

### Timing Precision
- **press** operations: Frame-accurate timing via emulator counter
- **duration_ms**: Converted to frame counts internally
- **Minimum duration**: 1 frame (~16ms)
- **Maximum practical**: ~1 second (60 frames)

### Synchronization
All input commands execute in the emulator thread with proper mutex synchronization to ensure:
- No input corruption during state changes
- Atomic multi-button operations
- Consistent timing relative to emulation

## Common Input Patterns

### Basic Movement
```bash
# Walk right
curl -X POST http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["RIGHT"], "duration_ms": 500}'

# Walk left  
curl -X POST http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["LEFT"], "duration_ms": 500}'
```

### Jump Mechanics
```bash
# Simple jump
curl -X POST http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["A"], "duration_ms": 33}'

# Running jump (B + A)
curl -X POST http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["B", "A"], "duration_ms": 50}'

# Directional jump (RIGHT + A)
curl -X POST http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["RIGHT", "A"], "duration_ms": 100}'
```

### Menu Navigation
```bash
# Navigate down in menu
curl -X POST http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["DOWN"], "duration_ms": 16}'

# Select menu item
curl -X POST http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["A"], "duration_ms": 16}'

# Back/Cancel
curl -X POST http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["B"], "duration_ms": 16}'
```

## Advanced Usage Examples

### Konami Code Input
```bash
#!/bin/bash

# Famous Konami Code: UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A
KONAMI_SEQUENCE=("UP" "UP" "DOWN" "DOWN" "LEFT" "RIGHT" "LEFT" "RIGHT" "B" "A")

for button in "${KONAMI_SEQUENCE[@]}"; do
    curl -s -X POST http://localhost:8080/api/input/port/1/press \
      -H "Content-Type: application/json" \
      -d "{\"buttons\": [\"$button\"], \"duration_ms\": 16}"
    sleep 0.1  # Brief pause between inputs
done
```

### Frame-Perfect Input Sequence
```bash
#!/bin/bash

# Frame-perfect combo for fighting games
# Set state, wait for exact timing, change state

# Frame 1: Charge down
curl -s -X POST http://localhost:8080/api/input/port/1/state \
  -d '{"DOWN": true, "A": false, "B": false}'

# Wait exactly 30 frames (500ms)
sleep 0.5

# Frame 31: Release down, press A
curl -s -X POST http://localhost:8080/api/input/port/1/state \
  -d '{"DOWN": false, "A": true, "B": false}'

# Frame 32: Add B for combo
sleep 0.016
curl -s -X POST http://localhost:8080/api/input/port/1/state \
  -d '{"DOWN": false, "A": true, "B": true}'
```

### Input Automation Loop
```bash
#!/bin/bash

# Automated grinding: repeatedly press A button
while true; do
    # Press A
    curl -s -X POST http://localhost:8080/api/input/port/1/press \
      -d '{"buttons": ["A"], "duration_ms": 33}'
    
    # Wait before next press
    sleep 0.5
    
    # Check if we should continue
    STATUS=$(curl -s http://localhost:8080/api/emulation/status | jq -r '.running')
    if [ "$STATUS" != "true" ]; then
        echo "Emulation stopped, ending automation"
        break
    fi
done
```

### Multi-Controller Demo
```bash
#!/bin/bash

# Two-player cooperative movement
# Player 1 goes right, Player 2 goes left

curl -s -X POST http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["RIGHT"], "duration_ms": 1000}' &

curl -s -X POST http://localhost:8080/api/input/port/2/press \
  -d '{"buttons": ["LEFT"], "duration_ms": 1000}' &

wait
echo "Synchronized movement complete"
```

### Input State Monitoring
```bash
#!/bin/bash

# Monitor input changes
LAST_STATE=""

while true; do
    CURRENT_STATE=$(curl -s http://localhost:8080/api/input/status)
    
    if [ "$CURRENT_STATE" != "$LAST_STATE" ]; then
        echo "$(date): Input changed"
        echo $CURRENT_STATE | jq '.port1.buttons'
        LAST_STATE="$CURRENT_STATE"
    fi
    
    sleep 0.1
done
```

## Error Handling

### Invalid Button Names
Valid button names are case-sensitive:
- Correct: `"A"`, `"B"`, `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`, `"SELECT"`, `"START"`
- Invalid: `"a"`, `"button_a"`, `"BUTTON_A"`, `"up"`, `"X"`, `"Y"`

### Controller Port Validation
- Valid ports: 1, 2
- Invalid ports return 400 Bad Request
- NES only supports 2 standard controllers

### Timing Limitations
- Minimum duration: ~16ms (1 frame)
- Durations < 16ms rounded up to 16ms
- Very long durations (> 10 seconds) may timeout

### Game State Dependencies
- Some operations require ROM loaded (503 error)
- Input commands generally work without ROM
- Real game response requires active emulation

## Performance Considerations

- Input commands are lightweight (< 1ms overhead)
- No artificial rate limiting on input frequency
- Natural limit: ~60 commands/second due to frame rate
- Multiple simultaneous button presses more efficient than sequence
- Use `state` endpoint for complex input patterns