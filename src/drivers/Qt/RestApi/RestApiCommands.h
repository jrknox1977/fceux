#ifndef __REST_API_COMMANDS_H__
#define __REST_API_COMMANDS_H__

#include <memory>
#include <future>
#include <exception>
#include <string>

/**
 * @brief Base class for all REST API commands
 * 
 * Commands are executed on the emulator thread via the command queue.
 * Derived classes must implement execute() and name() methods.
 */
class ApiCommand {
public:
    virtual ~ApiCommand() = default;
    
    /**
     * @brief Execute the command on the emulator thread
     * 
     * This method is called with the emulator mutex already held.
     * Commands should check GameInfo != nullptr if they require an active game.
     * 
     * @throws std::exception on error (will be caught and logged)
     */
    virtual void execute() = 0;
    
    /**
     * @brief Get the command name for logging/debugging
     * @return Command name as C-string
     */
    virtual const char* name() const = 0;
};

/**
 * @brief Base class for commands that return a result
 * 
 * Uses std::promise/future for thread-safe result passing.
 * The REST API endpoint can wait on the future with a timeout.
 * 
 * @tparam T Result type
 */
template<typename T>
class ApiCommandWithResult : public ApiCommand {
protected:
    std::promise<T> resultPromise;
    
public:
    /**
     * @brief Get the future for retrieving the command result
     * 
     * Call this before submitting the command to the queue.
     * The future will be ready after execute() completes.
     * 
     * @return std::future<T> for the result
     */
    std::future<T> getResult() {
        return resultPromise.get_future();
    }
    
    /**
     * @brief Set an exception result
     * 
     * Helper method for derived classes to report errors.
     * 
     * @param e Exception to set
     */
    void setException(const std::exception_ptr& e) {
        try {
            resultPromise.set_exception(e);
        } catch (const std::future_error&) {
            // Promise already satisfied, ignore
        }
    }
};

/**
 * @brief Command without a result (void return)
 * 
 * Convenience base class for commands that don't return a value.
 * Still uses promise/future to signal completion.
 */
class ApiCommandVoid : public ApiCommandWithResult<void> {
protected:
    /**
     * @brief Signal successful completion
     * 
     * Call this at the end of execute() for void commands.
     */
    void setSuccess() {
        try {
            resultPromise.set_value();
        } catch (const std::future_error&) {
            // Promise already satisfied, ignore
        }
    }
};

#endif // __REST_API_COMMANDS_H__