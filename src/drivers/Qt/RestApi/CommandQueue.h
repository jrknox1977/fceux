#ifndef __COMMAND_QUEUE_H__
#define __COMMAND_QUEUE_H__

#include <queue>
#include <memory>
#include <cstddef>
#include "utils/mutex.h"

// Forward declaration
class ApiCommand;

/**
 * @brief Thread-safe command queue for REST API to emulator communication
 * 
 * This queue allows the REST API server thread to submit commands that
 * will be executed on the emulator thread. All operations are protected
 * by FCEU::mutex for thread safety.
 * 
 * Usage:
 * - REST API thread: push() commands into the queue
 * - Emulator thread: tryPop() and execute commands
 * - Both threads: can check empty() and size()
 */
class CommandQueue {
private:
    std::queue<std::unique_ptr<ApiCommand>> commands;
    mutable FCEU::mutex queueMutex;
    static constexpr size_t DEFAULT_MAX_SIZE = 1000;
    size_t maxQueueSize;
    
public:
    /**
     * @brief Construct a new Command Queue
     * @param maxSize Maximum queue size (default: 1000)
     */
    explicit CommandQueue(size_t maxSize = DEFAULT_MAX_SIZE);
    
    /**
     * @brief Destructor - clears any pending commands
     */
    ~CommandQueue();
    
    // Disable copy operations
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;
    
    /**
     * @brief Push a command onto the queue
     * 
     * Thread-safe. Returns false if queue is full.
     * 
     * @param cmd Command to push (ownership transferred)
     * @return true if pushed successfully, false if queue is full
     */
    bool push(std::unique_ptr<ApiCommand> cmd);
    
    /**
     * @brief Try to pop a command from the queue
     * 
     * Thread-safe. Non-blocking - returns nullptr if queue is empty.
     * 
     * @return Command if available, nullptr if queue is empty
     */
    std::unique_ptr<ApiCommand> tryPop();
    
    /**
     * @brief Check if queue is empty
     * 
     * Thread-safe. Result may be stale by the time it's used.
     * 
     * @return true if queue is empty
     */
    bool empty() const;
    
    /**
     * @brief Get current queue size
     * 
     * Thread-safe. Result may be stale by the time it's used.
     * 
     * @return Current number of commands in queue
     */
    size_t size() const;
    
    /**
     * @brief Clear all pending commands
     * 
     * Thread-safe. Used during shutdown to prevent hanging futures.
     * Note: Commands are destroyed without executing, which may
     * leave their futures in a broken promise state.
     */
    void clear();
    
    /**
     * @brief Get maximum queue size
     * @return Maximum number of commands allowed in queue
     */
    size_t getMaxSize() const { return maxQueueSize; }
};

#endif // __COMMAND_QUEUE_H__