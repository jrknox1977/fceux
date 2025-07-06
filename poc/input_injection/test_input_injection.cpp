/**
 * Test program demonstrating input injection bypassing Qt event system
 * 
 * This proof-of-concept shows how to:
 * 1. Directly manipulate the joy[4] array that stores controller state
 * 2. Update JSreturn which is used by Qt input system
 * 3. Thread-safe queueing for REST API integration
 * 
 * Integration points:
 * - Call ApplyQueuedInputs() from FCEUD_UpdateInput() before UpdateGamepad()
 * - REST API endpoints can call QueueInput() from any thread
 * - Movie recording system will capture these inputs automatically
 */

#include <iostream>
#include <thread>
#include <chrono>
#include "input_injection.h"

// Mock the external variables for testing
uint8_t joy[4] = {0};
uint32_t JSreturn = 0;
bool FSAttached = false;

void testBasicButtonPress() {
    std::cout << "Test 1: Basic button press/release" << std::endl;
    
    // Press A button on controller 0
    InputInjection::PressButton(0, BUTTON_A);
    InputInjection::ApplyQueuedInputs();
    
    std::cout << "After pressing A: joy[0] = 0x" 
              << std::hex << (int)joy[0] << std::dec << std::endl;
    
    // Release A button
    InputInjection::ReleaseButton(0, BUTTON_A);
    InputInjection::ApplyQueuedInputs();
    
    std::cout << "After releasing A: joy[0] = 0x" 
              << std::hex << (int)joy[0] << std::dec << std::endl;
}

void testMultipleButtons() {
    std::cout << "\nTest 2: Multiple buttons simultaneously" << std::endl;
    
    // Press A+B+Start
    InputInjection::SetControllerState(0, BUTTON_A | BUTTON_B | BUTTON_START);
    InputInjection::ApplyQueuedInputs();
    
    std::cout << "A+B+Start pressed: joy[0] = 0x" 
              << std::hex << (int)joy[0] << std::dec << std::endl;
    std::cout << "JSreturn = 0x" << std::hex << JSreturn << std::dec << std::endl;
}

void testMultipleControllers() {
    std::cout << "\nTest 3: Multiple controllers" << std::endl;
    
    // Set different states for each controller
    InputInjection::SetControllerState(0, BUTTON_A);
    InputInjection::SetControllerState(1, BUTTON_B);
    InputInjection::SetControllerState(2, BUTTON_SELECT);
    InputInjection::SetControllerState(3, BUTTON_START);
    
    InputInjection::ApplyQueuedInputs();
    
    for (int i = 0; i < 4; i++) {
        std::cout << "Controller " << i << ": 0x" 
                  << std::hex << (int)joy[i] << std::dec << std::endl;
    }
    std::cout << "JSreturn = 0x" << std::hex << JSreturn << std::dec << std::endl;
}

void testThreadSafety() {
    std::cout << "\nTest 4: Thread safety" << std::endl;
    
    // Simulate multiple threads queueing inputs
    std::thread t1([]() {
        for (int i = 0; i < 10; i++) {
            InputInjection::PressButton(0, BUTTON_A);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    std::thread t2([]() {
        for (int i = 0; i < 10; i++) {
            InputInjection::PressButton(1, BUTTON_B);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    t1.join();
    t2.join();
    
    InputInjection::ApplyQueuedInputs();
    
    std::cout << "After concurrent access:" << std::endl;
    std::cout << "Controller 0: 0x" << std::hex << (int)joy[0] << std::dec << std::endl;
    std::cout << "Controller 1: 0x" << std::hex << (int)joy[1] << std::dec << std::endl;
}

void demonstrateIntegration() {
    std::cout << "\n=== Integration Example ===" << std::endl;
    std::cout << "To integrate with FCEUX:" << std::endl;
    std::cout << "1. Add to src/drivers/Qt/input.cpp:" << std::endl;
    std::cout << "   - Include input_injection.h" << std::endl;
    std::cout << "   - Call InputInjection::ApplyQueuedInputs() at start of FCEUD_UpdateInput()" << std::endl;
    std::cout << "\n2. REST API endpoint example:" << std::endl;
    std::cout << "   POST /api/controller/{port}/press" << std::endl;
    std::cout << "   Body: { \"button\": \"A\" }" << std::endl;
    std::cout << "   Handler: InputInjection::PressButton(port, BUTTON_A);" << std::endl;
    std::cout << "\n3. The movie system will automatically record these inputs!" << std::endl;
}

int main() {
    std::cout << "FCEUX Input Injection Proof of Concept" << std::endl;
    std::cout << "======================================" << std::endl;
    
    testBasicButtonPress();
    testMultipleButtons();
    testMultipleControllers();
    testThreadSafety();
    demonstrateIntegration();
    
    return 0;
}