#ifndef __FCEUX_API_SERVER_H__
#define __FCEUX_API_SERVER_H__

#include "RestApiServer.h"
#include <QString>

/**
 * @brief FCEUX-specific REST API server implementation
 * 
 * Provides system information endpoints that don't require emulator state access.
 * These endpoints can respond even when no ROM is loaded.
 */
class FceuxApiServer : public RestApiServer
{
    Q_OBJECT

public:
    explicit FceuxApiServer(QObject* parent = nullptr);
    virtual ~FceuxApiServer() = default;

protected:
    /**
     * @brief Register FCEUX-specific API routes
     */
    void registerRoutes() override;

private:
    /**
     * @brief GET /api/system/info - Returns FCEUX version and build information
     */
    void handleSystemInfo(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief GET /api/system/ping - Health check endpoint
     */
    void handleSystemPing(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief GET /api/system/capabilities - Lists available API features
     */
    void handleSystemCapabilities(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief Get current ISO 8601 timestamp
     */
    QString getCurrentTimestamp() const;
    
    /**
     * @brief Handle errors for input endpoints
     */
    void handleInputError(const std::runtime_error& e, httplib::Response& res);
};

#endif // __FCEUX_API_SERVER_H__