# FCEUX REST API Quick Start Guide

This guide helps you get started with the FCEUX REST API quickly with practical examples and common workflows.

## Prerequisites

### Build Requirements
- FCEUX compiled with REST API support
- CMake configuration: `cmake .. -DREST_API=ON`

### Runtime Requirements
- A NES ROM file for most operations
- `curl` for command-line testing
- `jq` for JSON processing (optional but recommended)

### Verify Installation
Check if REST API support is compiled in:
```bash
./build/src/fceux --help | grep -i "rest\|api"
```

## Starting the REST API Server

### Method 1: GUI (Recommended)
1. Launch FCEUX: `./build/src/fceux`
2. Load a ROM file: **File → Open ROM**
3. Enable API server: **Tools → REST API Server**
4. Server starts on `http://127.0.0.1:8080`

### Method 2: Configuration File
Add to FCEUX config file:
```ini
SDL.RestApiEnabled = 1
SDL.RestApiPort = 8080
SDL.RestApiBindAddress = 127.0.0.1
```

### Method 3: Command Line (Future)
```bash
# Will be available in future versions
./build/src/fceux --rest-api --rest-port 8080 game.nes
```

## First API Call

Test if the server is running:
```bash
curl http://localhost:8080/api/system/ping
```

Expected response:
```json
{
  "status": "ok",
  "timestamp": "2025-07-08T12:00:00Z"
}
```

## Quick Reference

### Essential Commands
```bash
# Health check
curl http://localhost:8080/api/system/ping

# Get server info
curl http://localhost:8080/api/system/info

# Check emulation status
curl http://localhost:8080/api/emulation/status

# Pause emulation
curl -X POST http://localhost:8080/api/emulation/pause

# Resume emulation  
curl -X POST http://localhost:8080/api/emulation/resume
```

### Memory Operations
```bash
# Read single byte
curl http://localhost:8080/api/memory/0x0000

# Read 256 bytes from start of RAM
curl http://localhost:8080/api/memory/range/0x0000/256

# Write data to memory (base64 encoded)
curl -X POST http://localhost:8080/api/memory/range/0x0000 \
  -H "Content-Type: application/json" \
  -d '{"data": "AAECAwQFBgcICQoLDA0ODw=="}'
```

### Input Control
```bash
# Press A button
curl -X POST http://localhost:8080/api/input/port/1/press \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["A"]}'

# Jump (A + UP)
curl -X POST http://localhost:8080/api/input/port/1/press \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["A", "UP"], "duration_ms": 100}'

# Release all buttons
curl -X POST http://localhost:8080/api/input/port/1/release
```

### Screenshots & Save States
```bash
# Take screenshot
curl -X POST http://localhost:8080/api/screenshot

# Create save state
curl -X POST http://localhost:8080/api/savestate

# Load save state
curl -X POST http://localhost:8080/api/loadstate
```

## Common Workflows

### 1. Basic Game Automation

```bash
#!/bin/bash
# basic_automation.sh - Simple game automation example

echo "Starting game automation..."

# Check if game is loaded
ROM_STATUS=$(curl -s http://localhost:8080/api/rom/info | jq -r '.loaded')
if [ "$ROM_STATUS" != "true" ]; then
    echo "Error: No ROM loaded. Please load a game first."
    exit 1
fi

# Resume emulation if paused
curl -s -X POST http://localhost:8080/api/emulation/resume

# Wait for game to start
sleep 2

# Press START to begin game
curl -s -X POST http://localhost:8080/api/input/port/1/press \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["START"], "duration_ms": 100}'

echo "Game started! Press Ctrl+C to stop automation."

# Simple automation loop
while true; do
    # Jump occasionally
    curl -s -X POST http://localhost:8080/api/input/port/1/press \
      -H "Content-Type: application/json" \
      -d '{"buttons": ["A"], "duration_ms": 50}'
    
    sleep 2
    
    # Move right
    curl -s -X POST http://localhost:8080/api/input/port/1/press \
      -H "Content-Type: application/json" \
      -d '{"buttons": ["RIGHT"], "duration_ms": 500}'
    
    sleep 1
done
```

### 2. Memory Monitoring

```bash
#!/bin/bash
# memory_monitor.sh - Monitor specific memory locations

echo "Monitoring memory locations..."

# Function to read and display memory
monitor_memory() {
    local addr=$1
    local name=$2
    
    local result=$(curl -s http://localhost:8080/api/memory/$addr)
    local value=$(echo $result | jq -r '.value')
    local hex=$(echo $result | jq -r '.hex')
    
    printf "%-15s: %3d (%s)\n" "$name" "$value" "$hex"
}

# Monitor loop
while true; do
    clear
    echo "=== NES Memory Monitor ==="
    echo "Press Ctrl+C to stop"
    echo
    
    # Common memory locations (examples for Super Mario Bros)
    monitor_memory "0x000E" "Lives"
    monitor_memory "0x0075" "Score (high)"
    monitor_memory "0x0076" "Score (mid)"
    monitor_memory "0x0077" "Score (low)"
    monitor_memory "0x001D" "Timer (hundreds)"
    monitor_memory "0x001E" "Timer (tens)"
    monitor_memory "0x001F" "Timer (ones)"
    
    sleep 0.5
done
```

### 3. Screenshot Time-lapse

```bash
#!/bin/bash
# screenshot_timelapse.sh - Create time-lapse screenshots

SCREENSHOT_DIR="/tmp/fceux_timelapse"
mkdir -p "$SCREENSHOT_DIR"

echo "Creating time-lapse screenshots in $SCREENSHOT_DIR"
echo "Press Ctrl+C to stop"

COUNTER=1

while true; do
    # Generate filename with zero-padded counter
    FILENAME=$(printf "frame_%06d.png" $COUNTER)
    
    # Capture screenshot
    curl -s -X POST http://localhost:8080/api/screenshot \
      -H "Content-Type: application/json" \
      -d "{\"path\": \"$SCREENSHOT_DIR/$FILENAME\", \"format\": \"png\"}"
    
    if [ $? -eq 0 ]; then
        echo "Captured: $FILENAME"
    else
        echo "Failed to capture: $FILENAME"
    fi
    
    COUNTER=$((COUNTER + 1))
    sleep 1  # Capture every second
done

echo
echo "To create video from screenshots:"
echo "ffmpeg -r 30 -i $SCREENSHOT_DIR/frame_%06d.png -c:v libx264 -pix_fmt yuv420p timelapse.mp4"
```

### 4. Save State Backup System

```bash
#!/bin/bash
# save_backup.sh - Automated save state backup

BACKUP_DIR="/tmp/fceux_saves"
mkdir -p "$BACKUP_DIR"

echo "Starting save state backup system..."

# Function to create timestamped backup
create_backup() {
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local rom_info=$(curl -s http://localhost:8080/api/rom/info)
    local rom_name=$(echo $rom_info | jq -r '.name // "unknown"')
    
    # Clean ROM name for filename
    rom_name=$(echo "$rom_name" | tr ' ' '_' | tr '[:upper:]' '[:lower:]')
    
    local backup_path="$BACKUP_DIR/${rom_name}_${timestamp}.fc0"
    
    # Create save state
    curl -s -X POST http://localhost:8080/api/savestate \
      -H "Content-Type: application/json" \
      -d "{\"path\": \"$backup_path\"}"
    
    if [ $? -eq 0 ]; then
        echo "Backup created: $(basename $backup_path)"
    else
        echo "Failed to create backup"
    fi
}

# Create backups every 5 minutes
while true; do
    # Check if game is running
    STATUS=$(curl -s http://localhost:8080/api/emulation/status | jq -r '.running')
    
    if [ "$STATUS" = "true" ]; then
        create_backup
    else
        echo "Game not running, skipping backup"
    fi
    
    sleep 300  # 5 minutes
done
```

### 5. Input Sequence Playback

```bash
#!/bin/bash
# input_playback.sh - Play back recorded input sequences

# Example: Konami Code sequence
konami_code() {
    echo "Executing Konami Code..."
    
    local sequence=("UP" "UP" "DOWN" "DOWN" "LEFT" "RIGHT" "LEFT" "RIGHT" "B" "A")
    
    for button in "${sequence[@]}"; do
        echo "Pressing: $button"
        curl -s -X POST http://localhost:8080/api/input/port/1/press \
          -H "Content-Type: application/json" \
          -d "{\"buttons\": [\"$button\"], \"duration_ms\": 100}"
        
        sleep 0.2  # Brief pause between inputs
    done
    
    echo "Konami Code complete!"
}

# Example: Mario level 1-1 speed run sequence
mario_speedrun() {
    echo "Mario 1-1 speedrun sequence..."
    
    # Start game
    curl -s -X POST http://localhost:8080/api/input/port/1/press \
      -d '{"buttons": ["START"], "duration_ms": 100}'
    
    sleep 2
    
    # Run right and jump sequence
    for i in {1..10}; do
        # Hold right and jump
        curl -s -X POST http://localhost:8080/api/input/port/1/press \
          -d '{"buttons": ["RIGHT", "A"], "duration_ms": 200}'
        
        sleep 0.5
        
        # Just run right
        curl -s -X POST http://localhost:8080/api/input/port/1/press \
          -d '{"buttons": ["RIGHT"], "duration_ms": 300}'
        
        sleep 0.3
    done
}

# Menu
echo "Input Sequence Playback"
echo "1) Konami Code"
echo "2) Mario 1-1 Speedrun"
echo "Enter choice (1-2): "
read choice

case $choice in
    1) konami_code ;;
    2) mario_speedrun ;;
    *) echo "Invalid choice" ;;
esac
```

## API Testing with Different Tools

### Using curl (Command Line)
```bash
# Basic GET request
curl http://localhost:8080/api/system/info

# POST with JSON data
curl -X POST http://localhost:8080/api/emulation/pause \
  -H "Content-Type: application/json"

# Save response to file
curl http://localhost:8080/api/memory/range/0x0000/256 > memory_dump.json

# Follow redirects and show headers
curl -L -i http://localhost:8080/api/rom/info
```

### Using wget
```bash
# Simple GET
wget -qO- http://localhost:8080/api/system/ping

# POST data
wget --post-data='{"buttons": ["A"]}' \
  --header='Content-Type: application/json' \
  -qO- http://localhost:8080/api/input/port/1/press
```

### Using HTTPie (if available)
```bash
# Install: pip install httpie

# GET request
http localhost:8080/api/system/info

# POST request
http POST localhost:8080/api/input/port/1/press buttons:='["A","B"]' duration_ms:=100

# Upload data
echo '{"data": "AAECAwQFBgc="}' | http POST localhost:8080/api/memory/range/0x0000
```

### Using Python
```python
#!/usr/bin/env python3
# api_test.py - Python API client example

import requests
import json
import time

BASE_URL = "http://localhost:8080"

def api_get(endpoint):
    """Make GET request to API"""
    response = requests.get(f"{BASE_URL}{endpoint}")
    return response.json()

def api_post(endpoint, data=None):
    """Make POST request to API"""
    headers = {"Content-Type": "application/json"}
    response = requests.post(f"{BASE_URL}{endpoint}", 
                           json=data, headers=headers)
    return response.json()

# Test API connection
try:
    ping = api_get("/api/system/ping")
    print(f"Server status: {ping['status']}")
    
    # Get ROM info
    rom_info = api_get("/api/rom/info")
    if rom_info['loaded']:
        print(f"ROM loaded: {rom_info['name']}")
    else:
        print("No ROM loaded")
    
    # Press A button
    result = api_post("/api/input/port/1/press", 
                     {"buttons": ["A"], "duration_ms": 100})
    print(f"Button press result: {result}")
    
except requests.exceptions.ConnectionError:
    print("Error: Cannot connect to FCEUX API server")
    print("Make sure FCEUX is running with REST API enabled")
```

## Troubleshooting

### Server Not Responding
```bash
# Check if server is running
curl -w "%{http_code}" http://localhost:8080/api/system/ping

# Check if port is in use
netstat -ln | grep :8080
lsof -i :8080

# Check FCEUX process
ps aux | grep fceux
```

### Common Error Responses

**503 Service Unavailable - No game loaded**
```bash
# Solution: Load a ROM first
# Check ROM status:
curl http://localhost:8080/api/rom/info
```

**400 Bad Request - Invalid parameters**
```bash
# Common causes:
# - Invalid JSON format
# - Wrong button names (case-sensitive)
# - Invalid memory addresses
# - Missing required fields

# Debug: Check response headers
curl -i http://localhost:8080/api/input/port/1/press \
  -d '{"buttons": ["invalid_button"]}'
```

**504 Gateway Timeout**
```bash
# Usually indicates emulator is busy or frozen
# Check emulation status:
curl http://localhost:8080/api/emulation/status
```

### Performance Tips

**Memory Operations**
- Use range reads for > 10 bytes (332x faster)
- Batch operations when possible
- Avoid unnecessary polling

**Input Operations**
- Use state endpoint for complex patterns
- Reasonable timing between inputs
- Release buttons when done

**Screenshots**
- PNG for quality, JPG for size
- Use base64 encoding for web apps
- File encoding for local storage

## Next Steps

1. **Explore the full API**: See [REST API Documentation](../REST_API.md)
2. **Category-specific guides**:
   - [System endpoints](system.md)
   - [Emulation control](emulation.md)
   - [Memory access](memory.md)
   - [Input control](input.md)
   - [Media operations](media.md)
3. **Advanced topics**:
   - [OpenAPI specification](openapi.yaml)
   - Error handling patterns
   - Performance optimization
   - Client library development

## Example Projects

### Game AI Training
Use the API to train AI agents:
- Memory monitoring for game state
- Input simulation for actions
- Screenshot capture for visual processing
- Save states for episode resets

### Automated Testing
Create test suites for ROM hacks:
- Input sequence verification
- Memory state validation
- Screenshot comparison testing
- Regression testing workflows

### Speedrun Tools
Build speedrun assistance tools:
- Split timing with memory triggers
- Input display and recording
- Save state practice segments
- Performance analysis

### Research Applications
Academic and research uses:
- Game state analysis
- Player behavior simulation
- Algorithm validation
- Data collection frameworks

Ready to start? Load a ROM, enable the REST API, and try your first API call!