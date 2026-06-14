#include "memoriesdialog.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QCheckBox>

MemoriesDialog::MemoriesDialog(const QString &part, QWidget *parent)
    : QDialog(parent), m_part(part)
{
    setWindowTitle(tr("Memory Operations – %1").arg(part));
    resize(480, 400);

    auto *vbox = new QVBoxLayout(this);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildSignatureTab(), tr("Signature"));
    tabs->addTab(buildFlashTab(),     tr("Flash"));
    tabs->addTab(buildEepromTab(),    tr("EEPROM"));
    tabs->addTab(buildFusesTab(),     tr("Fuses"));
    vbox->addWidget(tabs);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    vbox->addWidget(btns);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ---------------------------------------------------------------------------
// Signature tab
// ---------------------------------------------------------------------------
QWidget *MemoriesDialog::buildSignatureTab()
{
    auto *w    = new QWidget;
    auto *form = new QFormLayout(w);

    m_sigExpected = new QLineEdit(w);
    m_sigExpected->setReadOnly(true);
    m_sigExpected->setPlaceholderText(tr("(loaded from config)"));

    m_sigRead = new QLineEdit(w);
    m_sigRead->setReadOnly(true);

    form->addRow(tr("Expected:"), m_sigExpected);
    form->addRow(tr("Read:"),     m_sigRead);

    auto *readBtn = new QPushButton(tr("Read Signature"), w);
    form->addRow(readBtn);
    connect(readBtn, &QPushButton::clicked, this, &MemoriesDialog::onReadSignature);
    return w;
}

// ---------------------------------------------------------------------------
// Flash tab
// ---------------------------------------------------------------------------
QWidget *MemoriesDialog::buildFlashTab()
{
    auto *w    = new QWidget;
    auto *vbox = new QVBoxLayout(w);

    auto *fileRow = new QHBoxLayout;
    m_flashFile = new QLineEdit(w);
    m_flashFile->setPlaceholderText(tr("Path to .hex / .elf file"));
    auto *browseBtn = new QPushButton(tr("Browse…"), w);
    fileRow->addWidget(m_flashFile);
    fileRow->addWidget(browseBtn);
    vbox->addLayout(fileRow);

    auto *btnRow = new QHBoxLayout;
    auto *readBtn   = new QPushButton(tr("Read"),   w);
    auto *writeBtn  = new QPushButton(tr("Write"),  w);
    auto *verifyBtn = new QPushButton(tr("Verify"), w);
    btnRow->addWidget(readBtn);
    btnRow->addWidget(writeBtn);
    btnRow->addWidget(verifyBtn);
    vbox->addLayout(btnRow);
    vbox->addStretch();

    connect(browseBtn, &QPushButton::clicked, this, &MemoriesDialog::onBrowseFlash);
    connect(readBtn,   &QPushButton::clicked, this, &MemoriesDialog::onReadFlash);
    connect(writeBtn,  &QPushButton::clicked, this, &MemoriesDialog::onWriteFlash);
    connect(verifyBtn, &QPushButton::clicked, this, &MemoriesDialog::onVerifyFlash);
    return w;
}

// ---------------------------------------------------------------------------
// EEPROM tab
// ---------------------------------------------------------------------------
QWidget *MemoriesDialog::buildEepromTab()
{
    auto *w    = new QWidget;
    auto *vbox = new QVBoxLayout(w);

    auto *fileRow   = new QHBoxLayout;
    m_eepromFile = new QLineEdit(w);
    m_eepromFile->setPlaceholderText(tr("Path to .hex / .eep file"));
    auto *browseBtn = new QPushButton(tr("Browse…"), w);
    fileRow->addWidget(m_eepromFile);
    fileRow->addWidget(browseBtn);
    vbox->addLayout(fileRow);

    auto *btnRow    = new QHBoxLayout;
    auto *readBtn   = new QPushButton(tr("Read"),   w);
    auto *writeBtn  = new QPushButton(tr("Write"),  w);
    auto *verifyBtn = new QPushButton(tr("Verify"), w);
    btnRow->addWidget(readBtn);
    btnRow->addWidget(writeBtn);
    btnRow->addWidget(verifyBtn);
    vbox->addLayout(btnRow);
    vbox->addStretch();

    connect(browseBtn, &QPushButton::clicked, this, &MemoriesDialog::onBrowseEeprom);
    connect(readBtn,   &QPushButton::clicked, this, &MemoriesDialog::onReadEeprom);
    connect(writeBtn,  &QPushButton::clicked, this, &MemoriesDialog::onWriteEeprom);
    connect(verifyBtn, &QPushButton::clicked, this, &MemoriesDialog::onVerifyEeprom);
    return w;
}

// ---------------------------------------------------------------------------
// Fuses tab – mirrors the bit-level fuse editor in adgui.py
// ---------------------------------------------------------------------------
QWidget *MemoriesDialog::buildFusesTab()
{
    auto *w    = new QWidget;
    auto *form = new QFormLayout(w);

    m_lfuse = new QLineEdit("0xFF", w);
    m_hfuse = new QLineEdit("0xD9", w);
    m_efuse = new QLineEdit("0xFF", w);

    m_lfuse->setMaxLength(4);
    m_hfuse->setMaxLength(4);
    m_efuse->setMaxLength(4);

    form->addRow(tr("LFUSE (hex):"), m_lfuse);
    form->addRow(tr("HFUSE (hex):"), m_hfuse);
    form->addRow(tr("EFUSE (hex):"), m_efuse);

    auto *note = new QLabel(
        tr("<small><i>Note: unprogrammed bit = 1, programmed bit = 0 (AVR convention)</i></small>"), w);
    note->setWordWrap(true);
    form->addRow(note);

    auto *btnRow  = new QHBoxLayout;
    auto *readBtn = new QPushButton(tr("Read Fuses"),  w);
    auto *writeBtn= new QPushButton(tr("Write Fuses"), w);
    btnRow->addWidget(readBtn);
    btnRow->addWidget(writeBtn);
    form->addRow(btnRow);

    connect(readBtn,  &QPushButton::clicked, this, &MemoriesDialog::onReadFuses);
    connect(writeBtn, &QPushButton::clicked, this, &MemoriesDialog::onWriteFuses);
    return w;
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void MemoriesDialog::onReadSignature() {
    emit operationRequested(AvrdudeWorker::Operation::ReadSignature, {});
}
void MemoriesDialog::onBrowseFlash() {
    QString f = QFileDialog::getOpenFileName(
        this, tr("Open Flash File"), {},
        tr("Hex Files (*.hex);;ELF Files (*.elf);;All Files (*)"));
    if (!f.isEmpty()) m_flashFile->setText(f);
}
void MemoriesDialog::onReadFlash() {
    QString f = QFileDialog::getSaveFileName(
        this, tr("Save Flash Dump"), {}, tr("Hex Files (*.hex);;All Files (*)"));
    if (!f.isEmpty()) emit operationRequested(AvrdudeWorker::Operation::ReadFlash, f);
}
void MemoriesDialog::onWriteFlash() {
    emit operationRequested(AvrdudeWorker::Operation::WriteFlash, m_flashFile->text());
}
void MemoriesDialog::onVerifyFlash() {
    emit operationRequested(AvrdudeWorker::Operation::VerifyFlash, m_flashFile->text());
}
void MemoriesDialog::onBrowseEeprom() {
    QString f = QFileDialog::getOpenFileName(
        this, tr("Open EEPROM File"), {},
        tr("Hex Files (*.hex);;EEP Files (*.eep);;All Files (*)"));
    if (!f.isEmpty()) m_eepromFile->setText(f);
}
void MemoriesDialog::onReadEeprom() {
    QString f = QFileDialog::getSaveFileName(
        this, tr("Save EEPROM Dump"), {}, tr("Hex Files (*.hex);;All Files (*)"));
    if (!f.isEmpty()) emit operationRequested(AvrdudeWorker::Operation::ReadEeprom, f);
}
void MemoriesDialog::onWriteEeprom() {
    emit operationRequested(AvrdudeWorker::Operation::WriteEeprom, m_eepromFile->text());
}
void MemoriesDialog::onVerifyEeprom() {
    emit operationRequested(AvrdudeWorker::Operation::VerifyEeprom, m_eepromFile->text());
}
void MemoriesDialog::onReadFuses() {
    emit operationRequested(AvrdudeWorker::Operation::ReadFuses, {});
}
void MemoriesDialog::onWriteFuses() {
    QString packed = QString("%1,%2,%3").arg(m_lfuse->text(), m_hfuse->text(), m_efuse->text());
    emit operationRequested(AvrdudeWorker::Operation::WriteFuses, packed);
}
