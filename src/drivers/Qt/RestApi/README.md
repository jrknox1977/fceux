# REST API Server

This directory contains the REST API server implementation for FCEUX.

## Overview

The `RestApiServer` class provides a Qt-integrated HTTP server using the cpp-httplib library. It's designed to be extended through subclassing to add emulator-specific REST endpoints.

## Features

- **Qt Integration**: Uses Qt signals/slots for event notification
- **Thread-Safe**: Server runs in separate thread with proper synchronization
- **Extensible**: Virtual `registerRoutes()` method for custom endpoints
- **Configurable**: Timeouts and port settings can be adjusted
- **Secure by Default**: Binds to localhost only (127.0.0.1)

## Usage

### Basic Server

```cpp
RestApiServer server;
server.setStartupTimeout(10);  // 10 second startup timeout
server.setReadTimeout(5);      // 5 second read timeout
server.setWriteTimeout(5);     // 5 second write timeout

if (server.start(8080)) {
    // Server is running on port 8080
}
```

### Custom Endpoints

```cpp
class FceuxApiServer : public RestApiServer {
protected:
    void registerRoutes() override {
        // Add emulator-specific endpoints
        addGetRoute("/api/status", [](const auto& req, auto& res) {
            res.set_content("{\"status\":\"running\"}", "application/json");
        });
        
        addPostRoute("/api/rom/load", [](const auto& req, auto& res) {
            // Handle ROM loading
        });
    }
};
```

## Building

The REST API is enabled with the CMake option:

```bash
cmake .. -DREST_API=ON
```

## Architecture

### Threading Model

- **Main Thread**: Qt event loop, handles signals/slots
- **Server Thread**: Runs HTTP server, blocks on `listen()`
- **Synchronization**: Promise/future for startup, atomic bool for state

### Error Handling

The server uses an `ErrorCode` enum for structured error reporting:
- `PortInUse`: The specified port is already in use
- `BindFailed`: Failed to bind to the port
- `ThreadStartFailed`: Server thread failed to start
- `AlreadyRunning`: Attempt to start when already running

### Thread Safety

Qt signals emitted from the server thread (`errorOccurred`) use Qt's default queued connections, ensuring thread-safe communication with slots in other threads.

## Implementation Details

### Promise Protection

The server uses try-catch blocks around promise `set_value()` calls to prevent exceptions if the promise is set multiple times. This handles edge cases where the server thread might attempt to set the result after timeout.

### Configurable Timeouts

- **Startup Timeout**: Time to wait for server to bind and start listening
- **Read/Write Timeouts**: HTTP request/response timeouts

### Signal/Slot Interface

```cpp
signals:
    void serverStarted();      // Emitted when server successfully starts
    void serverStopped();      // Emitted when server stops
    void errorOccurred(const QString& error);  // Emitted on errors
```

## Future Enhancements

- WebSocket support for real-time updates
- SSL/TLS support (requires httplib SSL build)
- Authentication/authorization middleware
- Request logging and metrics