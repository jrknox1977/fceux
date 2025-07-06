#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QLabel>
#include <QMutex>
#include <QString>
#include <atomic>

class WorkerThread;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Thread-safe methods that can be called via QMetaObject::invokeMethod
    Q_INVOKABLE void updateStatus(const QString &message);
    Q_INVOKABLE QString getEmulatorState();
    Q_INVOKABLE bool performEmulatorAction(const QString &action);
    Q_INVOKABLE void requestDataWithCallback(int requestId);

signals:
    // Signal to send data back to worker thread
    void dataReady(int requestId, const QString &data);

private slots:
    // Internal slot for handling worker requests
    void handleWorkerRequest(const QString &request);

private:
    QTextEdit *m_logWidget;
    QLabel *m_statusLabel;
    WorkerThread *m_workerThread;
    
    // Simulated emulator state
    std::atomic<bool> m_emulatorRunning;
    std::atomic<int> m_frameCount;
    QMutex m_stateMutex;
    
    void log(const QString &message);
};

#endif // MAINWINDOW_H