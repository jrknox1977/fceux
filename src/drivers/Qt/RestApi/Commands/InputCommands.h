#ifndef __INPUT_COMMANDS_H__
#define __INPUT_COMMANDS_H__

#include "../RestApiCommands.h"
#include "../../../../types.h"
#include "../../../../fceu.h"
#include <string>
#include <cstdint>
#include <vector>
#include <queue>
#include <mutex>
#include <unordered_map>

// Forward declarations
extern uint8 joy[4];
extern int currFrameCounter;
extern FCEUGI* GameInfo;

// Button bit positions for NES controllers
#define JOY_A      0x01  // Bit 0
#define JOY_B      0x02  // Bit 1  
#define JOY_SELECT 0x04  // Bit 2
#define JOY_START  0x08  // Bit 3
#define JOY_UP     0x10  // Bit 4
#define JOY_DOWN   0x20  // Bit 5
#define JOY_LEFT   0x40  // Bit 6
#define JOY_RIGHT  0x80  // Bit 7

/**
 * @brief Pending button release entry
 * 
 * Tracks buttons that need to be released at a specific frame
 */
struct PendingRelease {
    uint8_t port;          ///< Controller port (0-3)
    uint8_t buttonMask;    ///< Buttons to release (bitmask)
    int releaseFrame;      ///< Frame counter when to release
};

/**
 * @brief Static manager for pending button releases
 * 
 * This class manages timed button releases using the emulator's
 * frame counter for precise timing.
 */
class InputReleaseManager {
private:
    static std::vector<PendingRelease> pendingReleases;
    static std::mutex releaseMutex;
    
public:
    /**
     * @brief Add a pending button release
     * @param port Controller port (0-3)
     * @param buttonMask Buttons to release
     * @param releaseFrame Frame when to release
     */
    static void addPendingRelease(uint8_t port, uint8_t buttonMask, int releaseFrame);
    
    /**
     * @brief Process pending releases for current frame
     * 
     * Should be called from emulator update loop.
     * Requires emulator mutex to already be held.
     */
    static void processPendingReleases();
    
    /**
     * @brief Clear all pending releases
     */
    static void clearAll();
};

/**
 * @brief Result structure for input status query
 */
struct InputStatusResult {
    struct ControllerState {
        bool connected;
        std::unordered_map<std::string, bool> buttons;
    };
    
    ControllerState port1;
    ControllerState port2;
    
    std::string toJson() const;
};

/**
 * @brief Result structure for button press operations
 */
struct InputPressResult {
    bool success;
    uint8_t port;
    std::vector<std::string> buttonsPressed;
    int durationMs;
    
    std::string toJson() const;
};

/**
 * @brief Result structure for button release operations
 */
struct InputReleaseResult {
    bool success;
    uint8_t port;
    std::vector<std::string> buttonsReleased;
    
    std::string toJson() const;
};

/**
 * @brief Result structure for state set operations
 */
struct InputStateResult {
    bool success;
    uint8_t port;
    uint8_t state;
    
    std::string toJson() const;
};

/**
 * @brief Helper to convert button names to bitmask
 * @param buttonNames Vector of button names ("A", "B", etc.)
 * @return Bitmask of buttons
 * @throws std::runtime_error for invalid button names
 */
uint8_t buttonNamesToBitmask(const std::vector<std::string>& buttonNames);

/**
 * @brief Helper to convert bitmask to button names
 * @param mask Button bitmask
 * @return Vector of button names
 */
std::vector<std::string> bitmaskToButtonNames(uint8_t mask);

/**
 * @brief Command to get current input state
 */
class InputStatusCommand : public ApiCommandWithResult<InputStatusResult> {
public:
    void execute() override;
    const char* name() const override { return "InputStatusCommand"; }
};

/**
 * @brief Command to press buttons with optional duration
 */
class InputPressCommand : public ApiCommandWithResult<InputPressResult> {
private:
    uint8_t port;
    std::vector<std::string> buttons;
    int durationMs;
    
public:
    InputPressCommand(int portNum, const std::vector<std::string>& btns, int duration = 16);
    void execute() override;
    const char* name() const override { return "InputPressCommand"; }
};

/**
 * @brief Command to release specific buttons
 */
class InputReleaseCommand : public ApiCommandWithResult<InputReleaseResult> {
private:
    uint8_t port;
    std::vector<std::string> buttons;
    
public:
    InputReleaseCommand(int portNum, const std::vector<std::string>& btns);
    void execute() override;
    const char* name() const override { return "InputReleaseCommand"; }
};

/**
 * @brief Command to set complete controller state
 */
class InputStateCommand : public ApiCommandWithResult<InputStateResult> {
private:
    uint8_t port;
    std::unordered_map<std::string, bool> state;
    
public:
    InputStateCommand(int portNum, const std::unordered_map<std::string, bool>& newState);
    void execute() override;
    const char* name() const override { return "InputStateCommand"; }
};

#endif // __INPUT_COMMANDS_H__