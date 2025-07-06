# FCEUX Main Emulation Loop and Threading Architecture

## Overview
FCEUX uses a multi-threaded architecture with Qt GUI running in the main thread and emulation running in a dedicated thread. This separation ensures the GUI remains responsive while emulation runs at a consistent speed.

## Threading Model

### Main Thread (Qt GUI)
- Handles all Qt widgets and user interface
- Processes menu actions and keyboard/mouse events
- Communicates with emulator thread via shared memory and mutexes

### Emulator Thread (`emulatorThread_t`)
- Runs the actual NES emulation
- Located in `src/drivers/Qt/ConsoleWindow.cpp:5132`
- Controlled by `nes_shm->runEmulator` flag

## Emulation Loop Call Stack

### 1. Thread Entry Point
```cpp
// ConsoleWindow.cpp:5132
void emulatorThread_t::run(void)
{
    while ( nes_shm->runEmulator )
    {
        fceuWrapperUpdate();
    }
}
```

### 2. Thread-Safe Wrapper
```cpp
// fceuWrapper.cpp:1469
int fceuWrapperUpdate(void)
{
    // Mutex protection for thread safety
    fceuWrapperLock();
    
    // Handle NetPlay synchronization
    if (NetPlayActive()) { /* ... */ }
    
    // Main emulation call
    if (GameInfo) {
        DoFun(frameskip, periodic_saves);
    }
    
    fceuWrapperUnLock();
}
```

### 3. Frame Control Logic
```cpp
// fceuWrapper.cpp:1328
static void DoFun(int frameskip, int periodic_saves)
{
    // Handle TAS editor pause frames
    // Frame skip logic
    // Calls core emulation
    FCEUI_Emulate(&gfx, &sound, &ssize, frameskip);
}
```

### 4. Core Emulation Function
```cpp
// fceu.cpp:745
void FCEUI_Emulate(uint8 **pXBuf, int32 **SoundBuf, int32 *SoundBufSize, int skip)
{
    // Handle pause state
    // Update input
    // Run one frame of emulation
    FCEUPPU_Loop(skip);
    // Update counters and state
}
```

### 5. PPU Loop (Actual Frame Emulation)
```cpp
// ppu.cpp:1771
void FCEUPPU_Loop(int skip)
{
    // For each scanline (0-261):
    //   - Execute CPU cycles via X6502_Run()
    //   - Update PPU state
    //   - Generate video output
    //   - Handle sprite evaluation
    //   - Trigger audio generation
}
```

## Key Integration Points for REST API

### 1. Command Queue Injection Point
The ideal location for command queue processing is in `fceuWrapperUpdate()` after acquiring the mutex lock but before calling `DoFun()`. This ensures:
- Thread safety via existing mutex
- Execution once per frame
- Access to emulator state

### 2. Frame Synchronization
Commands should be processed at frame boundaries to ensure consistency:
- Beginning of frame: After `fceuWrapperLock()` in `fceuWrapperUpdate()`
- End of frame: After `DoFun()` returns

### 3. Global State Access
The emulator uses global state extensively:
- `GameInfo` - Current game information
- `FCEUI_*` functions - Public API for emulator control
- `nes_shm` - Shared memory structure for thread communication

## Thread Safety Considerations

### Existing Mutex Usage
- `fceuWrapperLock()/fceuWrapperUnLock()` - Main emulation mutex
- Protects all emulator state access
- Must be held when accessing emulator from external threads

### Shared Memory Structure (`nes_shm`)
- Used for communication between GUI and emulator threads
- Contains control flags like `runEmulator`, `pauseEmulator`
- Can be extended for REST API command flags

## Recommended API Integration Approach

1. **Add Command Queue**
   ```cpp
   // In shared memory structure
   std::queue<ApiCommand> commandQueue;
   std::mutex commandQueueMutex;
   ```

2. **Process Commands in fceuWrapperUpdate()**
   ```cpp
   int fceuWrapperUpdate(void)
   {
       fceuWrapperLock();
       
       // NEW: Process API commands
       processApiCommands();
       
       // Existing emulation logic
       if (GameInfo) {
           DoFun(frameskip, periodic_saves);
       }
       
       fceuWrapperUnLock();
   }
   ```

3. **Command Processing Function**
   ```cpp
   void processApiCommands()
   {
       std::lock_guard<std::mutex> lock(commandQueueMutex);
       while (!commandQueue.empty()) {
           auto cmd = commandQueue.front();
           commandQueue.pop();
           executeCommand(cmd);
       }
   }
   ```

## Performance Considerations

- Command processing must be fast to avoid frame drops
- Complex operations should be deferred or run asynchronously
- Target: < 1ms command processing time to maintain 60 FPS

## Next Steps for PoC

1. Modify `fceuWrapper.cpp` to add command processing hook
2. Create simple command queue structure
3. Test with basic commands (pause/unpause, frame advance)
4. Measure performance impact