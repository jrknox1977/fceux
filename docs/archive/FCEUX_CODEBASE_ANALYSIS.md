# FCEUX Codebase Analysis

## Overview
FCEUX is an open-source NES (Nintendo Entertainment System) emulator that supports Windows, Linux, and macOS. The project is actively maintained with version 2.7.0 in development. It features both a Qt-based GUI frontend and SDL backend for cross-platform compatibility.

## Project Structure

### Root Directory Layout
```
fceux/
├── CMakeLists.txt          # Main CMake configuration
├── README                  # SDL/Qt specific README
├── readme.md              # GitHub README with build status
├── src/                   # Main source code directory
├── documentation/         # User documentation and technical specs
├── output/               # Pre-built binaries and resources
├── pipelines/            # CI/CD build scripts
├── scripts/              # Development and utility scripts
├── vc/                   # Visual Studio project files
├── web/                  # Web documentation and help files
└── icons/                # Application icons and resources
```

### Source Code Organization (`src/`)
```
src/
├── boards/               # Mapper/board implementations (100+ files)
├── drivers/             # Platform-specific code
│   ├── Qt/              # Qt GUI implementation
│   ├── win/             # Windows-specific code
│   ├── sdl/             # Legacy SDL-only GUI
│   └── common/          # Shared driver code
├── input/               # Input device implementations
├── lua/                 # Embedded Lua 5.1 interpreter
├── utils/               # Utility functions and helpers
├── palettes/            # NES color palette data
└── [core emulation files]
```

## Build System

### CMake-based Build
- **Minimum CMake version**: 3.8
- **Build types**: Release, Debug
- **Main configuration**: `src/CMakeLists.txt`

### Supported Qt Versions
- Qt5 (default, minimum 5.11 recommended)
- Qt6 (enabled with `-DQT6=1`)

### Key Dependencies
**Required:**
- SDL2 (>= 2.0, 2.8+ recommended)
- Qt5/Qt6 Widgets and OpenGL modules
- zlib
- minizip
- OpenGL

**Optional:**
- lua5.1 (will use internal copy if not found)
- libarchive (for 7zip support)
- x264/x265 (for video encoding)
- ffmpeg libraries (for AVI recording)
- Qt Help module (for offline documentation)

### Platform-Specific Notes
- **Linux**: Uses pkg-config for dependency detection
- **Windows**: Requires manual dependency paths, uses MSVC
- **macOS**: Similar to Linux with some framework adjustments

## Development Workflow

### Building the Project
```bash
# Standard Qt/SDL build
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install

# Debug build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ..

# Qt6 build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DQT6=1 ..

# Enable profiling
cmake -DGPROF_ENABLE=1 ..

# Enable address sanitizer
cmake -DASAN_ENABLE=1 ..
```

### Code Quality Tools
- **cppcheck**: Static analysis tool
  - Script: `scripts/runCppCheck.sh`
  - Excludes attic and legacy SDL directories
  
### Testing
- No formal unit test framework identified
- Testing appears to be manual/integration based
- PPU tests mentioned in `NewPPUtests.txt`

### CI/CD Pipeline
- **AppVeyor**: Windows builds
- **GitHub Actions**: Implied for other platforms
- Build scripts in `pipelines/` directory:
  - `linux_build.sh` - Ubuntu/Debian builds
  - `macOS_build.sh` - macOS builds
  - `win64_build.bat`, `qwin64_build.bat` - Windows builds

## Architecture and Key Components

### Core Emulation
- **CPU Emulation**: `x6502.cpp/h` - 6502 processor emulation
- **PPU (Graphics)**: `ppu.cpp/h` - Picture Processing Unit
- **APU (Audio)**: `sound.cpp/h` - Audio Processing Unit
- **Memory Management**: `fceu.cpp/h` - Main emulation core
- **Cartridge/Mapper Support**: `boards/` directory with 100+ mapper implementations

### Frontend Architecture
The Qt GUI is the primary interface:
- **Main Window**: `ConsoleWindow.cpp` - Central application window
- **Video Rendering**: Multiple backends (OpenGL, SDL, QWidget)
- **Debugger Tools**: Extensive debugging suite including:
  - CPU debugger
  - PPU viewer
  - Hex editor
  - Trace logger
  - RAM search/watch
  - TAS (Tool-Assisted Speedrun) editor

### Scripting Support
- **Lua 5.1**: Embedded scripting engine
- **Script Manager**: Qt-based script management
- **API**: Extensive emulation control API for automation

### Network Play
- Netplay support for multiplayer gaming
- Separate server component in `fceux-server/`

## Key Features

### Emulation Features
- Accurate NES emulation with cycle-level timing options
- Support for 100+ cartridge mappers
- Save states and movie recording/playback
- Game Genie and cheat support
- NSF (NES Sound Format) playback

### Developer/Power User Tools
- Integrated debugger with breakpoints and stepping
- Memory viewer/editor
- PPU/Nametable viewers  
- Code/Data logger
- Trace logger
- RAM search for finding values
- TAS Editor for frame-perfect input recording

### Input Support
- Keyboard, gamepad, mouse
- Specialized controllers (Zapper, Power Pad, etc.)
- Configurable hotkeys
- Input recording and playback

## Code Style and Guidelines
- C++ codebase with some C code
- GPL v2 license
- Style guide in `STYLE-GUIDELINES-SDL`
- Extensive use of preprocessor directives for platform/feature detection

## Documentation
- User documentation in `documentation/` and `web/help/`
- API documentation can be generated with Doxygen
- Help files available in both HTML and CHM formats
- Qt Help integration for offline browsing

## Common Development Tasks

### Adding a New Mapper
1. Create new file in `src/boards/`
2. Implement mapper logic following existing patterns
3. Register in the mapper list

### Debugging Emulation Issues
1. Use built-in debugger tools
2. Enable trace logging
3. Use Lua scripts for automated testing

### Contributing
- Main repository: https://github.com/TASEmulators/fceux
- Follow existing code style
- Test on multiple platforms if possible
- Use the issue tracker for bug reports

## Notes for Future Development
- The codebase is mature with 20+ years of development
- Qt GUI is actively developed while SDL-only GUI is legacy
- Windows builds use Visual Studio, Linux/Mac use GCC/Clang
- Extensive mapper collection makes this suitable for compatibility testing
- TAS community is a primary user base, influencing feature development