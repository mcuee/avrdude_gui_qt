#include "mainwindow.h"
#include "avrdude_backend.h"
#include "devicedialog.h"
#include "programmerdialog.h"
#include "memoriesdialog.h"
#include "logleveldialog.h"
#include "devinfodialog.h"
#include "helpdialog.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QProgressBar>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QColor>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QFile>
#include <QFileDialog>

// ---------------------------------------------------------------------------
// AvrdudeWorker
// ---------------------------------------------------------------------------
AvrdudeWorker::AvrdudeWorker(QObject *parent) : QThread(parent) {}

void AvrdudeWorker::run()
{
    auto &backend = AvrdudeBackend::instance();
    bool ok = false;

    switch (m_op) {
    case Operation::Connect:
        ok = backend.connectDevice(m_programmer, m_partId, m_port);
        break;

    case Operation::Disconnect:
        backend.disconnectDevice();
        ok = true;
        break;

    case Operation::ReadSignature: {
        QString sig;
        ok = backend.readSignature(sig);
        if (ok) emit logMessage(2, "Signature: " + sig);
        break;
    }

    case Operation::ReadFlash:
        ok = backend.readMemory("flash", m_flashFile);
        break;
    case Operation::WriteFlash:
        ok = backend.writeMemory("flash", m_flashFile);
        break;
    case Operation::VerifyFlash:
        ok = backend.verifyMemory("flash", m_flashFile);
        break;

    case Operation::ReadEeprom:
        ok = backend.readMemory("eeprom", m_eepromFile);
        break;
    case Operation::WriteEeprom:
        ok = backend.writeMemory("eeprom", m_eepromFile);
        break;
    case Operation::VerifyEeprom:
        ok = backend.verifyMemory("eeprom", m_eepromFile);
        break;

    case Operation::ReadFuses: {
        uint8_t lo = 0, hi = 0, ext = 0;
        bool okLo  = backend.readFuse("lfuse", lo);
        bool okHi  = backend.readFuse("hfuse", hi);
        bool okExt = backend.readFuse("efuse", ext);
        ok = okLo || okHi || okExt;  // not all parts have all three
        if (okLo)  emit logMessage(2, QString("LFUSE = 0x%1").arg(lo,  2, 16, QChar('0')));
        if (okHi)  emit logMessage(2, QString("HFUSE = 0x%1").arg(hi,  2, 16, QChar('0')));
        if (okExt) emit logMessage(2, QString("EFUSE = 0x%1").arg(ext, 2, 16, QChar('0')));
        break;
    }

    case Operation::WriteFuses: {
        // m_flashFile is repurposed here as "lfuse,hfuse,efuse" hex triplet,
        // set by MemoriesDialog before emitting operationRequested.
        const QStringList parts = m_flashFile.split(',');
        bool any = false;
        if (parts.size() >= 1 && !parts[0].isEmpty())
            any |= backend.writeFuse("lfuse", static_cast<uint8_t>(parts[0].toUInt(nullptr, 16)));
        if (parts.size() >= 2 && !parts[1].isEmpty())
            any |= backend.writeFuse("hfuse", static_cast<uint8_t>(parts[1].toUInt(nullptr, 16)));
        if (parts.size() >= 3 && !parts[2].isEmpty())
            any |= backend.writeFuse("efuse", static_cast<uint8_t>(parts[2].toUInt(nullptr, 16)));
        ok = any;
        break;
    }
    }

    if (!ok && !backend.lastError().isEmpty())
        emit logMessage(0, backend.lastError());

    emit finished(ok);
}

// ---------------------------------------------------------------------------
// MainWindow
// ---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_worker(new AvrdudeWorker(this))
{
    setWindowTitle("AVRDUDE GUI");
    resize(800, 500);

    setupCentralWidget();
    setupMenuBar();
    setupStatusBar();

    connect(m_worker, &AvrdudeWorker::logMessage, this, &MainWindow::onLogMessage);
    connect(m_worker, &AvrdudeWorker::progress,   this, &MainWindow::onProgress);
    connect(m_worker, &AvrdudeWorker::finished,   this, &MainWindow::onWorkerFinished);

    setConnected(false);
    onLogMessage(2, "AVRDUDE GUI started.");

    // ---- libavrdude backend setup ----
    auto &backend = AvrdudeBackend::instance();

    // Callbacks fire from the worker thread; route them to the GUI thread
    // via Qt::QueuedConnection-equivalent using QMetaObject::invokeMethod.
    backend.setLogCallback([this](LogLevel lvl, const QString &msg) {
        QMetaObject::invokeMethod(this, [this, lvl, msg]() {
            onLogMessage(static_cast<int>(lvl), msg);
        }, Qt::QueuedConnection);
    });
    backend.setProgressCallback([this](const QString &task, int pct) {
        QMetaObject::invokeMethod(this, [this, task, pct]() {
            onProgress(task, pct);
        }, Qt::QueuedConnection);
    });

    if (!backend.loadConfig()) {
        onLogMessage(1, "avrdude.conf not loaded: " + backend.lastError());
        onLogMessage(2, "Device/programmer lists will use built-in fallback entries.");
    }
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// Private setup helpers
// ---------------------------------------------------------------------------
void MainWindow::setupCentralWidget()
{
    auto *central = new QWidget(this);
    auto *layout  = new QVBoxLayout(central);

    m_logView = new QTextEdit(central);
    m_logView->setReadOnly(true);
    m_logView->setFont(QFont("Monospace", 9));

    m_progressBar = new QProgressBar(central);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);

    layout->addWidget(m_logView);
    layout->addWidget(m_progressBar);

    setCentralWidget(central);
}

void MainWindow::setupMenuBar()
{
    // ---- File ----
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("Select &Device…"),     this, &MainWindow::onSelectDevice);
    fileMenu->addAction(tr("Select &Programmer…"), this, &MainWindow::onSelectProgrammer);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Connect"),    this, &MainWindow::onConnect);
    fileMenu->addAction(tr("&Disconnect"), this, &MainWindow::onDisconnect);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), this, &QWidget::close);

    // ---- Device ----
    QMenu *devMenu = menuBar()->addMenu(tr("&Device"));
    devMenu->addAction(tr("Device &Information…"), this, &MainWindow::onDeviceInfo);
    devMenu->addAction(tr("Memory &Operations…"),  this, &MainWindow::onMemoryOps);

    // ---- Settings ----
    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    settingsMenu->addAction(tr("Log &Level…"), this, &MainWindow::onSetLogLevel);

    // ---- Help ----
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&Usage…"), this, &MainWindow::onHelp);
    helpMenu->addAction(tr("&About…"), this, &MainWindow::onAbout);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(tr("Not connected"), this);
    statusBar()->addWidget(m_statusLabel);
}

// ---------------------------------------------------------------------------
// Slot implementations
// ---------------------------------------------------------------------------
void MainWindow::onSelectDevice()
{
    DeviceDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_currentPart   = dlg.selectedPartDesc();
        m_currentPartId = dlg.selectedPartId();
        onLogMessage(2, "Device selected: " + m_currentPart + " (" + m_currentPartId + ")");
    }
}

void MainWindow::onSelectProgrammer()
{
    ProgrammerDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_currentProgrammer = dlg.selectedProgrammer();
        m_currentPort       = dlg.selectedPort();
        onLogMessage(5, QString("Programmer: %1  Port: %2")
                           .arg(m_currentProgrammer, m_currentPort));
    }
}

void MainWindow::onConnect()
{
    if (m_currentPart.isEmpty() || m_currentProgrammer.isEmpty()) {
        QMessageBox::warning(this, tr("Missing configuration"),
                             tr("Please select a device and programmer first."));
        return;
    }
    if (m_worker->isRunning()) return;

    m_worker->setPart(m_currentPart);
    m_worker->setPartId(m_currentPartId);
    m_worker->setProgrammer(m_currentProgrammer);
    m_worker->setPort(m_currentPort);
    m_worker->setOperation(AvrdudeWorker::Operation::Connect);
    m_lastOp = AvrdudeWorker::Operation::Connect;
    m_worker->start();
}

void MainWindow::onDisconnect()
{
    if (m_worker->isRunning()) return;
    m_worker->setOperation(AvrdudeWorker::Operation::Disconnect);
    m_lastOp = AvrdudeWorker::Operation::Disconnect;
    m_worker->start();
}

void MainWindow::onDeviceInfo()
{
    DevInfoDialog dlg(m_currentPart, m_currentPartId, this);
    dlg.exec();
}

void MainWindow::onMemoryOps()
{
    if (!m_connected) {
        QMessageBox::warning(this, tr("Not connected"),
                             tr("Please connect to a device first."));
        return;
    }
    MemoriesDialog dlg(m_currentPart, this);
    connect(&dlg, &MemoriesDialog::operationRequested,
            this, [this](AvrdudeWorker::Operation op, const QString &file) {
        if (m_worker->isRunning()) return;
        m_worker->setOperation(op);
        m_lastOp = op;
        if (op == AvrdudeWorker::Operation::WriteFlash ||
            op == AvrdudeWorker::Operation::VerifyFlash ||
            op == AvrdudeWorker::Operation::WriteFuses)
            m_worker->setFlashFile(file);
        else if (op == AvrdudeWorker::Operation::WriteEeprom ||
                 op == AvrdudeWorker::Operation::VerifyEeprom)
            m_worker->setEepromFile(file);
        m_worker->start();
    });
    dlg.exec();
}

void MainWindow::onSetLogLevel()
{
    LogLevelDialog dlg(m_logLevel, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_logLevel = dlg.logLevel();
        onLogMessage(5, QString("Log level set to %1").arg(m_logLevel));
    }
}

void MainWindow::onHelp()
{
    HelpDialog dlg(this);
    dlg.exec();
}

void MainWindow::onAbout()
{
    AboutDialog dlg(this);
    dlg.exec();
}

// ---------------------------------------------------------------------------
// Worker callbacks
// ---------------------------------------------------------------------------
void MainWindow::onLogMessage(int level, const QString &msg)
{
    if (level > m_logLevel) return;
    applyLogColor(level, msg);
}

void MainWindow::onProgress(const QString &task, int percent)
{
    m_progressBar->setFormat(task + " %p%");
    m_progressBar->setValue(percent);
}

void MainWindow::onWorkerFinished(bool success)
{
    if (m_lastOp == AvrdudeWorker::Operation::Connect)
        setConnected(success);
    else if (m_lastOp == AvrdudeWorker::Operation::Disconnect)
        setConnected(false);

    m_progressBar->setValue(success ? 100 : 0);
    onLogMessage(success ? 2 : 0,
                  success ? "Operation completed." : "Operation failed.");
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void MainWindow::applyLogColor(int level, const QString &msg)
{
    // Match adgui.py colour scheme: ERROR=red, WARNING=orange, INFO=default, DEBUG=gray
    static const QMap<int, QColor> levelColors = {
        {0, Qt::red},
        {1, Qt::red},
        {2, QColor(255, 140, 0)},   // orange – warning
        {3, Qt::black},
        {4, Qt::darkGray},
        {5, Qt::gray},
    };

    QTextCharFormat fmt;
    fmt.setForeground(levelColors.value(qBound(0, level, 5), Qt::black));

    QTextCursor cursor = m_logView->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(msg + "\n", fmt);
    m_logView->setTextCursor(cursor);
    m_logView->ensureCursorVisible();
}

void MainWindow::setConnected(bool connected)
{
    m_connected = connected;
    m_statusLabel->setText(connected
                           ? tr("Connected: %1 via %2 (%3)")
                                 .arg(m_currentPart, m_currentProgrammer, m_currentPort)
                           : tr("Not connected"));
}
