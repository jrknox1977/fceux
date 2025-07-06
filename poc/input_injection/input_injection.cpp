#include "input_injection.h"
#include <QMutex>
#include <cstring>

// External references to FCEUX input system
extern uint8_t joy[4];
extern uint32_t JSreturn;
extern bool FSAttached; // Four-score attachment status

// Static member definitions
uint8_t InputInjection::queuedInputs[4] = {0};
bool InputInjection::inputsQueued[4] = {false};
void* InputInjection::queueMutex = new QMutex();

void InputInjection::PressButton(int port, uint8_t button) {
    if (port < 0 || port > 3) return;
    
    QMutexLocker locker((QMutex*)queueMutex);
    queuedInputs[port] |= button;
    inputsQueued[port] = true;
}

void InputInjection::ReleaseButton(int port, uint8_t button) {
    if (port < 0 || port > 3) return;
    
    QMutexLocker locker((QMutex*)queueMutex);
    queuedInputs[port] &= ~button;
    inputsQueued[port] = true;
}

void InputInjection::SetControllerState(int port, uint8_t state) {
    if (port < 0 || port > 3) return;
    
    QMutexLocker locker((QMutex*)queueMutex);
    queuedInputs[port] = state;
    inputsQueued[port] = true;
}

uint8_t InputInjection::GetControllerState(int port) {
    if (port < 0 || port > 3) return 0;
    return joy[port];
}

void InputInjection::QueueInput(int port, uint8_t state) {
    SetControllerState(port, state);
}

void InputInjection::ApplyQueuedInputs() {
    QMutexLocker locker((QMutex*)queueMutex);
    
    // Apply queued inputs to the joy array
    for (int i = 0; i < 4; i++) {
        if (inputsQueued[i]) {
            joy[i] = queuedInputs[i];
            inputsQueued[i] = false;
        }
    }
    
    // Update JSreturn for ports 0 and 1
    // JSreturn format: Port 0 in low byte, Port 1 in bits 8-15
    // Port 2 in bits 16-23, Port 3 in bits 24-31 (when Four Score is enabled)
    JSreturn = 0;
    JSreturn |= ((uint32_t)joy[0]) << 0;
    JSreturn |= ((uint32_t)joy[1]) << 8;
    
    if (FSAttached) {
        JSreturn |= ((uint32_t)joy[2]) << 16;
        JSreturn |= ((uint32_t)joy[3]) << 24;
    }
}