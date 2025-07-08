#include "RestApiServer.h"
#include "../../../lib/httplib.h"
#include <iostream>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <thread>

RestApiServer::RestApiServer(QObject* parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_running(false)
    , m_lastError(ErrorCode::None)
{
    // m_config is initialized with defaults from the struct
}

RestApiServer::~RestApiServer()
{
    stop();
}

void RestApiServer::setConfig(const RestApiConfig& config)
{
    if (!m_running.load()) {
        m_config = config;
    }
}

bool RestApiServer::start(int port)
{
    if (m_running.load()) {
        m_lastError = ErrorCode::AlreadyRunning;
        emit errorOccurred(errorCodeToString(m_lastError));
        return false;
    }

    // Override port if specified
    if (port != 8080) {
        m_config.port = port;
    }
    
    m_lastError = ErrorCode::None;

    try {
        // Create server instance
        m_server = std::unique_ptr<httplib::Server>(new httplib::Server());

        // Configure timeouts
        m_server->set_read_timeout(m_config.readTimeoutSec, 0);
        m_server->set_write_timeout(m_config.writeTimeoutSec, 0);

        // Setup default routes
        setupDefaultRoutes();

        // Allow subclasses to register their routes
        registerRoutes();

        // Reset promise for new startup
        m_startupPromise = std::promise<bool>();
        std::future<bool> startupFuture = m_startupPromise.get_future();

        // Start server thread
        m_running = true;
        m_serverThread = std::thread(&RestApiServer::serverThreadFunc, this);

        // Wait for server to start with timeout
        try {
            auto status = startupFuture.wait_for(std::chrono::seconds(m_config.startupTimeoutSec));
            
            if (status == std::future_status::ready) {
                bool success = startupFuture.get();
                if (success) {
                    emit serverStarted();
                    return true;
                } else {
                    // Error already set and signal emitted in serverThreadFunc
                    return false;
                }
            } else {
                // Timeout waiting for server to start
                m_running = false;
                m_lastError = ErrorCode::ThreadStartFailed;
                emit errorOccurred("Server startup timed out");
                
                // Clean up the thread
                if (m_serverThread.joinable()) {
                    m_serverThread.join();
                }
                return false;
            }
        } catch (const std::exception& e) {
            m_running = false;
            m_lastError = ErrorCode::Unknown;
            QString error = QString("Server startup failed: %1").arg(e.what());
            emit errorOccurred(error);
            return false;
        }

    } catch (const std::exception& e) {
        m_lastError = ErrorCode::Unknown;
        QString error = QString("Failed to start server: %1").arg(e.what());
        emit errorOccurred(error);
        m_running = false;
        return false;
    }
}

void RestApiServer::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_running = false;

    if (m_server) {
        m_server->stop();
    }

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    m_server.reset();
    emit serverStopped();
}

bool RestApiServer::isRunning() const
{
    return m_running.load();
}

void RestApiServer::registerRoutes()
{
    // Default implementation does nothing
    // Subclasses can override to add custom routes
}

void RestApiServer::addGetRoute(const std::string& pattern, 
    std::function<void(const httplib::Request&, httplib::Response&)> handler)
{
    if (m_server) {
        m_server->Get(pattern, handler);
        // Debug output
        printf("REST API: Registered GET route: %s\n", pattern.c_str());
    } else {
        printf("REST API: ERROR - Cannot register GET route %s - server is null\n", pattern.c_str());
    }
}

void RestApiServer::addPostRoute(const std::string& pattern,
    std::function<void(const httplib::Request&, httplib::Response&)> handler)
{
    if (m_server) {
        // WORKAROUND: Store POST handlers in our map for manual routing
        // This bypasses httplib's POST routing which fails in Qt environment
        m_postHandlers[pattern] = handler;
        
        // Still register with httplib for future compatibility when issue is fixed
        m_server->Post(pattern, handler);
        
        // Debug output
        printf("REST API: Registered POST route: %s\n", pattern.c_str());
    } else {
        printf("REST API: ERROR - Cannot register POST route %s - server is null\n", pattern.c_str());
    }
}

void RestApiServer::addPutRoute(const std::string& pattern,
    std::function<void(const httplib::Request&, httplib::Response&)> handler)
{
    if (m_server) {
        m_server->Put(pattern, handler);
    }
}

void RestApiServer::addDeleteRoute(const std::string& pattern,
    std::function<void(const httplib::Request&, httplib::Response&)> handler)
{
    if (m_server) {
        m_server->Delete(pattern, handler);
    }
}

void RestApiServer::serverThreadFunc()
{
    if (!m_server) {
        m_running = false;
        try {
            m_startupPromise.set_value(false);
        } catch (const std::future_error&) {
            // Promise already set, ignore
        }
        return;
    }

    // First, try to bind to the port
    if (!m_server->bind_to_port(m_config.bindAddress.toStdString().c_str(), m_config.port)) {
        m_running = false;
        
        // Check if port is in use
        int savedErrno = errno;
        if (savedErrno == EADDRINUSE) {
            m_lastError = ErrorCode::PortInUse;
            QString error = QString("Port %1 is already in use").arg(m_config.port);
            emit errorOccurred(error);
        } else {
            m_lastError = ErrorCode::BindFailed;
            QString error = QString("Failed to bind to %1:%2 - %3")
                .arg(m_config.bindAddress)
                .arg(m_config.port)
                .arg(std::strerror(savedErrno));
            emit errorOccurred(error);
        }
        
        try {
            m_startupPromise.set_value(false);
        } catch (const std::future_error&) {
            // Promise already set, ignore
        }
        return;
    }
    
    // Binding succeeded, notify that server is ready
    try {
        m_startupPromise.set_value(true);
    } catch (const std::future_error&) {
        // Promise already set, ignore
    }
    
    // Now start listening (this will block until stop() is called)
    m_server->listen_after_bind();
    
    m_running = false;
}

void RestApiServer::setupDefaultRoutes()
{
    if (!m_server) {
        return;
    }

    // Error handler for 404
    m_server->set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        printf("REST API: Error handler called for %s %s (status: %d)\n", 
               req.method.c_str(), req.path.c_str(), res.status);
        
        // Only set 404 if status is not already set
        if (res.status == -1 || res.status == 0) {
            res.status = 404;
        }
        
        const char* fmt = R"({"error": "Not Found", "path": "%s", "method": "%s", "status": %d})";
        char buf[512];
        snprintf(buf, sizeof(buf), fmt, req.path.c_str(), req.method.c_str(), res.status);
        res.set_content(buf, "application/json");
    });

    // WORKAROUND for httplib v0.22.0 POST route issue:
    // In the FCEUX/Qt environment, httplib rejects POST requests with 400 status
    // before route matching. We intercept POST requests here and route manually.
    // See docs/REST_API_POST_ROUTE_FIX.md for details.
    m_postHandlers.clear();
    
    // Pre-routing handler for request inspection and POST workaround
    m_server->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        // WORKAROUND: Manually handle POST requests due to httplib issue
        if (req.method == "POST") {
            // Check our stored POST handlers
            auto it = m_postHandlers.find(req.path);
            if (it != m_postHandlers.end()) {
                it->second(req, res);
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // Exception handler
    m_server->set_exception_handler([](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            const char* fmt = R"({"error": "Internal Server Error", "message": "%s"})";
            char buf[512];
            snprintf(buf, sizeof(buf), fmt, e.what());
            res.status = 500;
            res.set_content(buf, "application/json");
        } catch (...) {
            res.status = 500;
            res.set_content(R"({"error": "Internal Server Error"})", "application/json");
        }
    });
    
    // CRITICAL FIX: Set payload max length to allow POST requests
    // httplib by default might reject POST requests without proper content length
    m_server->set_payload_max_length(1024 * 1024); // 1MB max
}

QString RestApiServer::errorCodeToString(ErrorCode code) const
{
    switch (code) {
        case ErrorCode::None:
            return "No error";
        case ErrorCode::PortInUse:
            return QString("Port %1 is already in use").arg(m_config.port);
        case ErrorCode::BindFailed:
            return QString("Failed to bind to port %1").arg(m_config.port);
        case ErrorCode::ThreadStartFailed:
            return "Failed to start server thread";
        case ErrorCode::AlreadyRunning:
            return "Server is already running";
        case ErrorCode::NotRunning:
            return "Server is not running";
        case ErrorCode::Unknown:
        default:
            return "Unknown error";
    }
}