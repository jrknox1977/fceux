#include "InputApi.h"

// API input overlay masks (similar to Lua's luajoypads1/2)
uint8_t apiJoypadMask1[4] = { 0xFF, 0xFF, 0xFF, 0xFF };  // AND mask - all bits pass through by default
uint8_t apiJoypadMask2[4] = { 0x00, 0x00, 0x00, 0x00 };  // OR mask - no bits forced by default

void FCEU_ApiInputInit() {
    // Reset all masks to pass-through state
    for (int i = 0; i < 4; i++) {
        apiJoypadMask1[i] = 0xFF;
        apiJoypadMask2[i] = 0x00;
    }
}

uint8_t FCEU_ApiReadJoypad(int which, uint8_t joyl) {
    // Apply the overlay masks just like Lua does
    // AND with mask1 clears bits where mask1 has 0
    // OR with mask2 sets bits where mask2 has 1
    joyl = (joyl & apiJoypadMask1[which]) | apiJoypadMask2[which];
    
    // Reset masks after reading (single-frame effect)
    // This ensures buttons are only pressed for the duration specified
    apiJoypadMask1[which] = 0xFF;
    apiJoypadMask2[which] = 0x00;
    
    return joyl;
}

void FCEU_ApiSetJoypad(int which, uint8_t buttonMask, bool force) {
    if (which < 0 || which > 3) return;
    
    if (force) {
        // Force these buttons on (set bits in OR mask)
        apiJoypadMask2[which] |= buttonMask;
    } else {
        // Force these buttons off (clear bits in AND mask)
        apiJoypadMask1[which] &= ~buttonMask;
    }
}

void FCEU_ApiClearJoypad(int which) {
    if (which < 0 || which > 3) return;
    
    // Reset to pass-through state
    apiJoypadMask1[which] = 0xFF;
    apiJoypadMask2[which] = 0x00;
}

void FCEU_ApiClearAllJoypads() {
    for (int i = 0; i < 4; i++) {
        apiJoypadMask1[i] = 0xFF;
        apiJoypadMask2[i] = 0x00;
    }
}