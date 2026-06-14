#pragma once

#include <QMainWindow>
#include <QThread>
#include <QMutex>

// Forward declarations
namespace Ui { class MainWindow; }
class QLabel;
class QProgressBar;
class QTextEdit;

// ---------------------------------------------------------------------------
// Worker thread – wraps libavrdude operations so the GUI stays responsive
// ---------------------------------------------------------------------------
class AvrdudeWorker : public QThread
{
    Q_OBJECT
public:
    enum class Operation { Connect, Disconnect, ReadSignature,
                           ReadFlash, WriteFlash, VerifyFlash,
                           ReadEeprom, WriteEeprom, VerifyEeprom,
                           ReadFuses, WriteFuses };

    explicit AvrdudeWorker(QObject *parent = nullptr);

    void setOperation(Operation op)        { m_op = op; }
    void setFlashFile(const QString &f)    { m_flashFile = f; }
    void setEepromFile(const QString &f)   { m_eepromFile = f; }
    void setPart(const QString &p)         { m_part = p; }
    void setPartId(const QString &id)      { m_partId = id; }
    void setProgrammer(const QString &pg)  { m_programmer = pg; }
    void setPort(const QString &port)      { m_port = port; }

signals:
    void logMessage(int level, const QString &msg);
    void progress(const QString &task, int percent);
    void finished(bool success);

protected:
    void run() override;

private:
    Operation m_op{Operation::Connect};
    QString   m_flashFile, m_eepromFile;
    QString   m_part, m_partId, m_programmer, m_port;
};

// ---------------------------------------------------------------------------
// Main application window
// ---------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // File menu
    void onSelectDevice();
    void onSelectProgrammer();
    void onConnect();
    void onDisconnect();

    // Device menu
    void onDeviceInfo();
    void onMemoryOps();

    // Settings menu
    void onSetLogLevel();

    // Help menu
    void onHelp();
    void onAbout();

    // Worker callbacks
    void onLogMessage(int level, const QString &msg);
    void onProgress(const QString &task, int percent);
    void onWorkerFinished(bool success);

private:
    void setupMenuBar();
    void setupStatusBar();
    void setupCentralWidget();
    void applyLogColor(int level, const QString &msg);
    void setConnected(bool connected);

    Ui::MainWindow *ui{nullptr};

    // Widgets created programmatically (fallback when no .ui loader)
    QTextEdit    *m_logView{nullptr};
    QProgressBar *m_progressBar{nullptr};
    QLabel       *m_statusLabel{nullptr};

    // State
    QString m_currentPart;       // human-readable description (display)
    QString m_currentPartId;     // libavrdude short id (for backend calls)
    QString m_currentProgrammer; // libavrdude programmer id
    QString m_currentPort;
    int     m_logLevel{5};
    bool    m_connected{false};

    AvrdudeWorker *m_worker{nullptr};
    AvrdudeWorker::Operation m_lastOp{AvrdudeWorker::Operation::Connect};
};
