# HTTP Server Build Integration PoC

This PoC demonstrates how to integrate cpp-httplib into FCEUX's build system and create a basic HTTP server.

## Overview

The PoC shows:
1. How to add cpp-httplib as a header-only dependency
2. CMake modifications for C++11 support
3. Basic HTTP server with thread pool and timeouts
4. Integration with Qt's event loop

## Implementation

### HTTP Server Class
```cpp
class RestApiServer : public QObject {
    Q_OBJECT
    
private:
    httplib::Server svr;
    httplib::ThreadPool pool;
    std::thread serverThread;
    
public:
    RestApiServer() : pool(8) {
        // Configure timeouts
        svr.set_read_timeout(5, 0);
        svr.set_write_timeout(5, 0);
        
        // Setup endpoints
        setupEndpoints();
    }
};
```

### Key Features
- 8-thread pool for handling concurrent requests
- 5-second timeouts for read/write operations
- Runs in separate thread from emulation
- Communicates with emulator via command queue

## Build Integration

### 1. Download cpp-httplib
```bash
mkdir -p src/lib/cpp-httplib
wget https://github.com/yhirose/cpp-httplib/releases/download/v0.14.3/httplib.h \
     -O src/lib/cpp-httplib/httplib.h
```

### 2. CMake Modifications
Add to `src/CMakeLists.txt`:
```cmake
# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add REST API option
option(REST_API "Enable REST API server" ON)

if(REST_API)
    add_definitions(-D__FCEU_REST_API_ENABLE__)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/cpp-httplib)
    
    set(SRC_REST_API
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/Qt/RestApi/RestApiServer.cpp
    )
    
    set(SOURCES ${SOURCES} ${SRC_REST_API})
endif()
```

## Files

- `RestApiServer.h` - Server class definition
- `RestApiServer.cpp` - Implementation with endpoints
- `CMakeLists.patch` - Build system changes
- `test_server.cpp` - Standalone test program

## Testing

1. Build the standalone test:
```bash
g++ -std=c++11 test_server.cpp -pthread -o test_server
./test_server
```

2. Test endpoints:
```bash
curl http://localhost:8080/api/status
curl -X POST http://localhost:8080/api/pause
```

## Performance Considerations

- Server runs in separate thread, no impact on emulation
- Thread pool prevents connection overload
- Timeouts prevent hanging connections
- Command queue ensures thread-safe communication

## Security Considerations

- Bind to localhost only by default
- No authentication (add for production)
- Input validation on all endpoints
- Rate limiting recommended