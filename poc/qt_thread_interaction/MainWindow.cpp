#include "MainWindow.h"
#include "WorkerThread.h"
#include <QVBoxLayout>
#include <QDateTime>
#include <QTimer>
#include <QThread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_emulatorRunning(false)
    , m_frameCount(0)
{
    // Set up UI
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    auto *layout = new QVBoxLayout(centralWidget);
    
    m_statusLabel = new QLabel("Status: Ready", this);
    layout->addWidget(m_statusLabel);
    
    m_logWidget = new QTextEdit(this);
    m_logWidget->setReadOnly(true);
    layout->addWidget(m_logWidget);
    
    setWindowTitle("Qt Thread Interaction PoC");
    resize(600, 400);
    
    // Create and start worker thread
    m_workerThread = new WorkerThread(this);
    connect(m_workerThread, &WorkerThread::requestGuiAction,
            this, &MainWindow::handleWorkerRequest);
    connect(this, &MainWindow::dataReady,
            m_workerThread, &WorkerThread::handleDataReady);
    
    m_workerThread->start();
    
    // Simulate emulator running
    m_emulatorRunning = true;
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [this]() {
        if (m_emulatorRunning) {
            m_frameCount++;
        }
    });
    timer->start(16); // ~60 FPS
    
    log("MainWindow initialized. Thread ID: " + 
        QString::number((qintptr)QThread::currentThreadId()));
}

MainWindow::~MainWindow()
{
    if (m_workerThread) {
        m_workerThread->stop();
        m_workerThread->wait();
    }
}

void MainWindow::updateStatus(const QString &message)
{
    // This method can be safely called from any thread via invokeMethod
    m_statusLabel->setText("Status: " + message);
    log("Status updated from thread " + 
        QString::number((qintptr)QThread::currentThreadId()) + ": " + message);
}

QString MainWindow::getEmulatorState()
{
    // Thread-safe read of emulator state
    QMutexLocker locker(&m_stateMutex);
    
    QString state = QString("Running: %1, Frame: %2")
                    .arg(m_emulatorRunning.load() ? "true" : "false")
                    .arg(m_frameCount.load());
    
    log("State requested from thread " + 
        QString::number((qintptr)QThread::currentThreadId()));
    
    return state;
}

bool MainWindow::performEmulatorAction(const QString &action)
{
    // Thread-safe emulator control
    log("Action requested from thread " + 
        QString::number((qintptr)QThread::currentThreadId()) + ": " + action);
    
    if (action == "pause") {
        m_emulatorRunning = false;
        updateStatus("Paused");
        return true;
    } else if (action == "resume") {
        m_emulatorRunning = true;
        updateStatus("Running");
        return true;
    } else if (action == "reset") {
        QMutexLocker locker(&m_stateMutex);
        m_frameCount = 0;
        updateStatus("Reset");
        return true;
    }
    
    return false;
}

void MainWindow::requestDataWithCallback(int requestId)
{
    // Simulate async operation that sends data back via signal
    log("Data request " + QString::number(requestId) + " received");
    
    // In real implementation, this might read from emulator memory,
    // get save state info, etc.
    QString data = QString("Frame=%1, Running=%2")
                   .arg(m_frameCount.load())
                   .arg(m_emulatorRunning.load());
    
    // Send data back to worker thread
    emit dataReady(requestId, data);
}

void MainWindow::handleWorkerRequest(const QString &request)
{
    log("Worker request received: " + request);
}

void MainWindow::log(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_logWidget->append(timestamp + " [GUI] " + message);
}