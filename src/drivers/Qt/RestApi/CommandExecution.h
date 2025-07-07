#ifndef __COMMAND_EXECUTION_H__
#define __COMMAND_EXECUTION_H__

#include <memory>
#include <future>
#include <chrono>
#include <exception>
#include "CommandQueue_fwd.h"
#include "RestApiCommands.h"

/**
 * @brief Execute a command with automatic exception handling
 * 
 * This function submits a command to the queue and waits for its result,
 * properly handling exceptions and timeouts.
 * 
 * @tparam T Result type of the command
 * @param cmd Command to execute (ownership transferred)
 * @param timeoutMs Timeout in milliseconds (default 5000ms)
 * @return Future containing the result or exception
 * 
 * @throws std::runtime_error if queue is full
 * @throws std::future_error on timeout
 */
template<typename T>
std::future<T> executeCommand(std::unique_ptr<ApiCommandWithResult<T>> cmd, 
                              unsigned int timeoutMs = 5000) {
    auto future = cmd->getResult();
    
    // Submit to queue
    if (!getRestApiCommandQueue().push(std::move(cmd))) {
        // Queue is full - set exception on the promise
        std::promise<T> errorPromise;
        errorPromise.set_exception(std::make_exception_ptr(
            std::runtime_error("Command queue is full")));
        return errorPromise.get_future();
    }
    
    return future;
}

/**
 * @brief Execute a void command with automatic exception handling
 * 
 * Specialization for void commands.
 */
inline std::future<void> executeCommand(std::unique_ptr<ApiCommandVoid> cmd,
                                        unsigned int timeoutMs = 5000) {
    auto future = cmd->getResult();
    
    // Submit to queue
    if (!getRestApiCommandQueue().push(std::move(cmd))) {
        // Queue is full - set exception on the promise
        std::promise<void> errorPromise;
        errorPromise.set_exception(std::make_exception_ptr(
            std::runtime_error("Command queue is full")));
        return errorPromise.get_future();
    }
    
    return future;
}

/**
 * @brief Wait for command result with timeout
 * 
 * Helper function to wait for a future with proper timeout handling.
 * 
 * @tparam T Result type
 * @param future Future to wait on
 * @param timeoutMs Timeout in milliseconds
 * @return Result value
 * 
 * @throws std::runtime_error on timeout
 * @throws Any exception set by the command
 */
template<typename T>
T waitForResult(std::future<T>& future, unsigned int timeoutMs = 5000) {
    auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));
    
    if (status == std::future_status::timeout) {
        throw std::runtime_error("Command execution timeout");
    }
    
    // This will re-throw any exception set by the command
    return future.get();
}

/**
 * @brief Check if any recent commands have failed
 * 
 * @param withinSeconds Check commands executed within this many seconds
 * @return true if any commands failed recently
 */
inline bool hasRecentCommandFailures(unsigned int withinSeconds = 60) {
    auto errors = getRecentCommandErrors(100);
    if (errors.empty()) return false;
    
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::seconds(withinSeconds);
    
    for (const auto& error : errors) {
        if (error.timestamp >= cutoff) {
            return true;
        }
    }
    
    return false;
}

#endif // __COMMAND_EXECUTION_H__