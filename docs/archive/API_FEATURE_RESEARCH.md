# FCEUX REST API Implementation Guide (Linux)

## Table of Contents
1. [Overview](#1-overview)
2. [Architecture Analysis](#2-architecture-analysis)
3. [Key C++ Functions and Lua Mappings](#3-key-c-functions-and-lua-mappings)
4. [Linux-Specific Implementation Strategy](#4-linux-specific-implementation-strategy)
5. [Phased Implementation Plan](#5-phased-implementation-plan)
6. [Complete Code Examples](#6-complete-code-examples)
7. [Build Integration](#7-build-integration)
8. [Testing Guide](#8-testing-guide)

## 1. Overview

This guide provides a comprehensive approach to adding REST API capabilities to FCEUX (Linux version), bypassing the Lua engine entirely for direct C++ integration. With the constraints of Linux-only support and guaranteed single API call serialization, this becomes a **medium-low difficulty** project.

### Key Simplifications
- **Linux-only**: Use Qt/SDL version exclusively
- **Single API call guarantee**: Simple mutex-based synchronization
- **No command queue needed**: Direct function calls
- **No complex thread safety**: One mutex protects everything

## 2. Architecture Analysis

### Current FCEUX Architecture
```
┌─────────────────┐      ┌──────────────────┐      ┌─────────────────┐
│   Lua Script    │      │  Lua Bindings    │      │  FCEUX Core     │
│                 │      │ (lua-engine.cpp) │      │   Emulator      │
│ memory.readbyte │ ---> │ memory_readbyte  │ ---> │    GetMem()     │
│ joypad.write   │ ---> │ joypad_write     │ ---> │  joy[player]    │
│ gui.screenshot │ ---> │ gui_savescreenshot│---> │ FCEUI_SaveSnapshot│
└─────────────────┘      └──────────────────┘      └─────────────────┘
```

### New REST API Architecture
```
┌─────────────────┐      ┌──────────────────┐      ┌─────────────────┐
│   REST Client   │      │   REST Server    │      │  FCEUX Core     │
│                 │      │  (rest_api.cpp)  │      │   Emulator      │
│ GET /memory/100│ ---> │ mutex.lock()     │ ---> │    GetMem()     │
│ POST /input/1  │ ---> │ Direct calls     │ ---> │  joy[player]    │
│ POST /screenshot│ ---> │ mutex.unlock()   │ ---> │ FCEUI_SaveSnapshot│
└─────────────────┘      └──────────────────┘      └─────────────────┘
```

## 3. Key C++ Functions and Lua Mappings

### Memory Operations
```cpp
// Lua: memory.readbyte(address)
uint8 GetMem(uint16 address);  // Read from CPU memory space

// Lua: memory.writebyte(address, value)  
FCEU_CheatSetByte(uint32 A, uint8 V);  // Safe write for RAM
BWrite[address](address, value);        // Write using proper handler

// Lua: memory.readbyterange(address, size)
// Direct implementation - no single function exists
for(int i = 0; i < size; i++) {
    buffer[i] = GetMem(address + i);
}
```

### Input Control
```cpp
// Global controller state array
extern uint8 joy[4];  // Players 0-3

// Button bit positions (for joy[] array)
#define JOY_A      0x01  // Bit 0
#define JOY_B      0x02  // Bit 1  
#define JOY_SELECT 0x04  // Bit 2
#define JOY_START  0x08  // Bit 3
#define JOY_UP     0x10  // Bit 4
#define JOY_DOWN   0x20  // Bit 5
#define JOY_LEFT   0x40  // Bit 6
#define JOY_RIGHT  0x80  // Bit 7

// Lua: joypad.write(player, buttons)
// Direct implementation:
joy[player] = buttonState;
```

### Emulator Control
```cpp
// Lua: emu.pause() / emu.unpause()
FCEUI_ToggleEmulationPause();
bool FCEUI_EmulationPaused();

// Lua: emu.frameadvance()
FCEUI_FrameAdvance();  // Qt version has this

// Lua: emu.message(msg)
FCEU_DispMessage(const char* format, int disppos, ...);

// Lua: emu.loadrom(filename)
LoadGame(const char* path, bool silent = false);
```

### Graphics Operations
```cpp
// Lua: gui.savescreenshot()
FCEUI_SaveSnapshot();

// Lua: gui.savescreenshotas(filename)
FCEUI_SetSnapshotAsName(const char* name);
FCEUI_SaveSnapshotAs();

// Lua: emu.getscreenpixel(x, y, true)
// Need to access XBuf (video buffer) directly
```

### Save State Operations
```cpp
// Lua: savestate.save(object)
FCEUI_SaveState(const char *fname, bool display_message = true);

// Lua: savestate.load(object)  
FCEUI_LoadState(const char *fname, bool display_message = true);

// For slot-based saves (0-9)
FCEUI_SelectState(int slot, bool display_message = true);
FCEUI_SaveState(NULL);  // Saves to selected slot
FCEUI_LoadState(NULL);  // Loads from selected slot
```

## 4. Linux-Specific Implementation Strategy

### Key Advantages of Linux/Qt Build
1. **Clean CMake build system** - No Visual Studio complexity
2. **Modern codebase** - Qt version is actively maintained
3. **Better separation** - GUI and emulation are already somewhat separated
4. **Single platform** - No ifdef hell for platform-specific code

### Simplified Threading Model
```cpp
class FCEUXRestServer {
private:
    httplib::Server server;
    std::thread server_thread;
    std::mutex api_mutex;  // ONE mutex for everything!
    
public:
    template<typename Func>
    auto ExecuteWithLock(Func func) -> decltype(func()) {
        std::lock_guard<std::mutex> lock(api_mutex);
        return func();
    }
};
```

### Key Files in Qt/SDL Build
- `src/drivers/Qt/fceuWrapper.cpp` - Main emulation wrapper
- `src/drivers/Qt/main.cpp` - Application entry point
- `src/input.cpp` - Core input handling
- `src/fceu.cpp` - Core emulator functions
- `src/x6502.cpp` - CPU emulation (memory access)

## 5. Phased Implementation Plan

### Phase 1: Memory Operations (1-2 days)
**Endpoints:**
```
GET  /memory/{address}
GET  /memory/range/{start}/{size}
POST /memory/{address}
```

**Why First:** No timing issues, easy to test, learn codebase

### Phase 2: Basic Emulator Control (1 day)
**Endpoints:**
```
GET  /emulator/status
POST /emulator/pause
POST /emulator/unpause
POST /emulator/reset
```

**Why Second:** Needed for testing other features

### Phase 3: Controller Input (2-3 days)
**Endpoints:**
```
POST /input/controller/{player}
GET  /input/controller/{player}
POST /input/controller/{player}/raw
```

**Why Third:** Visual feedback, fun to test

### Phase 4: Frame Control (2-3 days)
**Endpoints:**
```
POST /emulator/frame-advance
POST /emulator/frame-advance/{count}
GET  /emulator/framecount
```

**Why Fourth:** More complex but critical for automation

### Phase 5: Screenshots & Visuals (1-2 days)
**Endpoints:**
```
POST /screenshot
GET  /screenshot/base64
GET  /screen/pixel/{x}/{y}
```

### Phase 6: Save States (2-3 days)
**Endpoints:**
```
POST /savestate/save/{slot}
POST /savestate/load/{slot}
GET  /savestate/list
DELETE /savestate/{slot}
```

**Total Time Estimate:** 2-3 weeks for full implementation

## 6. Complete Code Examples

### rest_api.h
```cpp
#ifndef REST_API_H
#define REST_API_H

void InitializeRestAPI(int port = 8080);
void ShutdownRestAPI();
bool IsRestAPIRunning();

#endif
```

### rest_api.cpp (Complete Phase 1-3 Implementation)
```cpp
#include "../../fceu.h"
#include "../../x6502.h"
#include "../../input.h"
#include "../../state.h"
#include "../../movie.h"
#include "../../debug.h"
#include "fceuWrapper.h"
#include "httplib.h"
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static httplib::Server* rest_server = nullptr;
static std::thread* server_thread = nullptr;
static std::mutex api_mutex;
static bool server_running = false;

// Helper function for JSON responses
void JsonResponse(httplib::Response& res, const json& data) {
    res.set_content(data.dump(), "application/json");
}

void InitializeRestAPI(int port) {
    if (server_running) return;
    
    rest_server = new httplib::Server();
    
    // ===== PHASE 1: MEMORY OPERATIONS =====
    
    // GET /memory/{address}
    rest_server->Get(R"(/memory/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(api_mutex);
        
        try {
            int addr = std::stoi(req.matches[1]);
            if (addr < 0 || addr > 0xFFFF) {
                res.status = 400;
                JsonResponse(res, {{"error", "Invalid address"}});
                return;
            }
            
            uint8 value = GetMem(addr);
            JsonResponse(res, {{"address", addr}, {"value", value}});
            
        } catch (const std::exception& e) {
            res.status = 400;
            JsonResponse(res, {{"error", e.what()}});
        }
    });
    
    // GET /memory/range/{start}/{size}
    rest_server->Get(R"(/memory/range/(\d+)/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(api_mutex);
        
        try {
            int start = std::stoi(req.matches[1]);
            int size = std::stoi(req.matches[2]);
            
            if (start < 0 || start > 0xFFFF || size < 1 || (start + size) > 0x10000) {
                res.status = 400;
                JsonResponse(res, {{"error", "Invalid range"}});
                return;
            }
            
            std::vector<int> values;
            for (int i = 0; i < size; i++) {
                values.push_back(GetMem(start + i));
            }
            
            JsonResponse(res, {
                {"start", start},
                {"size", size},
                {"values", values}
            });
            
        } catch (const std::exception& e) {
            res.status = 400;
            JsonResponse(res, {{"error", e.what()}});
        }
    });
    
    // POST /memory/{address}
    rest_server->Post(R"(/memory/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(api_mutex);
        
        try {
            int addr = std::stoi(req.matches[1]);
            json body = json::parse(req.body);
            int value = body["value"];
            
            if (addr < 0 || addr > 0xFFFF || value < 0 || value > 255) {
                res.status = 400;
                JsonResponse(res, {{"error", "Invalid address or value"}});
                return;
            }
            
            // Use FCEU_CheatSetByte for RAM writes
            FCEU_CheatSetByte(addr, value);
            
            JsonResponse(res, {{"success", true}});
            
        } catch (const std::exception& e) {
            res.status = 400;
            JsonResponse(res, {{"error", e.what()}});
        }
    });
    
    // ===== PHASE 2: EMULATOR CONTROL =====
    
    // GET /emulator/status
    rest_server->Get("/emulator/status", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(api_mutex);
        
        bool rom_loaded = (GameInfo != nullptr);
        bool paused = FCEUI_EmulationPaused();
        int frame_count = FCEUMOV_GetFrame();
        
        JsonResponse(res, {
            {"rom_loaded", rom_loaded},
            {"paused", paused},
            {"frame_count", frame_count},
            {"rom_name", rom_loaded ? GameInfo->filename : ""}
        });
    });
    
    // POST /emulator/pause
    rest_server->Post("/emulator/pause", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(api_mutex);
        
        if (!FCEUI_EmulationPaused()) {
            FCEUI_ToggleEmulationPause();
        }
        JsonResponse(res, {{"paused", true}});
    });
    
    // POST /emulator/unpause
    rest_server->Post("/emulator/unpause", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(api_mutex);
        
        if (FCEUI_EmulationPaused()) {
            FCEUI_ToggleEmulationPause();
        }
        JsonResponse(res, {{"paused", false}});
    });
    
    // POST /emulator/reset
    rest_server->Post("/emulator/reset", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(api_mutex);
        
        ResetNES();
        JsonResponse(res, {{"success", true}});
    });
    
    // ===== PHASE 3: CONTROLLER INPUT =====
    
    // POST /input/controller/{player}
    rest_server->Post(R"(/input/controller/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(api_mutex);
        
        try {
            int player = std::stoi(req.matches[1]) - 1;  // API uses 1-based
            if (player < 0 || player > 3) {
                res.status = 400;
                JsonResponse(res, {{"error", "Invalid player number (1-4)"}});
                return;
            }
            
            json body = json::parse(req.body);
            uint8 buttons = 0;
            
            // Parse button states
            if (body.contains("A") && body["A"]) buttons |= 0x01;
            if (body.contains("B") && body["B"]) buttons |= 0x02;
            if (body.contains("select") && body["select"]) buttons |= 0x04;
            if (body.contains("start") && body["start"]) buttons |= 0x08;
            if (body.contains("up") && body["up"]) buttons |= 0x10;
            if (body.contains("down") && body["down"]) buttons |= 0x20;
            if (body.contains("left") && body["left"]) buttons |= 0x40;
            if (body.contains("right") && body["right"]) buttons |= 0x80;
            
            // Set controller state
            extern uint8 joy[4];
            joy[player] = buttons;
            
            JsonResponse(res, {{"success", true}, {"buttons", buttons}});
            
        } catch (const std::exception& e) {
            res.status = 400;
            JsonResponse(res, {{"error", e.what()}});
        }
    });
    
    // GET /input/controller/{player}
    rest_server->Get(R"(/input/controller/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(api_mutex);
        
        try {
            int player = std::stoi(req.matches[1]) - 1;
            if (player < 0 || player > 3) {
                res.status = 400;
                JsonResponse(res, {{"error", "Invalid player number (1-4)"}});
                return;
            }
            
            extern uint8 joy[4];
            uint8 state = joy[player];
            
            JsonResponse(res, {
                {"player", player + 1},
                {"raw", state},
                {"buttons", {
                    {"A", (state & 0x01) != 0},
                    {"B", (state & 0x02) != 0},
                    {"select", (state & 0x04) != 0},
                    {"start", (state & 0x08) != 0},
                    {"up", (state & 0x10) != 0},
                    {"down", (state & 0x20) != 0},
                    {"left", (state & 0x40) != 0},
                    {"right", (state & 0x80) != 0}
                }}
            });
            
        } catch (const std::exception& e) {
            res.status = 400;
            JsonResponse(res, {{"error", e.what()}});
        }
    });
    
    // POST /input/controller/{player}/raw
    rest_server->Post(R"(/input/controller/(\d+)/raw)", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(api_mutex);
        
        try {
            int player = std::stoi(req.matches[1]) - 1;
            json body = json::parse(req.body);
            int value = body["value"];
            
            if (player < 0 || player > 3 || value < 0 || value > 255) {
                res.status = 400;
                JsonResponse(res, {{"error", "Invalid player or value"}});
                return;
            }
            
            extern uint8 joy[4];
            joy[player] = value;
            
            JsonResponse(res, {{"success", true}});
            
        } catch (const std::exception& e) {
            res.status = 400;
            JsonResponse(res, {{"error", e.what()}});
        }
    });
    
    // Start server in background thread
    server_thread = new std::thread([port]() {
        server_running = true;
        rest_server->listen("localhost", port);
        server_running = false;
    });
    
    printf("REST API server started on port %d\n", port);
}

void ShutdownRestAPI() {
    if (rest_server) {
        rest_server->stop();
        if (server_thread) {
            server_thread->join();
            delete server_thread;
            server_thread = nullptr;
        }
        delete rest_server;
        rest_server = nullptr;
    }
}

bool IsRestAPIRunning() {
    return server_running;
}
```

## 7. Build Integration

### Step 1: Download Dependencies
```bash
# Get httplib (header-only)
cd fceux/src
wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h

# Get nlohmann/json (header-only)
mkdir -p nlohmann
wget https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp -O nlohmann/json.hpp
```

### Step 2: Modify CMakeLists.txt
```cmake
# In src/CMakeLists.txt, find the Qt driver section and add:
if (FCEUX_SDL_OPENGL AND ${Qt5OpenGL_FOUND})
    set(SRC_DRIVERS_QT
        ${SRC_DRIVERS_QT}
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/Qt/rest_api.cpp
    )
endif()

# Add C++11 support
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### Step 3: Integrate with fceuWrapper.cpp
```cpp
// In src/drivers/Qt/fceuWrapper.cpp

// Add at top
#include "rest_api.h"

// In LoadGame function, after successful load:
int LoadGame(const char *path, bool silent)
{
    // ... existing code ...
    
    if (GameInfo) {
        // Start REST API server
        InitializeRestAPI(8080);
    }
    
    return 1;
}

// In CloseGame function:
int CloseGame(void)
{
    // ... existing code ...
    
    // Stop REST API server
    ShutdownRestAPI();
    
    return 0;
}
```

### Step 4: Build FCEUX
```bash
cd fceux
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 8. Testing Guide

### Phase 1 Testing (Memory)
```bash
# Start FCEUX and load Super Mario Bros

# Read Mario's X position
curl http://localhost:8080/memory/117

# Write Mario's X position
curl -X POST -H "Content-Type: application/json" \
  -d '{"value": 128}' http://localhost:8080/memory/117

# Read multiple bytes
curl http://localhost:8080/memory/range/0/256
```

### Phase 2 Testing (Emulator Control)
```bash
# Check status
curl http://localhost:8080/emulator/status

# Pause emulation
curl -X POST http://localhost:8080/emulator/pause

# Reset console
curl -X POST http://localhost:8080/emulator/reset
```

### Phase 3 Testing (Input)
```bash
# Press A and Right for Player 1
curl -X POST -H "Content-Type: application/json" \
  -d '{"A": true, "right": true}' http://localhost:8080/input/controller/1

# Read current input state
curl http://localhost:8080/input/controller/1

# Set raw input value
curl -X POST -H "Content-Type: application/json" \
  -d '{"value": 129}' http://localhost:8080/input/controller/1/raw
```

### Python Test Script
```python
import requests
import json
import time

BASE_URL = "http://localhost:8080"

# Test memory operations
def test_memory():
    # Read memory
    r = requests.get(f"{BASE_URL}/memory/117")
    print(f"Mario X position: {r.json()['value']}")
    
    # Write memory
    r = requests.post(f"{BASE_URL}/memory/117", 
                      json={"value": 200})
    print(f"Write result: {r.json()}")

# Test input
def test_input():
    # Press right for 1 second
    requests.post(f"{BASE_URL}/input/controller/1",
                  json={"right": True})
    time.sleep(1)
    
    # Release all buttons
    requests.post(f"{BASE_URL}/input/controller/1",
                  json={})

if __name__ == "__main__":
    test_memory()
    test_input()
```

## Next Steps

After implementing Phases 1-3:

1. **Phase 4**: Add frame advance functionality
2. **Phase 5**: Implement screenshot capture
3. **Phase 6**: Add save state management
4. **Optimization**: Add request queuing for better performance
5. **Security**: Add authentication/authorization
6. **Documentation**: Generate OpenAPI/Swagger docs

## Troubleshooting

### Common Issues

1. **Build fails with httplib errors**
   - Make sure you have C++11 enabled
   - Check that httplib.h is in the correct location

2. **Server doesn't start**
   - Check if port 8080 is already in use
   - Ensure REST API initialization is called after ROM load

3. **Memory writes don't work**
   - Some addresses are read-only (ROM areas)
   - Use FCEU_CheatSetByte for RAM areas (0x0000-0x07FF)

4. **Input doesn't affect game**
   - Ensure game is unpaused
   - Check that correct player number is used (1-4, not 0-3)

## Conclusion

This implementation provides a clean, efficient REST API for FCEUX on Linux. The single-threaded guarantee with mutex protection makes it much simpler than a full thread-safe implementation while still providing all necessary functionality. The phased approach allows for incremental development and testing, reducing risk and complexity.