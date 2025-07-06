# Input System Architecture

## Overview

The FCEUX input system handles the flow of input from the host OS (keyboard/joystick events) through the Qt GUI layer to the emulated NES controller ports. This document traces the complete input path and identifies key integration points for the REST API.

## Input Flow Diagram

```
Host OS Events (Keyboard/Joystick)
         ↓
Qt Event System (keyPressEvent/keyReleaseEvent)
         ↓
ConsoleViewerGL::keyPressEvent() [src/drivers/Qt/ConsoleViewerGL.cpp]
         ↓
pushKeyEvent() → keyEventQueue
         ↓
FCEUD_UpdateInput() [src/drivers/Qt/input.cpp:1682]
         ↓
UpdateGamepad() [src/drivers/Qt/input.cpp:1516]
         ↓
JSreturn (global uint32)
         ↓
InitInputInterface() connects JSreturn → joyports[x].ptr [src/drivers/Qt/input.cpp:1830]
         ↓
FCEU_UpdateInput() [src/input.cpp:442]
         ↓
UpdateGP() copies joyports[x].ptr → joy[4] array [src/input.cpp:241]
         ↓
CPU reads via JPRead() at 0x4016/0x4017 [src/input.cpp:121]
```

## Key Data Structures

### Controller State Storage

1. **`uint8_t joy[4]`** [src/input.cpp:95]
   - Core array storing the state of 4 NES controllers
   - Each byte represents one controller's button states
   - Bit mapping: A(0), B(1), Select(2), Start(3), Up(4), Down(5), Left(6), Right(7)

2. **`uint32_t JSreturn`** [src/drivers/Qt/input.cpp:142]
   - Qt-specific variable that packs controller states
   - Format: Port0[7:0], Port1[15:8], Port2[23:16], Port3[31:24]
   - Updated by UpdateGamepad() from Qt input events

3. **`struct JOYPORT joyports[2]`** [src/input.h:74]
   - Abstraction layer for input devices
   - `joyports[x].ptr` points to input data (e.g., &JSreturn)
   - `joyports[x].driver` contains function pointers for device operations

## Key Functions

### Qt Layer

1. **`FCEUD_UpdateInput()`** [src/drivers/Qt/input.cpp:1682]
   - Main entry point called each frame
   - Processes Qt events and updates controller states
   - Respects movie playback mode

2. **`UpdateGamepad()`** [src/drivers/Qt/input.cpp:1516]
   - Reads button mappings and updates JSreturn
   - Handles rapid-fire buttons and opposite direction blocking
   - Updates GamePad[x].bmapState for GUI display

3. **`InitInputInterface()`** [src/drivers/Qt/input.cpp:1807]
   - Connects Qt input data to emulator core
   - Sets joyports[x].ptr = &JSreturn for gamepads
   - Configures expansion port devices

### Core Emulator

1. **`FCEU_UpdateInput()`** [src/input.cpp:442]
   - Calls device-specific Update() functions
   - Handles VS Unisystem coin inputs
   - Triggers movie recording

2. **`UpdateGP()`** [src/input.cpp:241]
   - Copies data from joyports[x].ptr to joy[4] array
   - Integrates Lua script inputs
   - Handles Four Score configuration

3. **`JPRead()`** [src/input.cpp:121]
   - CPU read handler for addresses 0x4016/0x4017
   - Returns controller state one bit at a time
   - Implements serial shift register behavior

## Input Recording/Playback

The movie system automatically captures the joy[4] array state:

1. **Recording**: `LogGP()` [src/input.cpp:279] saves joy[4] to MovieRecord
2. **Playback**: `LoadGP()` [src/input.cpp:293] restores joy[4] from MovieRecord
3. **Protection**: Input updates are skipped during MOVIEMODE_PLAY

## REST API Integration Strategy

### Injection Point

The optimal injection point is at the beginning of `FCEUD_UpdateInput()`:

```cpp
void FCEUD_UpdateInput(void) {
    // Apply REST API inputs first
    InputInjection::ApplyQueuedInputs();
    
    // Continue with normal processing
    updateGamePadKeyMappings();
    pollEventsSDL();
    // ...
}
```

### Thread Safety

Since REST API calls arrive on different threads than the emulation thread:

1. Use a mutex-protected queue for input commands
2. Queue operations are thread-safe
3. Apply queued inputs synchronously in emulation thread

### Implementation Approach

1. **Direct joy[4] manipulation**: Immediate effect, works with movie system
2. **Update JSreturn**: Maintains consistency with Qt layer
3. **Respect input modes**: Check movie playback state before applying

## Special Considerations

### Four Score Support
- When enabled, all 4 controllers are active
- Ports 2&3 are accessed through expansion port protocol
- Check `FSAttached` flag and `eoptions & EO_FOURSCORE`

### TAS Editor Mode
- Has separate input handling via `FCEUMOV_Mode(MOVIEMODE_TASEDITOR)`
- May need special handling for frame-perfect input

### Input Lag
- Inputs are read once per frame during VBlank
- REST API inputs must be queued before frame processing

## Button Constants

```cpp
enum NESButton {
    BUTTON_A      = 0x01,
    BUTTON_B      = 0x02,
    BUTTON_SELECT = 0x04,
    BUTTON_START  = 0x08,
    BUTTON_UP     = 0x10,
    BUTTON_DOWN   = 0x20,
    BUTTON_LEFT   = 0x40,
    BUTTON_RIGHT  = 0x80
};
```

## Files Modified for REST API

1. `src/drivers/Qt/input.cpp` - Add injection call
2. `src/drivers/Qt/CMakeLists.txt` - Include new files
3. `src/input_injection.cpp/h` - New injection interface
4. REST API handler files - Queue input commands