#ifndef __EMULATION_CONTROLLER_H__
#define __EMULATION_CONTROLLER_H__

#include <string>

// Forward declarations
namespace httplib {
    struct Request;
    struct Response;
}

/**
 * @brief REST API controller for emulation control endpoints
 * 
 * Provides HTTP handlers for pause, resume, and status operations.
 * All handlers execute commands on the emulator thread via the command queue.
 */
class EmulationController {
public:
    /**
     * @brief Handle POST /api/emulation/pause
     * 
     * Pauses emulation if currently running.
     * 
     * Response format:
     * {
     *   "success": true,
     *   "state": "paused"
     * }
     * 
     * Error responses:
     * - 400 Bad Request: No ROM loaded
     * - 500 Internal Server Error: Command execution failed
     */
    static void handlePause(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief Handle POST /api/emulation/resume
     * 
     * Resumes emulation if currently paused.
     * 
     * Response format:
     * {
     *   "success": true,
     *   "state": "resumed"
     * }
     * 
     * Error responses:
     * - 400 Bad Request: No ROM loaded
     * - 500 Internal Server Error: Command execution failed
     */
    static void handleResume(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief Handle GET /api/emulation/status
     * 
     * Returns current emulation status.
     * 
     * Response format:
     * {
     *   "running": true,
     *   "paused": false,
     *   "rom_loaded": true,
     *   "fps": 60.0,
     *   "frame_count": 12345
     * }
     * 
     * Error responses:
     * - 500 Internal Server Error: Command execution failed
     */
    static void handleStatus(const httplib::Request& req, httplib::Response& res);
    
private:
    // Prevent instantiation
    EmulationController() = delete;
    ~EmulationController() = delete;
    
    // Helper to create error response
    static std::string createErrorResponse(const std::string& error);
    
    // Helper to create success response
    static std::string createSuccessResponse(const std::string& state);
};

#endif // __EMULATION_CONTROLLER_H__