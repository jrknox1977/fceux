# REST API Emulation Control Endpoints

This document describes the emulation control endpoints added to FCEUX REST API.

## Prerequisites

Build FCEUX with REST API support:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DREST_API=ON
make -j$(nproc)
```

## Running FCEUX with REST API

```bash
./src/fceux --rest-api --rest-port 8080 your_rom.nes
```

## Endpoints

### 1. Pause Emulation

**Endpoint:** `POST /api/emulation/pause`

**Description:** Pauses the emulation if currently running.

**Response (Success):**
```json
{
  "success": true,
  "state": "paused"
}
```

**Response (Error - No ROM):**
```json
{
  "success": false,
  "error": "No ROM loaded"
}
```

**Example:**
```bash
curl -X POST http://localhost:8080/api/emulation/pause
```

### 2. Resume Emulation

**Endpoint:** `POST /api/emulation/resume`

**Description:** Resumes the emulation if currently paused.

**Response (Success):**
```json
{
  "success": true,
  "state": "resumed"
}
```

**Response (Error - No ROM):**
```json
{
  "success": false,
  "error": "No ROM loaded"
}
```

**Example:**
```bash
curl -X POST http://localhost:8080/api/emulation/resume
```

### 3. Get Emulation Status

**Endpoint:** `GET /api/emulation/status`

**Description:** Returns the current emulation status including running state, pause state, loaded ROM status, FPS, and frame count.

**Response:**
```json
{
  "running": true,
  "paused": false,
  "rom_loaded": true,
  "fps": 60.098824,
  "frame_count": 12345
}
```

**Field Descriptions:**
- `running`: true if ROM is loaded and emulation is not paused
- `paused`: true if emulation is paused
- `rom_loaded`: true if a ROM is currently loaded
- `fps`: Current frames per second (60.1 for NTSC, 50.0 for PAL)
- `frame_count`: Total frames emulated since ROM was loaded

**Example:**
```bash
curl http://localhost:8080/api/emulation/status
```

## Error Handling

All endpoints return appropriate HTTP status codes:
- `200 OK`: Operation successful
- `400 Bad Request`: Invalid request (e.g., no ROM loaded)
- `500 Internal Server Error`: Server error or timeout

## Thread Safety

All commands are executed on the emulator thread via a thread-safe command queue. Commands have a 2-second timeout to prevent hanging.

## Testing Script

A test script is provided at `test_rest_api.sh` to verify all endpoints:

```bash
./test_rest_api.sh
```

## Implementation Details

The emulation control is implemented using:
- `EmulationCommands.h`: Command classes for pause, resume, and status
- `EmulationController.cpp`: HTTP endpoint handlers
- Thread-safe command queue for cross-thread communication
- FCEUX core functions: `FCEUI_SetEmulationPaused()`, `FCEUI_EmulationPaused()`, `FCEUI_GetDesiredFPS()`