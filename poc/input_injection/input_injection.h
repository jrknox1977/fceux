#ifndef INPUT_INJECTION_H
#define INPUT_INJECTION_H

#include <cstdint>

// NES Controller button masks
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

// Input injection interface for REST API
class InputInjection {
public:
    // Press a button on a specific controller (0-3)
    static void PressButton(int port, uint8_t button);
    
    // Release a button on a specific controller (0-3)
    static void ReleaseButton(int port, uint8_t button);
    
    // Set the complete state of a controller
    static void SetControllerState(int port, uint8_t state);
    
    // Get the current state of a controller
    static uint8_t GetControllerState(int port);
    
    // Queue input for next frame (thread-safe)
    static void QueueInput(int port, uint8_t state);
    
    // Apply queued inputs (called from emulation thread)
    static void ApplyQueuedInputs();
    
private:
    // Thread-safe input queue
    static uint8_t queuedInputs[4];
    static bool inputsQueued[4];
    static void* queueMutex;
};

#endif // INPUT_INJECTION_H