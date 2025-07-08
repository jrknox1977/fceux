# REST API Implementation Notes

## Key Information for Memory Endpoint Implementation

### Build System
- REST API files are added to `src/CMakeLists.txt` in the `REST_API` conditional section (around line 710)
- Pattern: `${CMAKE_CURRENT_SOURCE_DIR}/drivers/Qt/RestApi/...`
- No separate CMakeLists.txt for RestApi directory

### Exception Handling
- **No RestApiException class exists** - use `std::runtime_error` instead
- Include: `#include <stdexcept>`
- Pattern: `throw std::runtime_error("descriptive message");`

### Project Structure
```
src/drivers/Qt/RestApi/
├── RestApiServer.cpp/h     # Base HTTP server
├── FceuxApiServer.cpp/h     # FCEUX-specific endpoints
├── CommandQueue.cpp/h       # Command queue implementation
├── RestApiCommands.h        # Base command classes
├── EmulationController.cpp/h # Emulation control endpoints
├── RomInfoController.cpp/h   # ROM info endpoints
└── Utils/                   # Utility functions (created in #30)
    └── AddressParser.cpp/h
```

### Command Pattern
- Base classes in `RestApiCommands.h`:
  - `ApiCommand` - base class
  - `ApiCommandWithResult<T>` - commands that return values
  - `ApiCommandVoid` - commands with no return value
- Commands use `std::promise`/`std::future` for thread-safe results

### HTTP Server Details
- Uses cpp-httplib (header `#include "../../../lib/httplib.h"`)
- JSON handling: `#include "../../../lib/json.hpp"` (nlohmann::json)
- Route registration in `FceuxApiServer::registerRoutes()`
- Pattern: `addGetRoute("/api/path", handler);`

### Memory Access Functions (from FCEUX core)
- Safe memory read: `FCEU_CheatGetByte(uint32 address)`
- Located in: `#include "../../../../cheat.h"`
- Requires: `#include "../../../../fceu.h"` for GameInfo
- Thread safety: Use `FCEU_WRAPPER_LOCK()`/`FCEU_WRAPPER_UNLOCK()`
- From: `#include "../../fceuWrapper.h"`

### Global State
- `GameInfo` - global pointer to current game info
- Check `GameInfo != NULL` before accessing
- `GameInfo->battery` - bool indicating if SRAM is present

### Memory Ranges
- RAM: 0x0000-0x07FF (2KB)
- SRAM: 0x6000-0x7FFF (battery-backed, conditional on GameInfo->battery)

### Testing Pattern
- Standalone test programs (not gtest)
- Located in `tests/` or `Tests/` directories
- Compile with Qt5: `$(pkg-config --cflags --libs Qt5Core)`

## For Issue #31 (MemoryReadCommand)

### Command Implementation Pattern
```cpp
class MyCommand : public ApiCommandWithResult<ResultType> {
private:
    // input parameters
    ResultType result;
    
public:
    MyCommand(params...);
    void execute() override {
        // Do work, set result via resultPromise.set_value(result);
    }
    const char* name() const override { return "MyCommand"; }
};
```

### Thread Safety Pattern
```cpp
void execute() override {
    FCEU_WRAPPER_LOCK();
    if (GameInfo == NULL) {
        FCEU_WRAPPER_UNLOCK();
        throw std::runtime_error("No game loaded");
    }
    // Do work with emulator
    FCEU_WRAPPER_UNLOCK();
}
```

## For Issue #32 (REST Endpoint)

### Endpoint Registration Pattern (in FceuxApiServer.cpp)
```cpp
addGetRoute("/api/memory/([0-9a-fA-Fx]+)", [this](const httplib::Request& req, httplib::Response& res) {
    try {
        std::string addressStr = req.matches[1];
        // Parse, create command, queue it, wait for result
        // Return JSON response
    } catch (const std::runtime_error& e) {
        res.status = 400;
        // Return error JSON
    }
});
```

### JSON Response Pattern
```cpp
nlohmann::json response;
response["field"] = value;
res.set_content(response.dump(), "application/json");
res.status = 200;
```

## For Issue #33 (Tests & Documentation)

### Documentation Location
- API docs: `src/drivers/Qt/RestApi/README.md`
- Architecture docs: `docs/architecture/`
- Add examples to: `docs/api-examples/`

### Test Executable Pattern
- Create standalone test program
- Add to build if needed (check if there's a test target)
- Manual testing with curl commands