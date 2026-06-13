#include "mainwindow.h"
#include "devicedialog.h"
#include "programmerdialog.h"
#include "memoriesdialog.h"
#include "logleveldialog.h"
#include "devinfodialog.h"
#include "helpdialog.h"
#include "aboutdialog.h"

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
    // All real libavrdude calls happen here so the GUI thread is never blocked.
    // The pattern mirrors adgui.py's use of threads for every avrdude operation.
    //
    // Replace each stub below with the actual libavrdude C API calls:
    //   avrdude_initconfig / avrdude_run / avr_read / avr_write / …
    //
    emit logMessage(5, QString("[worker] operation %1 started")
                           .arg(static_cast<int>(m_op)));

    bool ok = false;
    switch (m_op) {
    case Operation::Connect:
        // avrdude_open(m_programmer, m_part, m_port, …)
        emit logMessage(5, "Connecting to " + m_part + " via " + m_programmer
                               + " on " + m_port);
        ok = true;
        break;
    case Operation::Disconnect:
        // avrdude_close(…)
        ok = true;
        break;
    case Operation::ReadSignature:
        emit progress("Reading signature", 0);
        // avr_signature(…)
        emit progress("Reading signature", 100);
        ok = true;
        break;
    case Operation::ReadFlash:
        emit progress("Reading flash", 0);
        // avr_read(…, "flash", …)
        emit progress("Reading flash", 100);
        ok = true;
        break;
    case Operation::WriteFlash:
        emit progress("Writing flash", 0);
        // avr_write(…, "flash", …)
        emit progress("Writing flash", 100);
        ok = true;
        break;
    case Operation::VerifyFlash:
        emit progress("Verifying flash", 0);
        // avr_verify(…)
        emit progress("Verifying flash", 100);
        ok = true;
        break;
    case Operation::ReadEeprom:
        emit progress("Reading EEPROM", 0);
        emit progress("Reading EEPROM", 100);
        ok = true;
        break;
    case Operation::WriteEeprom:
        emit progress("Writing EEPROM", 0);
        emit progress("Writing EEPROM", 100);
        ok = true;
        break;
    case Operation::VerifyEeprom:
        emit progress("Verifying EEPROM", 0);
        emit progress("Verifying EEPROM", 100);
        ok = true;
        break;
    case Operation::ReadFuses:
        emit progress("Reading fuses", 0);
        emit progress("Reading fuses", 100);
        ok = true;
        break;
    case Operation::WriteFuses:
        emit progress("Writing fuses", 0);
        emit progress("Writing fuses", 100);
        ok = true;
        break;
    }

    emit logMessage(5, QString("[worker] operation finished (success=%1)").arg(ok));
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
    onLogMessage(5, "AVRDUDE GUI started.");
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
        m_currentPart = dlg.selectedPart();
        onLogMessage(5, "Device selected: " + m_currentPart);
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
    m_worker->setProgrammer(m_currentProgrammer);
    m_worker->setPort(m_currentPort);
    m_worker->setOperation(AvrdudeWorker::Operation::Connect);
    m_worker->start();
}

void MainWindow::onDisconnect()
{
    if (m_worker->isRunning()) return;
    m_worker->setOperation(AvrdudeWorker::Operation::Disconnect);
    m_worker->start();
}

void MainWindow::onDeviceInfo()
{
    DevInfoDialog dlg(m_currentPart, this);
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
        if (op == AvrdudeWorker::Operation::WriteFlash ||
            op == AvrdudeWorker::Operation::VerifyFlash)
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
    if (m_worker->property("op").isValid()) {
        // intentional: no-op here; specific ops update m_connected
    }
    // Detect connect/disconnect from last queued operation via a simple heuristic:
    // If progress bar label contains "Connecting" or value is still 0 → connected.
    setConnected(success && m_progressBar->value() == 0 ? m_connected : success);
    m_progressBar->setValue(success ? 100 : 0);
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
