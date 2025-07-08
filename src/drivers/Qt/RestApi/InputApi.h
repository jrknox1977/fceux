#ifndef __INPUT_API_H__
#define __INPUT_API_H__

#include <cstdint>

// API Input overlay masks - similar to Lua's approach
// These masks are applied in UpdateGP() just like Lua does
extern uint8_t apiJoypadMask1[4];  // AND mask (1 = pass through, 0 = force clear)
extern uint8_t apiJoypadMask2[4];  // OR mask  (1 = force set, 0 = no effect)

// Initialize the API input system
void FCEU_ApiInputInit();

// Apply API input overlays to controller state
// Called from UpdateGP() in input.cpp, similar to FCEU_LuaReadJoypad
uint8_t FCEU_ApiReadJoypad(int which, uint8_t joyl);

// Set button states for API control
// Used by REST API commands to control input
void FCEU_ApiSetJoypad(int which, uint8_t buttonMask, bool force);

// Clear API control for a specific controller
void FCEU_ApiClearJoypad(int which);

// Clear all API input control
void FCEU_ApiClearAllJoypads();

#endif