#include "WorkerThread.h"
#include <QMetaObject>
#include <QDateTime>
#include <QDebug>
#include <QCoreApplication>

WorkerThread::WorkerThread(QObject *mainWindow, QObject *parent)
    : QThread(parent)
    , m_mainWindow(mainWindow)
    , m_running(true)
    , m_lastRequestId(-1)
{
}

WorkerThread::~WorkerThread()
{
    stop();
    wait();
}

void WorkerThread::stop()
{
    m_running = false;
    m_dataCondition.wakeAll();
}

void WorkerThread::run()
{
    log("Worker thread started. Thread ID: " + 
        QString::number((qintptr)QThread::currentThreadId()));
    
    // Simulate REST API server operations
    while (m_running) {
        // Demonstrate different thread communication methods
        
        msleep(2000); // Wait 2 seconds
        if (!m_running) break;
        
        log("=== Demonstrating Direct Invocation ===");
        demonstrateDirectInvoke();
        
        msleep(2000);
        if (!m_running) break;
        
        log("=== Demonstrating Queued Invocation ===");
        demonstrateQueuedInvoke();
        
        msleep(2000);
        if (!m_running) break;
        
        log("=== Demonstrating Blocking Invocation ===");
        demonstrateBlockingInvoke();
        
        msleep(2000);
        if (!m_running) break;
        
        log("=== Demonstrating Callback Invocation ===");
        demonstrateCallbackInvoke();
        
        msleep(5000); // Wait before next cycle
    }
    
    log("Worker thread stopping");
}

void WorkerThread::demonstrateDirectInvoke()
{
    // Fire-and-forget GUI update
    log("Requesting status update (fire-and-forget)");
    
    QMetaObject::invokeMethod(m_mainWindow, "updateStatus",
                              Qt::QueuedConnection,
                              Q_ARG(QString, "Updated from worker thread"));
}

void WorkerThread::demonstrateQueuedInvoke()
{
    // Request emulator action with queued connection
    log("Requesting emulator pause");
    
    bool result;
    QMetaObject::invokeMethod(m_mainWindow, "performEmulatorAction",
                              Qt::QueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              Q_ARG(QString, "pause"));
    
    // Note: result won't be available immediately with QueuedConnection
    
    msleep(100); // Give time for action to complete
    
    log("Requesting emulator resume");
    QMetaObject::invokeMethod(m_mainWindow, "performEmulatorAction",
                              Qt::QueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              Q_ARG(QString, "resume"));
}

void WorkerThread::demonstrateBlockingInvoke()
{
    // BlockingQueuedConnection - waits for result
    log("Requesting emulator state (blocking)");
    
    QString state;
    bool success = QMetaObject::invokeMethod(m_mainWindow, "getEmulatorState",
                                             Qt::BlockingQueuedConnection,
                                             Q_RETURN_ARG(QString, state));
    
    if (success) {
        log("Received state: " + state);
    } else {
        log("Failed to get state");
    }
}

void WorkerThread::demonstrateCallbackInvoke()
{
    // Request data with callback via signal
    static int requestId = 0;
    requestId++;
    
    log("Requesting data with callback, ID: " + QString::number(requestId));
    
    // Request data
    QMetaObject::invokeMethod(m_mainWindow, "requestDataWithCallback",
                              Qt::QueuedConnection,
                              Q_ARG(int, requestId));
    
    // Wait for response (with timeout)
    QMutexLocker locker(&m_dataMutex);
    if (m_dataCondition.wait(&m_dataMutex, 1000)) {
        if (m_lastRequestId == requestId) {
            log("Received callback data: " + m_lastData);
        }
    } else {
        log("Timeout waiting for callback");
    }
}

void WorkerThread::handleDataReady(int requestId, const QString &data)
{
    QMutexLocker locker(&m_dataMutex);
    m_lastRequestId = requestId;
    m_lastData = data;
    m_dataCondition.wakeAll();
}

void WorkerThread::log(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    qDebug() << timestamp << "[Worker]" << message;
}