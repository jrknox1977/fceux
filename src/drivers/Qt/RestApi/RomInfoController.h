#ifndef __ROM_INFO_CONTROLLER_H__
#define __ROM_INFO_CONTROLLER_H__

namespace httplib {
    class Request;
    class Response;
}

/**
 * @brief REST API controller for ROM information endpoints
 */
class RomInfoController {
public:
    /**
     * @brief Handle GET /api/rom/info endpoint
     * @param req HTTP request
     * @param res HTTP response
     */
    static void handleRomInfo(const httplib::Request& req, httplib::Response& res);
};

#endif // __ROM_INFO_CONTROLLER_H__