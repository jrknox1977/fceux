# REST API Server Infrastructure

## Summary

This PR implements the foundational REST API server infrastructure for FCEUX, providing a Qt-integrated HTTP server that can be extended to add emulator-specific endpoints.

## Changes

### Core Implementation
- **RestApiServer Class**: Base class for HTTP REST API server with Qt integration
  - Thread-safe server running in separate thread
  - Promise/future synchronization for reliable startup
  - Configurable timeouts (startup, read, write)
  - Extensible through virtual `registerRoutes()` method
  - Error handling with structured ErrorCode enum
  - Qt signal/slot integration for event notification

### Build System
- Added `REST_API` CMake option (default: OFF)
- Conditional compilation of REST API components
- Integration with existing Qt build infrastructure

### Thread Safety
- Server runs in dedicated thread using std::thread
- Atomic bool for state management
- Promise/future for startup synchronization with timeout
- Qt signals use queued connections for cross-thread safety
- Protected against promise double-set with try-catch blocks

### Configuration
- Localhost-only binding (127.0.0.1) for security
- Configurable port (default: 8080)
- Adjustable timeouts:
  - Startup timeout (default: 5 seconds)
  - Read timeout (default: 5 seconds)
  - Write timeout (default: 5 seconds)

## Files Added/Modified

### Added
- `src/drivers/Qt/RestApi/RestApiServer.h` - Server class header
- `src/drivers/Qt/RestApi/RestApiServer.cpp` - Server implementation
- `src/drivers/Qt/RestApi/README.md` - API documentation

### Modified
- `CMakeLists.txt` - Added REST_API option
- `src/CMakeLists.txt` - Conditional REST API compilation
- `CLAUDE.md` - Updated with REST API implementation details

## Usage Example

```cpp
class FceuxApiServer : public RestApiServer {
protected:
    void registerRoutes() override {
        addGetRoute("/api/emulation/status", [](const auto& req, auto& res) {
            res.set_content("{\"running\":true}", "application/json");
        });
    }
};

// In main application
FceuxApiServer apiServer;
if (apiServer.start(8080)) {
    // Server running on http://localhost:8080
}
```

## Testing

- Verified compilation with REST_API=ON/OFF
- Tested server startup/shutdown
- Verified timeout handling
- Tested promise protection against double-set
- Confirmed Qt signal thread safety

## Future Work

This PR provides the foundation. Future PRs will add:
- Emulator-specific endpoints (ROM control, state management, etc.)
- WebSocket support for real-time updates
- Authentication/authorization
- API versioning

## Dependencies

- cpp-httplib (header-only, included in `src/lib/`)
- Qt5/Qt6 Network module (for future enhancements)
- C++11 standard (for threading primitives)

## Code Review Notes

All issues from code review have been addressed:
- ✓ CMake option properly defined
- ✓ Race condition fixed with promise/future
- ✓ Promise double-set protection added
- ✓ Startup timeout made configurable
- ✓ Qt signal thread safety documented