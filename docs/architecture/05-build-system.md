# FCEUX Build System Analysis

## Overview
FCEUX uses CMake as its build system with support for multiple platforms (Linux, Windows, macOS) and flexible dependency management. The build system is designed to work with both Qt5 and Qt6.

## CMake Structure

### Main CMakeLists.txt Files
- `/CMakeLists.txt` - Top-level, minimal setup (requires CMake 3.8+)
- `/src/CMakeLists.txt` - Main build configuration

### Key Build Variables
- `QT6` - Force Qt6 usage (optional)
- `QT` - Qt version (5 or 6, auto-detected if not specified)
- `ASAN` - Enable AddressSanitizer for memory debugging
- `GPROF_ENABLE` - Enable GNU profiling
- `PUBLIC_RELEASE` - Define for public release builds

## C++ Standard
**Important**: No explicit C++ standard is set in the CMake files. This means:
- The compiler's default C++ standard is used
- Modern C++ features (C++11/14/17) availability depends on the compiler
- For cpp-httplib integration, we may need to add explicit C++ standard requirements

## Dependency Management

### Qt Framework
```cmake
# Qt6 Support
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
find_package(Qt6 REQUIRED COMPONENTS Network)
find_package(Qt6 COMPONENTS Help QUIET)
find_package(Qt6 COMPONENTS Qml)
find_package(Qt6 COMPONENTS UiTools)

# Qt5 Support
find_package(Qt5 REQUIRED COMPONENTS Widgets OpenGL)
find_package(Qt5 COMPONENTS Help QUIET)
find_package(Qt5 COMPONENTS Network)
find_package(Qt5 COMPONENTS Qml)
find_package(Qt5 COMPONENTS UiTools)
```

### Network Support
- Qt Network module is already integrated
- When found, defines:
  - Qt6: `__FCEU_QNETWORK_ENABLE__`
  - Qt5: `__FCEU_NETWORK_ENABLE__`
- Used by existing NetPlay feature

### Other Dependencies
- OpenGL (required)
- ZLIB (required on Unix)
- SDL2 (for input/audio)
- minizip (required on Unix)
- Lua 5.1 (optional)
- libarchive (Windows)
- libav/ffmpeg (optional, for video recording)

## Platform-Specific Configuration

### Linux/Unix
```cmake
add_definitions(-Wall -Wno-write-strings -Wno-parentheses -fPIC)
add_definitions(-DFCEUDEF_DEBUGGER)
```

### Windows
```cmake
add_definitions(-DMSVC -D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D__SDL__ -D__QT_DRIVER__ -DQT_DEPRECATED_WARNINGS)
add_definitions(-DFCEUDEF_DEBUGGER)
```

## Adding cpp-httplib Integration

### Recommended Approach

1. **Add C++ Standard Requirement**
```cmake
# Add after project(fceux) in src/CMakeLists.txt
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

2. **Add cpp-httplib as Header-Only Library**
```cmake
# Create new section for REST API dependencies
# Option 1: Include as subdirectory
add_subdirectory(lib/cpp-httplib)

# Option 2: Direct include
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/cpp-httplib)
```

3. **Add REST API Source Files**
```cmake
# Add to source list
set(SRC_DRIVERS_REST_API
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/Qt/RestApi/RestApiServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/Qt/RestApi/RestApiHandlers.cpp
)

# Add to SOURCES
set(SOURCES ${SRC_CORE} ${SRC_DRIVERS_COMMON} ${SRC_DRIVERS_SDL} ${SRC_DRIVERS_REST_API})
```

4. **Conditional Compilation**
```cmake
# Add option for REST API
option(REST_API "Enable REST API server" ON)

if(REST_API)
    add_definitions(-D__FCEU_REST_API_ENABLE__)
    # Include REST API sources
endif()
```

## Thread and Network Libraries
No additional linking required for:
- Threading (pthreads on Linux, Windows threads on Windows)
- Networking (handled by Qt Network module)

## Example Build Commands

### Standard Release Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Debug Build with AddressSanitizer
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DASAN=ON
```

### Build with REST API (proposed)
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DREST_API=ON
```

## Integration Strategy for cpp-httplib

1. **Minimal Changes**: Add cpp-httplib as a header-only library to minimize build system changes
2. **Use Qt Integration**: Leverage existing Qt Network support for compatibility
3. **Follow Existing Patterns**: Model after NetPlay implementation for consistency
4. **Optional Feature**: Make REST API optional via CMake flag

## Potential Issues and Solutions

### Issue: C++ Standard Compatibility
- **Problem**: cpp-httplib requires C++11
- **Solution**: Add explicit C++ standard requirement to CMake

### Issue: Threading Model
- **Problem**: cpp-httplib uses std::thread, FCEUX uses Qt threads
- **Solution**: Use cpp-httplib's thread pool with careful synchronization to Qt event loop

### Issue: Header-Only Library Size
- **Problem**: cpp-httplib is a large header file
- **Solution**: Include only in implementation files, not headers

## Next Steps for PoC

1. Download cpp-httplib.h into `src/lib/cpp-httplib/`
2. Create minimal CMake modifications
3. Build simple HTTP server test
4. Verify C++11 features work correctly
5. Test integration with existing Qt infrastructure