# FCEUX Concurrency and Qt Integration

## Overview
FCEUX uses a multi-threaded architecture with Qt's threading framework. The design separates GUI operations from emulation to ensure responsive user interface while maintaining accurate emulation timing.

## Threading Model

### Thread Architecture
1. **Main Thread (GUI)**
   - Runs Qt event loop
   - Handles all UI widgets and user interactions
   - Manages menus, dialogs, and tool windows
   - Named "MainThread" in code

2. **Emulator Thread**
   - Dedicated thread for NES emulation
   - Runs continuous loop executing frames
   - Implemented as `emulatorThread_t` class
   - Communicates with GUI via Qt signals/slots

3. **Auxiliary Threads**
   - AVI recording disk writer thread
   - NetPlay server/client threads
   - Script execution threads
   - Audio processing thread (platform-dependent)

## Synchronization Primitives

### Custom Mutex Wrapper
FCEUX implements a cross-platform mutex wrapper:

```cpp
// src/utils/mutex.h
namespace FCEU {
    class mutex {
        #ifdef __QT_DRIVER__
            QRecursiveMutex *m;  // Qt builds use QRecursiveMutex
        #else
            // Platform-specific implementation
        #endif
    };
    
    class autoScopedLock {
        // RAII lock guard for automatic unlock
    };
}
```

### Primary Mutexes

1. **Emulator Mutex** (`consoleWindow->emulatorMutex`)
   - Most critical mutex in the system
   - Protects all emulator state
   - Must be held when:
     - Loading/closing ROMs
     - Saving/loading states
     - Modifying emulator configuration
     - Accessing memory or registers

2. **Video Buffer Mutex** (`consoleWindow->videoBufferMutex`)
   - Protects shared video frame buffer
   - Used during frame blitting
   - Ensures consistent frame rendering

3. **Wrapper Lock**
   - Global convenience functions:
   ```cpp
   void fceuWrapperLock();
   void fceuWrapperUnLock();
   bool fceuWrapperTryLock(int timeout = 1000);
   ```

### Lock Acquisition Patterns

#### Standard Lock
```cpp
FCEU_WRAPPER_LOCK();
// Critical section
FCEU_WRAPPER_UNLOCK();
```

#### RAII Pattern (Preferred)
```cpp
{
    FCEU_CRITICAL_SECTION(consoleWindow->emulatorMutex);
    // Critical section - automatically unlocked
}
```

#### Try-Lock Pattern
```cpp
if (fceuWrapperTryLock(100)) {
    // Got lock within 100ms
    fceuWrapperUnLock();
} else {
    // Handle timeout
}
```

## Qt Integration

### Signal/Slot Communication

#### Key Signals from Emulator Thread
```cpp
signals:
    void frameFinished();          // Emitted after each frame
    void loadRomRequest(QString);  // Request GUI to load ROM
    void stateLoaded();           // State load completed
    void errorOccurred(QString);  // Error reporting
```

#### Thread-Safe Qt Invocation
```cpp
// From non-GUI thread to GUI thread
QMetaObject::invokeMethod(consoleWindow, "updatePeriodic", 
                         Qt::QueuedConnection);

// Blocking call with return value
bool result;
QMetaObject::invokeMethod(consoleWindow, "loadROM", 
                         Qt::BlockingQueuedConnection,
                         Q_RETURN_ARG(bool, result),
                         Q_ARG(QString, path));
```

### Shared Memory Structure
```cpp
struct nes_shm_t {
    // Video buffers
    uint32_t video_buff[4][256*256];
    
    // Control flags
    int runEmulator;
    int pauseEmulator;
    
    // Input state
    uint32_t pad[4];
    
    // Timing info
    struct {
        uint64_t frameCount;
        double framerate;
    } timing;
};
```

## Thread-Safe Queue Pattern

### NetPlay Input Queue Example
```cpp
class NetPlayServer {
private:
    FCEU::mutex inputMtx;
    std::list<NetPlayFrameInput> input;
    
public:
    void pushBackInput(NetPlayFrameInput &in) {
        FCEU::autoScopedLock alock(inputMtx);
        input.push_back(in);
    }
    
    bool popFrontInput(NetPlayFrameInput &out) {
        FCEU::autoScopedLock alock(inputMtx);
        if (input.empty()) return false;
        out = input.front();
        input.pop_front();
        return true;
    }
};
```

## REST API Integration Guidelines

### 1. HTTP Server Thread
```cpp
class RestApiServer : public QObject {
    Q_OBJECT
    
private:
    std::thread serverThread;
    
    void serverLoop() {
        // HTTP server runs here
        // Use Qt::QueuedConnection for GUI updates
        // Use mutex for emulator state access
    }
};
```

### 2. Command Processing
```cpp
void processApiCommand(const ApiCommand& cmd) {
    // Option 1: Direct execution with lock
    if (fceuWrapperTryLock(100)) {
        executeCommand(cmd);
        fceuWrapperUnLock();
    }
    
    // Option 2: Queue for frame-boundary execution
    {
        std::lock_guard<std::mutex> lock(cmdQueueMutex);
        commandQueue.push(cmd);
    }
}
```

### 3. Safe State Access
```cpp
// From HTTP handler thread
json getEmulatorState() {
    json result;
    
    FCEU_WRAPPER_LOCK();
    result["paused"] = FCEUI_EmulationPaused();
    result["frame"] = FCEUI_GetFrameCount();
    result["rom_loaded"] = (GameInfo != nullptr);
    FCEU_WRAPPER_UNLOCK();
    
    return result;
}
```

## Common Pitfalls and Solutions

### Deadlock Prevention
1. **Never hold multiple mutexes** - Use single emulator mutex
2. **Avoid blocking Qt signals** while holding mutex
3. **Use try-lock** in performance-critical paths

### Race Condition Avoidance
1. **Always protect shared data** with appropriate mutex
2. **Use atomic operations** for simple flags
3. **Copy data out** of critical sections when possible

### Performance Considerations
1. **Minimize lock duration** - Do calculations outside critical section
2. **Use read-write locks** for read-heavy data (if needed)
3. **Batch operations** to reduce lock/unlock overhead

## Testing Thread Safety

### Tools
- Valgrind with Helgrind: `valgrind --tool=helgrind ./fceux`
- ThreadSanitizer: Compile with `-fsanitize=thread`
- Qt's thread checker in debug builds

### Test Scenarios
1. Rapid pause/unpause during emulation
2. State save/load while input is active
3. ROM loading during frame rendering
4. Concurrent API calls during emulation

## Best Practices for REST API

1. **Run HTTP server in separate thread**
   - Prevents blocking emulation or GUI

2. **Use command queue pattern**
   - Queue commands from HTTP thread
   - Process in emulation thread at frame boundaries

3. **Leverage Qt's thread communication**
   - Use signals/slots for GUI updates
   - Use `QMetaObject::invokeMethod` for cross-thread calls

4. **Respect existing lock hierarchy**
   - Always acquire emulator mutex first
   - Never call Qt GUI functions while holding emulator mutex

5. **Implement timeout handling**
   - Use try-lock with reasonable timeouts
   - Return appropriate HTTP status codes on lock timeout