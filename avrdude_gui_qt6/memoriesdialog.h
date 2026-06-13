#pragma once
// memoriesdialog.h – mirrors adgui.py memories dialog (memories.ui)
// Tabs: Signature | Flash | EEPROM | Fuses

#include "mainwindow.h"   // AvrdudeWorker::Operation

#include <QDialog>

class QTabWidget;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QSpinBox;

class MemoriesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MemoriesDialog(const QString &part, QWidget *parent = nullptr);

signals:
    void operationRequested(AvrdudeWorker::Operation op, const QString &file);

private slots:
    // Signature tab
    void onReadSignature();

    // Flash tab
    void onBrowseFlash();
    void onReadFlash();
    void onWriteFlash();
    void onVerifyFlash();

    // EEPROM tab
    void onBrowseEeprom();
    void onReadEeprom();
    void onWriteEeprom();
    void onVerifyEeprom();

    // Fuses tab
    void onReadFuses();
    void onWriteFuses();

private:
    QWidget *buildSignatureTab();
    QWidget *buildFlashTab();
    QWidget *buildEepromTab();
    QWidget *buildFusesTab();

    QString m_part;

    // Signature
    QLineEdit *m_sigExpected{nullptr};
    QLineEdit *m_sigRead{nullptr};

    // Flash
    QLineEdit *m_flashFile{nullptr};

    // EEPROM
    QLineEdit *m_eepromFile{nullptr};

    // Fuses (lfuse / hfuse / efuse hex spinboxes)
    QLineEdit *m_lfuse{nullptr};
    QLineEdit *m_hfuse{nullptr};
    QLineEdit *m_efuse{nullptr};
};
