#ifndef __REST_API_SERVER_H__
#define __REST_API_SERVER_H__

#include <QObject>
#include <QString>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <string>

// Forward declaration to avoid including httplib.h in header
namespace httplib {
    class Server;
    struct Request;
    struct Response;
}

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
    int getPort() const { return m_port; }
    void setReadTimeout(int seconds) { m_readTimeoutSec = seconds; }
    void setWriteTimeout(int seconds) { m_writeTimeoutSec = seconds; }

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
    int m_port;
    int m_readTimeoutSec;
    int m_writeTimeoutSec;
    ErrorCode m_lastError;
};

#endif // __REST_API_SERVER_H__