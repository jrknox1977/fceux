# FCEUX Input Control REST API Documentation

This document describes the REST API endpoints for controlling NES gamepad input in FCEUX.

## Overview

The Input Control API allows programmatic control of NES gamepads through REST endpoints. You can:
- Read current button states
- Press buttons with optional duration
- Release specific or all buttons
- Set complete controller state

## Prerequisites

- FCEUX must be built with `-DREST_API=ON`
- A game must be loaded for input commands to work
- API server runs on port 8080 by default

## Button Names

The NES controller has 8 buttons:
- `A` - A button
- `B` - B button  
- `Select` - Select button
- `Start` - Start button
- `Up` - D-pad up
- `Down` - D-pad down
- `Left` - D-pad left
- `Right` - D-pad right

## Endpoints

### GET /api/input/status

Get current input state for all controllers.

**Response:**
```json
{
  "port1": {
    "connected": true,
    "buttons": {
      "A": false,
      "B": false,
      "Select": false,
      "Start": false,
      "Up": false,
      "Down": false,
      "Left": false,
      "Right": false
    }
  },
  "port2": {
    "connected": true,
    "buttons": {
      "A": false,
      "B": false,
      "Select": false,
      "Start": false,
      "Up": false,
      "Down": false,
      "Left": false,
      "Right": false
    }
  }
}
```

### POST /api/input/port/{port}/press

Press buttons on a specific controller port (1 or 2).

**Request Body:**
```json
{
  "buttons": ["A", "Right"],
  "duration_ms": 100
}
```

- `buttons` (required): Array of button names to press
- `duration_ms` (optional): How long to hold buttons in milliseconds (default: 16ms ≈ 1 frame)

**Response:**
```json
{
  "success": true,
  "port": 1,
  "buttons_pressed": ["A", "Right"],
  "duration_ms": 100
}
```

### POST /api/input/port/{port}/release

Release specific buttons or all buttons.

**Request Body (optional):**
```json
{
  "buttons": ["A"]
}
```

- `buttons` (optional): Array of buttons to release. If omitted, releases all buttons.

**Response:**
```json
{
  "success": true,
  "port": 1,
  "buttons_released": ["A"]
}
```

### POST /api/input/port/{port}/state

Set complete controller state.

**Request Body:**
```json
{
  "A": true,
  "B": false,
  "Select": false,
  "Start": false,
  "Up": false,
  "Down": true,
  "Left": false,
  "Right": true
}
```

**Response:**
```json
{
  "success": true,
  "port": 1,
  "state": {
    "A": true,
    "B": false,
    "Select": false,
    "Start": false,
    "Up": false,
    "Down": true,
    "Left": false,
    "Right": true
  }
}
```

## Error Responses

- **400 Bad Request**: Invalid port number, unknown button names, or malformed JSON
- **503 Service Unavailable**: No game loaded
- **504 Gateway Timeout**: Command execution timeout

Example error response:
```json
{
  "error": "Invalid button name: X"
}
```

## Frame Timing

The NES runs at approximately:
- 60 FPS for NTSC (16.67ms per frame)
- 50 FPS for PAL (20ms per frame)

When specifying `duration_ms`, the actual duration will be rounded to the nearest frame. For example:
- 10ms → 1 frame
- 50ms → 3 frames  
- 100ms → 6 frames

## Example Usage

### Make Mario jump right
```bash
curl -X POST http://localhost:8080/api/input/port/1/press \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["Right", "A"], "duration_ms": 200}'
```

### Press Start button
```bash
curl -X POST http://localhost:8080/api/input/port/1/press \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["Start"], "duration_ms": 50}'
```

### Check current input state
```bash
curl http://localhost:8080/api/input/status
```

### Release all buttons
```bash
curl -X POST http://localhost:8080/api/input/port/1/release \
  -H "Content-Type: application/json"
```

## Implementation Notes

### Architecture
The REST API input control uses a Lua-style overlay mask system to ensure reliable input injection:

- **Overlay Masks**: Two masks per controller port
  - `apiJoypadMask1[]` - AND mask (0xFF = pass through, 0x00 = force clear)
  - `apiJoypadMask2[]` - OR mask (0x00 = no effect, 0xFF = force set)
- **Integration Point**: `UpdateGP()` in `input.cpp` applies overlays after physical input polling
- **Frame-based Timing**: Button releases are processed in `fceuWrapperUpdate()` each frame
- **Thread Safety**: Commands execute via the command queue with proper mutex protection

### Technical Details
- Input overlays are applied using: `joyl = (joyl & apiJoypadMask1[which]) | apiJoypadMask2[which]`
- Masks reset after each frame to ensure single-frame effect unless duration is specified
- The system follows the same pattern as Lua input (`FCEU_LuaReadJoypad`)
- Multiple simultaneous button presses are supported (e.g., Right+A for jumping while moving)

### Files Modified
- `src/drivers/Qt/RestApi/InputApi.h/cpp` - Core overlay system
- `src/drivers/Qt/RestApi/Commands/InputCommands.h/cpp` - Command implementations
- `src/drivers/Qt/RestApi/FceuxApiServer.cpp` - REST endpoint registration
- `src/input.cpp` - Integration point for overlay application
- `src/drivers/Qt/fceuWrapper.cpp` - Frame-based release processing