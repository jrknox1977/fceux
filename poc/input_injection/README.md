# Input Injection Proof of Concept

This demonstrates how to inject controller input into FCEUX bypassing the Qt event system, suitable for REST API integration.

## Key Findings

1. **Input Flow Architecture**:
   - Qt captures keyboard/joystick events in `ConsoleViewerGL::keyPressEvent()` 
   - Events are processed in `FCEUD_UpdateInput()` (src/drivers/Qt/input.cpp)
   - `UpdateGamepad()` updates the global `JSreturn` variable with button states
   - `JSreturn` is connected to the emulator core via `FCEUI_SetInput()` in `InitInputInterface()`
   - The emulator's `UpdateGP()` function copies data from `joyports[x].ptr` (which points to `JSreturn`) into the `joy[4]` array
   - CPU reads controller state via memory-mapped I/O at 0x4016/0x4017 through `JPRead()`

2. **Direct Injection Points**:
   - Modify `joy[4]` array directly (immediate effect)
   - Update `JSreturn` to maintain consistency with Qt layer
   - Apply changes before `UpdateGamepad()` is called

3. **Thread Safety**:
   - Input updates happen in emulation thread
   - REST API calls will come from different threads
   - Mutex-protected queue ensures thread safety

## Integration Steps

1. **Modify `src/drivers/Qt/input.cpp`**:
   ```cpp
   #include "input_injection.h"
   
   void FCEUD_UpdateInput(void) {
       // Apply any queued API inputs first
       InputInjection::ApplyQueuedInputs();
       
       // Continue with normal input processing
       updateGamePadKeyMappings();
       pollEventsSDL();
       // ... rest of function
   }
   ```

2. **REST API Endpoints**:
   ```
   POST /api/controller/{port}/button/{button}/press
   POST /api/controller/{port}/button/{button}/release
   POST /api/controller/{port}/state
   GET  /api/controller/{port}/state
   ```

3. **Movie Recording Compatibility**:
   - Inputs injected through `joy[4]` are automatically captured by the movie system
   - No special handling needed for recording/playback

## Build & Test

```bash
cd poc/input_injection
g++ -std=c++11 -I../../src -I/usr/include/qt5/QtCore test_input_injection.cpp input_injection.cpp -lQt5Core -o test_input
./test_input
```

## Button Mapping

| Button | Bit | Value |
|--------|-----|-------|
| A      | 0   | 0x01  |
| B      | 1   | 0x02  |
| Select | 2   | 0x04  |
| Start  | 3   | 0x08  |
| Up     | 4   | 0x10  |
| Down   | 5   | 0x20  |
| Left   | 6   | 0x40  |
| Right  | 7   | 0x80  |

## Notes

- The input system respects movie playback mode (inputs are ignored during playback)
- Four Score support allows 4 controllers when enabled
- TAS Editor has its own input handling that may need consideration