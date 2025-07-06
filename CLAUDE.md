# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview
FCEUX is a cross-platform NES emulator with Qt GUI. The codebase is written primarily in C++ with CMake build system. This fork is focused on Linux development and adding REST API capabilities.

## Build Commands

### Standard Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc)
```

### Debug Build with AddressSanitizer
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DASAN=ON
make -j$(nproc)
```

### Qt6 Build (if Qt5 not available)
```bash
cmake .. -DQT6=ON -DCMAKE_BUILD_TYPE=Release
```

### Run the Emulator
```bash
./build/src/fceux
```

### Install to System
```bash
sudo make install
```

### Static Analysis
```bash
./scripts/runCppCheck.sh
```

## Architecture

### Threading Model
FCEUX uses a multi-threaded architecture:
- **Main Thread**: Qt GUI event loop, handles user interaction
- **Emulator Thread**: Runs NES emulation (`emulatorThread_t` in `ConsoleWindow.cpp`)
- Communication via Qt signals/slots and shared memory (`nes_shm`)

### Core Emulation Components
- **CPU**: `src/x6502.cpp` - 6502 processor emulation
- **PPU**: `src/ppu.cpp` - Picture Processing Unit (graphics)
- **APU**: `src/sound.cpp` - Audio Processing Unit
- **Memory**: `src/fceu.cpp` - Main emulation core and memory mapping
- **Mappers**: `src/boards/` - Cartridge mapper implementations (100+ supported)

### Frontend Architecture
- **Main Window**: `src/drivers/Qt/ConsoleWindow.cpp` - Central GUI controller
- **Emulation Thread**: `src/drivers/Qt/ConsoleViewerGL.cpp` - Runs emulation in separate thread
- **Wrapper**: `src/drivers/Qt/fceuWrapper.cpp` - Thread-safe interface between GUI and core
- **Input System**: `src/drivers/Qt/input.cpp` - Handles keyboard/gamepad input
- **Config System**: `src/drivers/Qt/config.cpp` - Settings management

### Key Integration Points
- **Command Processing**: `fceuWrapperUpdate()` in `fceuWrapper.cpp` - Called once per frame
- **Mutex Protection**: `fceuWrapperLock()/fceuWrapperUnLock()` - Protects emulator state
- **Memory Access**: `FCEU_CheatGetByte()/FCEU_CheatSetByte()` - Safe memory R/W
- **Input Injection**: `FCEUD_UpdateInput()` in `input.cpp` - Controller state update

### REST API Research (In Progress)
- Research documentation in `docs/architecture/`
- Proof-of-concepts in `poc/`
- Key findings in `docs/FCEUX_REST_API_RESEARCH_REPORT.md`

## Development Patterns

### Thread Safety
- Always acquire `emulatorMutex` when accessing emulator state from external threads
- Use `FCEU_WRAPPER_LOCK()/UNLOCK()` macros
- Prefer `fceuWrapperTryLock()` with timeout to avoid deadlocks

### Memory Access
```cpp
// Safe memory read
FCEU_WRAPPER_LOCK();
uint8 value = FCEU_CheatGetByte(address);
FCEU_WRAPPER_UNLOCK();

// Safe write ranges: 0x0000-0x07FF (RAM), 0x6000-0x7FFF (SRAM if battery)
```

### Adding Features
- GUI features: `src/drivers/Qt/`
- Core emulation: `src/`
- New mappers: `src/boards/`
- Menu items: Modify `ConsoleWindow.cpp`
- Config options: Update `config.cpp` and `ConsoleWindow.cpp`

### Global Functions
- `FCEUI_*` functions: Public API for frontend/core communication
- `FCEUD_*` functions: Driver-specific implementations
- `GameInfo` global: Current game information (check for NULL)

## Dependencies
- Qt5/Qt6 (Widgets, OpenGL required; Network, Help optional)
- SDL2 (>= 2.0, 2.8 recommended)
- CMake (>= 3.8)
- Optional: lua5.1, libx264/x265, ffmpeg, libarchive
- Compiler: g++, clang++, or msvc 2019

## Testing
- No unit test framework - testing is manual/integration
- Debug builds include AddressSanitizer (`-DASAN=ON`)
- Use debugger: `Tools → Debugger...` in GUI
- Command line: `fceux --help` for options
- Trace logging: Add to `src/debug.cpp`

## Common Issues
- Qt Network module required for NetPlay features
- OpenGL required for video rendering
- Check `GameInfo != NULL` before accessing game-specific data
- Emulation runs ~60 FPS (NTSC) or ~50 FPS (PAL)