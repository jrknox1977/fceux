# FCEUX REST API Documentation

## Overview

The FCEUX REST API provides HTTP endpoints for controlling the NES emulator programmatically. This API enables developers to integrate FCEUX into automated testing frameworks, AI research, game analysis tools, and other applications requiring emulator control.

## Key Features

- **Thread-Safe**: All operations use proper synchronization with the emulator thread
- **Real-Time Control**: Pause, resume, and control emulation state
- **Memory Access**: Read and write NES memory with safety validation
- **Input Simulation**: Programmatically control NES controllers
- **Save States**: Create, load, and manage save states
- **Screenshots**: Capture emulator output in various formats
- **Machine-Readable**: OpenAPI specification for automated integration

## Quick Start

### Prerequisites

- FCEUX compiled with REST API support (`cmake -DREST_API=ON`)
- A loaded NES ROM for most operations

### Starting the API Server

#### Via GUI
1. Launch FCEUX
2. Go to **Tools → REST API Server**
3. Click to enable the server
4. Server starts on `http://127.0.0.1:8080` by default

#### Via Configuration
The server can be configured to auto-start:
```ini
# In FCEUX config file
SDL.RestApiEnabled = 1
SDL.RestApiPort = 8080
SDL.RestApiBindAddress = 127.0.0.1
```

### Test the Connection

```bash
curl http://localhost:8080/api/system/ping
```

Expected response:
```json
{
  "status": "ok",
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## Base Configuration

| Setting | Default | Description |
|---------|---------|-------------|
| **Host** | 127.0.0.1 | Server bind address (localhost only) |
| **Port** | 8080 | HTTP port (configurable 1-65535) |
| **Base Path** | /api | All endpoints start with `/api` |
| **Content Type** | application/json | Request/response format |

## Authentication

Currently, no authentication is required. The server binds to localhost only for security.

## Common Response Format

### Success Response
```json
{
  "field1": "value1",
  "field2": "value2"
}
```

### Error Response
```json
{
  "error": "Descriptive error message"
}
```

## HTTP Status Codes

| Code | Meaning | Common Causes |
|------|---------|---------------|
| **200** | OK | Request successful |
| **400** | Bad Request | Invalid parameters, malformed JSON |
| **503** | Service Unavailable | No game loaded |
| **504** | Gateway Timeout | Command execution timeout |
| **500** | Internal Server Error | Unexpected server error |

## Rate Limiting

The API processes a maximum of 10 commands per frame (~167 commands/second at 60 FPS) to maintain emulation performance.

## Data Formats

### Memory Addresses
- **Hex Format**: `0x0000` to `0xFFFF`
- **Decimal Format**: `0` to `65535`
- **Range**: 16-bit NES address space

### Binary Data
- **Encoding**: Base64 for request/response bodies
- **Maximum Size**: 4096 bytes per operation

### Timestamps
- **Format**: ISO 8601 UTC (`2024-01-01T12:00:00Z`)

## API Categories

### [System Endpoints](api/system.md)
Health checks, version info, and server capabilities

### [Emulation Control](api/emulation.md)
Pause, resume, and status control of emulation

### [ROM Information](api/rom.md)
Metadata about the currently loaded ROM

### [Memory Access](api/memory.md)
Read and write NES memory with safety validation

### [Input Control](api/input.md)
Simulate NES controller button presses and states

### [Media Operations](api/media.md)
Screenshots and save state management

## OpenAPI Specification

Machine-readable API specification: [openapi.yaml](api/openapi.yaml)

Use this with tools like:
- Swagger UI for interactive documentation
- Postman for API testing
- Code generators for client libraries

## Architecture Notes

### Threading Model
- **REST Server**: Runs in separate thread
- **Command Queue**: Thread-safe communication
- **Emulator Thread**: Processes commands with mutex protection
- **Promise/Future**: Used for command result handling

### Memory Safety
- Write operations limited to safe regions (RAM: 0x0000-0x07FF)
- Bounds checking on all memory operations
- 4KB maximum transfer size to prevent resource exhaustion

### Error Handling
- Structured error responses with descriptive messages
- Proper HTTP status codes for different error types
- Timeout handling for long-running operations

## Troubleshooting

### Server Won't Start
1. Check if port 8080 is already in use: `netstat -ln | grep :8080`
2. Try a different port in FCEUX settings
3. Ensure FCEUX was built with `-DREST_API=ON`

### Commands Time Out
1. Verify a ROM is loaded for memory/emulation operations
2. Check emulator isn't paused when expecting responses
3. Increase timeout values if processing large data

### Invalid Responses
1. Ensure Content-Type is `application/json` for POST requests
2. Validate JSON syntax in request bodies
3. Check address formats for memory operations

## Examples and Tutorials

See [Quick Start Guide](api/quickstart.md) for step-by-step examples and common workflows.

## Contributing

When adding new endpoints:
1. Follow the existing command pattern in `RestApi/Commands/`
2. Add proper error handling and validation
3. Update this documentation with examples
4. Add the endpoint to `openapi.yaml`
5. Include tests and validation

## License

This API documentation is part of FCEUX, released under the GNU General Public License.