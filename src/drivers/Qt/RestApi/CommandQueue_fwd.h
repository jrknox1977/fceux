#ifndef __COMMAND_QUEUE_FWD_H__
#define __COMMAND_QUEUE_FWD_H__

#include <string>
#include <vector>
#include <chrono>

// Forward declarations for REST API command queue

class CommandQueue;
class ApiCommand;

template<typename T>
class ApiCommandWithResult;

class ApiCommandVoid;

// Global accessor function
CommandQueue& getRestApiCommandQueue();

// Command execution result tracking
struct CommandExecutionResult {
    std::string commandName;
    bool success;
    std::string errorMessage;
    std::chrono::system_clock::time_point timestamp;
};

// Get recent command execution errors (thread-safe)
std::vector<CommandExecutionResult> getRecentCommandErrors(size_t maxCount = 10);

#endif // __COMMAND_QUEUE_FWD_H__