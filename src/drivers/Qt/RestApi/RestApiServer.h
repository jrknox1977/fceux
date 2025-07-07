#ifndef __REST_API_SERVER_H__
#define __REST_API_SERVER_H__

#include <QObject>
#include <QString>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <string>
#include <future>
#include <mutex>

// Forward declaration to avoid including httplib.h in header
namespace httplib {
    class Server;
    struct Request;
    struct Response;
}

/**
 * @brief HTTP REST API Server with Qt integration
 * 
 * Thread Safety: This class uses Qt signals which are thread-safe. Signals emitted
 * from the server thread (errorOccurred) use Qt's default queued connections when
 * connecting to slots in different threads, ensuring thread-safe communication.
 * See Qt documentation on signal/slot connections across threads.
 */
struct RestApiConfig {
    QString bindAddress = "127.0.0.1";
    int port = 8080;
    int readTimeoutSec = 5;
    int writeTimeoutSec = 5;
    int startupTimeoutSec = 10;
};

class RestApiServer : public QObject
{
    Q_OBJECT

public:
    enum class ErrorCode {
        None = 0,
        PortInUse,
        BindFailed,
        ThreadStartFailed,
        AlreadyRunning,
        NotRunning,
        Unknown
    };

    explicit RestApiServer(QObject* parent = nullptr);
    virtual ~RestApiServer();

    // Server lifecycle management
    bool start(int port = 8080);
    void stop();
    bool isRunning() const;

    // Configuration
    void setConfig(const RestApiConfig& config);
    const RestApiConfig& getConfig() const { return m_config; }
    
    // Deprecated individual setters (kept for compatibility)
    int getPort() const { return m_config.port; }
    void setReadTimeout(int seconds) { m_config.readTimeoutSec = seconds; }
    void setWriteTimeout(int seconds) { m_config.writeTimeoutSec = seconds; }
    void setStartupTimeout(int seconds) { m_config.startupTimeoutSec = seconds; }

signals:
    void serverStarted();
    void serverStopped();
    void errorOccurred(const QString& error);

protected:
    // Virtual method for subclasses to register routes
    virtual void registerRoutes();

    // Helper method for subclasses to add routes
    void addGetRoute(const std::string& pattern, std::function<void(const httplib::Request&, httplib::Response&)> handler);
    void addPostRoute(const std::string& pattern, std::function<void(const httplib::Request&, httplib::Response&)> handler);
    void addPutRoute(const std::string& pattern, std::function<void(const httplib::Request&, httplib::Response&)> handler);
    void addDeleteRoute(const std::string& pattern, std::function<void(const httplib::Request&, httplib::Response&)> handler);

private:
    void serverThreadFunc();
    void setupDefaultRoutes();
    QString errorCodeToString(ErrorCode code) const;

private:
    std::unique_ptr<httplib::Server> m_server;
    std::thread m_serverThread;
    std::atomic<bool> m_running;
    std::promise<bool> m_startupPromise;
    RestApiConfig m_config;
    ErrorCode m_lastError;
};

#endif // __REST_API_SERVER_H__