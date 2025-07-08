# REST API POST Route 404 Fix Documentation

## Issue Summary
POST routes in the FCEUX REST API were returning 404 errors while GET routes worked correctly. This issue affected all POST endpoints including `/api/emulation/pause`, `/api/emulation/resume`, and test endpoints.

## Root Cause Analysis
After extensive debugging, we discovered that httplib v0.22.0 was rejecting POST requests with a 400 (Bad Request) status before attempting to match routes. This behavior only occurred within the FCEUX/Qt environment - isolated test programs using the same httplib version worked correctly.

### Key Findings:
1. GET routes worked perfectly, only POST routes were affected
2. Pre-routing handler confirmed POST requests were received
3. Error handler was called with status 400 (not 404), indicating request validation failure
4. The issue was specific to the FCEUX build environment
5. httplib's `expect_content()` function always returns true for POST requests, triggering content reader path

## Implemented Solution
We implemented a workaround that intercepts POST requests in the pre-routing handler and manually routes them to stored handlers, bypassing httplib's normal POST routing mechanism.

### Changes Made:

1. **RestApiServer.h** - Added POST handler storage:
```cpp
// WORKAROUND: Manual POST handler storage due to httplib issue
std::map<std::string, std::function<void(const httplib::Request&, httplib::Response&)>> m_postHandlers;
```

2. **RestApiServer.cpp** - Modified `addPostRoute()`:
```cpp
void RestApiServer::addPostRoute(const std::string& pattern,
    std::function<void(const httplib::Request&, httplib::Response&)> handler)
{
    if (m_server) {
        // WORKAROUND: Store POST handlers manually
        m_postHandlers[pattern] = handler;
        
        // Still register with httplib for compatibility
        m_server->Post(pattern, handler);
        
        printf("REST API: Registered POST route: %s (stored in manual handler map)\n", pattern.c_str());
    }
}
```

3. **RestApiServer.cpp** - Modified pre-routing handler in `setupDefaultRoutes()`:
```cpp
m_server->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
    // WORKAROUND: Manually handle POST requests due to httplib issue
    if (req.method == "POST") {
        // Check our stored POST handlers
        auto it = m_postHandlers.find(req.path);
        if (it != m_postHandlers.end()) {
            it->second(req, res);
            return httplib::Server::HandlerResponse::Handled;
        }
    }
    
    return httplib::Server::HandlerResponse::Unhandled;
});
```

## Testing Results
After implementing the workaround:
- ✅ POST `/api/test` returns successful response
- ✅ POST `/api/emulation/pause` works correctly
- ✅ POST `/api/emulation/resume` works correctly
- ✅ GET routes continue to work as before

## Future Considerations
1. This workaround should be removed once the underlying httplib issue is resolved
2. Consider investigating why httplib behaves differently in the Qt/FCEUX environment
3. The workaround adds minimal overhead as it only affects POST request routing
4. All existing POST route registrations continue to work without modification

## Related Files Modified
- `/src/drivers/Qt/RestApi/RestApiServer.h`
- `/src/drivers/Qt/RestApi/RestApiServer.cpp`

## Debug Summary
The investigation revealed that httplib's request processing flow for POST requests differs from GET requests. POST requests trigger the content reader path due to `expect_content()` returning true, but in the FCEUX environment, this path fails with a 400 error before route matching occurs. The exact reason why this only affects FCEUX remains unclear but is likely related to Qt event loop interaction or build configuration differences.