#include "CommandQueue.h"
#include "RestApiCommands.h"
#include <utility>

CommandQueue::CommandQueue(size_t maxSize) 
    : maxQueueSize(maxSize) {
}

CommandQueue::~CommandQueue() {
    // Clear any pending commands to prevent hanging futures
    clear();
}

bool CommandQueue::push(std::unique_ptr<ApiCommand> cmd) {
    if (!cmd) {
        return false;  // Null command
    }
    
    FCEU::autoScopedLock lock(queueMutex);
    
    // Check if queue is full
    if (commands.size() >= maxQueueSize) {
        return false;  // Queue full
    }
    
    commands.push(std::move(cmd));
    return true;
}

std::unique_ptr<ApiCommand> CommandQueue::tryPop() {
    FCEU::autoScopedLock lock(queueMutex);
    
    if (commands.empty()) {
        return nullptr;
    }
    
    std::unique_ptr<ApiCommand> cmd = std::move(commands.front());
    commands.pop();
    return cmd;
}

bool CommandQueue::empty() const {
    FCEU::autoScopedLock lock(queueMutex);
    return commands.empty();
}

size_t CommandQueue::size() const {
    FCEU::autoScopedLock lock(queueMutex);
    return commands.size();
}

void CommandQueue::clear() {
    FCEU::autoScopedLock lock(queueMutex);
    
    // Cancel all pending commands before destroying
    // This prevents futures from being left in broken promise state
    while (!commands.empty()) {
        auto& cmd = commands.front();
        
        // Call virtual cancel method to handle promise cleanup
        cmd->cancel(std::make_exception_ptr(
            std::runtime_error("Command queue cleared - operation cancelled")));
        
        commands.pop();
    }
}