#include "FceuxApiServer.h"
#include "../../../lib/httplib.h"
#include "../../../lib/json.hpp"
#include "../../../version.h"
#include <QDateTime>
#include <QtGlobal>

using json = nlohmann::json;

FceuxApiServer::FceuxApiServer(QObject* parent)
    : RestApiServer(parent)
{
}

void FceuxApiServer::registerRoutes()
{
    // System information endpoints
    addGetRoute("/api/system/info", 
        [this](const httplib::Request& req, httplib::Response& res) {
            handleSystemInfo(req, res);
        });

    addGetRoute("/api/system/ping",
        [this](const httplib::Request& req, httplib::Response& res) {
            handleSystemPing(req, res);
        });

    addGetRoute("/api/system/capabilities",
        [this](const httplib::Request& req, httplib::Response& res) {
            handleSystemCapabilities(req, res);
        });
    
    // TODO: Add input validation framework for future POST/PUT endpoints
}

void FceuxApiServer::handleSystemInfo(const httplib::Request& req, httplib::Response& res)
{
    json response;
    
    // FCEUX version information
    response["version"] = FCEU_VERSION_STRING;
    
    // Build date from compile time
    response["build_date"] = __DATE__;
    
    // Qt version
    response["qt_version"] = qVersion();
    
    // API version (hardcoded for now, can be made configurable later)
    response["api_version"] = "1.0.0";
    
    // Platform
#ifdef __linux__
    response["platform"] = "linux";
#elif defined(_WIN32)
    response["platform"] = "windows";
#elif defined(__APPLE__)
    response["platform"] = "macos";
#else
    response["platform"] = "unknown";
#endif

    res.set_content(response.dump(), "application/json");
    res.status = 200;
}

void FceuxApiServer::handleSystemPing(const httplib::Request& req, httplib::Response& res)
{
    json response;
    response["status"] = "ok";
    response["timestamp"] = getCurrentTimestamp().toStdString();
    
    res.set_content(response.dump(), "application/json");
    res.status = 200;
}

void FceuxApiServer::handleSystemCapabilities(const httplib::Request& req, httplib::Response& res)
{
    json response;
    
    // List of available endpoints
    response["endpoints"] = json::array({
        "/api/system/info",
        "/api/system/ping",
        "/api/system/capabilities"
    });
    
    // Feature flags - all false for now as emulation features not yet implemented
    response["features"] = {
        {"emulation_control", false},
        {"memory_access", false},
        {"save_states", false},
        {"screenshots", false}
    };
    
    res.set_content(response.dump(), "application/json");
    res.status = 200;
}

QString FceuxApiServer::getCurrentTimestamp() const
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}