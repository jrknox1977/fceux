#ifndef __COMMAND_QUEUE_FWD_H__
#define __COMMAND_QUEUE_FWD_H__

// Forward declarations for REST API command queue

class CommandQueue;
class ApiCommand;

template<typename T>
class ApiCommandWithResult;

class ApiCommandVoid;

// Global accessor function
CommandQueue& getRestApiCommandQueue();

#endif // __COMMAND_QUEUE_FWD_H__