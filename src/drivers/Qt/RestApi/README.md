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

## GUI Usage

When built with REST API support, FCEUX provides GUI controls:

1. **Tools Menu**: Go to Tools → REST API Server
2. **Toggle Server**: Click to enable/disable the server
3. **Status Bar**: Shows "REST API: Running on 127.0.0.1:8080" when active
4. **Persistence**: Server state is saved and restored on restart
5. **Default**: Server is enabled by default for convenience

## Configuration

The REST API server can be configured through FCEUX settings:
- `SDL.RestApiEnabled`: Whether to start server on launch (default: 1)
- `SDL.RestApiPort`: Server port (default: 8080, valid range: 1-65535)
- `SDL.RestApiBindAddress`: Bind address (default: "127.0.0.1")

## Building

The REST API is enabled with the CMake option:

```bash
cmake .. -DREST_API=ON
```

## Architecture

### Threading Model

- **Main Thread**: Qt event loop, handles signals/slots
- **Emulator Thread**: Runs NES emulation, processes commands
- **Server Thread**: Runs HTTP server, blocks on `listen()`
- **Command Queue**: Thread-safe communication between server and emulator
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

## Current API Endpoints

### System Endpoints
- `GET /api/system/info` - Returns FCEUX version and build information
- `GET /api/system/ping` - Health check endpoint
- `GET /api/system/capabilities` - Lists available API features

### Testing the API
```bash
# Check if server is running
curl http://localhost:8080/api/system/ping

# Get FCEUX version info
curl http://localhost:8080/api/system/info

# List API capabilities
curl http://localhost:8080/api/system/capabilities
```

## Command Queue Architecture

The command queue enables thread-safe communication between the REST API server and emulator thread.

### Basic Usage

```cpp
#include "Qt/RestApi/CommandQueue.h"
#include "Qt/RestApi/RestApiCommands.h"

// Define a command
class PauseCommand : public ApiCommandWithResult<bool> {
public:
    void execute() override {
        bool success = FCEUI_SetEmulationPaused(true);
        resultPromise.set_value(success);
    }
    const char* name() const override { return "PauseCommand"; }
};

// Submit from REST endpoint
auto cmd = std::make_unique<PauseCommand>();
auto future = cmd->getResult();
getRestApiCommandQueue().push(std::move(cmd));

// Wait for result with timeout
if (future.wait_for(std::chrono::seconds(2)) == std::future_status::ready) {
    bool success = future.get();
}
```

### Thread Safety
- Commands execute on emulator thread with mutex already held
- Queue operations are protected by FCEU::mutex
- Results returned via promise/future pattern
- Maximum 10 commands processed per frame to maintain performance

### Command Types
- `ApiCommand`: Base class for all commands
- `ApiCommandWithResult<T>`: For commands that return values
- `ApiCommandVoid`: Convenience class for void commands

## Future Enhancements

- WebSocket support for real-time updates
- SSL/TLS support (requires httplib SSL build)
- Authentication/authorization middleware
- Request logging and metrics
- Emulation control endpoints (pause, resume, reset)
- Memory access endpoints
- Save state management
- Screenshot capture