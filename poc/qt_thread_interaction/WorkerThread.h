#ifndef WORKERTHREAD_H
#define WORKERTHREAD_H

#include <QThread>
#include <QObject>
#include <QString>
#include <QWaitCondition>
#include <QMutex>
#include <atomic>

class WorkerThread : public QThread
{
    Q_OBJECT

public:
    WorkerThread(QObject *mainWindow, QObject *parent = nullptr);
    ~WorkerThread();

    void stop();

public slots:
    void handleDataReady(int requestId, const QString &data);

signals:
    void requestGuiAction(const QString &request);

protected:
    void run() override;

private:
    QObject *m_mainWindow;
    std::atomic<bool> m_running;
    QWaitCondition m_dataCondition;
    QMutex m_dataMutex;
    QString m_lastData;
    int m_lastRequestId;
    
    // Demonstrate different invocation methods
    void demonstrateDirectInvoke();
    void demonstrateQueuedInvoke();
    void demonstrateBlockingInvoke();
    void demonstrateCallbackInvoke();
    
    void log(const QString &message);
};

#endif // WORKERTHREAD_H