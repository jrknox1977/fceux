# System Endpoints

System endpoints provide health checks, version information, and server capabilities.

## GET /api/system/info

**Description**: Get FCEUX version and system information

**Parameters**: None

**Request Example**:
```bash
curl -X GET http://localhost:8080/api/system/info
```

**Response**:
```json
{
  "version": "2.6.6",
  "build_date": "Jul  8 2025",
  "qt_version": "5.15.3",
  "api_version": "1.0.0",
  "platform": "linux"
}
```

**Response Fields**:
- `version`: FCEUX version string
- `build_date`: Compilation date
- `qt_version`: Qt framework version
- `api_version`: REST API version
- `platform`: Operating system (linux/windows/macos/unknown)

**Status Codes**:
- `200 OK`: Always successful

**Notes**:
- No authentication required
- Available even without a loaded ROM
- Useful for version compatibility checks

---

## GET /api/system/ping

**Description**: Health check endpoint for monitoring server availability

**Parameters**: None

**Request Example**:
```bash
curl -X GET http://localhost:8080/api/system/ping
```

**Response**:
```json
{
  "status": "ok",
  "timestamp": "2025-07-08T10:30:45Z"
}
```

**Response Fields**:
- `status`: Always "ok" when server is responsive
- `timestamp`: Current UTC time in ISO 8601 format

**Status Codes**:
- `200 OK`: Server is healthy and responsive

**Notes**:
- Fastest endpoint for health monitoring
- No game state dependencies
- Use for uptime monitoring and load balancers

---

## GET /api/system/capabilities

**Description**: List all available API endpoints and features

**Parameters**: None

**Request Example**:
```bash
curl -X GET http://localhost:8080/api/system/capabilities
```

**Response**:
```json
{
  "endpoints": [
    "/api/system/info",
    "/api/system/ping",
    "/api/system/capabilities",
    "/api/emulation/pause",
    "/api/emulation/resume",
    "/api/emulation/status",
    "/api/rom/info",
    "/api/memory/{address}",
    "/api/memory/range/{start}/{length}",
    "/api/memory/range/{start}",
    "/api/memory/batch",
    "/api/input/status",
    "/api/input/port/{port}/press",
    "/api/input/port/{port}/release",
    "/api/input/port/{port}/state",
    "/api/screenshot",
    "/api/screenshot/last",
    "/api/savestate",
    "/api/loadstate",
    "/api/savestate/list"
  ],
  "features": {
    "emulation_control": true,
    "memory_access": true,
    "memory_range_access": true,
    "input_control": true,
    "save_states": true,
    "screenshots": true
  }
}
```

**Response Fields**:
- `endpoints`: Array of all available endpoint paths
- `features`: Object with feature flags indicating capabilities

**Feature Flags**:
- `emulation_control`: Can pause/resume emulation
- `memory_access`: Can read/write individual memory bytes
- `memory_range_access`: Can read/write memory ranges efficiently
- `input_control`: Can simulate controller input
- `save_states`: Can create and load save states
- `screenshots`: Can capture emulator output

**Status Codes**:
- `200 OK`: Always successful

**Notes**:
- Use for API discovery and client feature detection
- Endpoint paths include parameter placeholders (e.g., `{address}`)
- Feature flags useful for conditional client functionality

## Error Handling

System endpoints are highly reliable and rarely fail. However, potential issues include:

**Network Connectivity**:
- Connection refused: Server not running
- Timeout: Server overloaded or network issues

**Server Issues**:
- 500 Internal Server Error: Rare Qt or system-level issues

## Rate Limiting

System endpoints are not subject to the 10 commands/frame limit as they don't interact with emulation state.

## Usage Examples

### Version Compatibility Check
```bash
#!/bin/bash
VERSION=$(curl -s http://localhost:8080/api/system/info | jq -r '.version')
if [[ "$VERSION" < "2.6.0" ]]; then
    echo "FCEUX version $VERSION not supported"
    exit 1
fi
```

### Health Monitoring Script
```bash
#!/bin/bash
while true; do
    if curl -s -f http://localhost:8080/api/system/ping > /dev/null; then
        echo "$(date): Server healthy"
    else
        echo "$(date): Server down!"
    fi
    sleep 30
done
```

### Feature Detection
```bash
# Check if memory range access is available
FEATURES=$(curl -s http://localhost:8080/api/system/capabilities | jq '.features.memory_range_access')
if [ "$FEATURES" = "true" ]; then
    echo "Using efficient range access"
else
    echo "Falling back to single-byte access"
fi
```