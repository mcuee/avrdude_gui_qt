#pragma once
// devicedialog.h – mirrors adgui.py device-selection dialog (device.ui)

#include "avrdude_backend.h"

#include <QDialog>
#include <QVector>

namespace Ui { class DeviceDialog; }
class QListWidget;
class QComboBox;
class QLineEdit;

class DeviceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DeviceDialog(QWidget *parent = nullptr);
    ~DeviceDialog() override;

    // Returns the short libavrdude id (e.g. "m328p"), suitable for
    // AvrdudeBackend::connectDevice() / partDetails().
    QString selectedPartId() const;
    // Returns the human-readable description (e.g. "ATmega328P").
    QString selectedPartDesc() const;

private slots:
    void onFamilyChanged(int index);
    void onFilterChanged(const QString &text);

private:
    void populateDevices();
    void applyFilter(const QString &family, const QString &text);

    QComboBox    *m_familyCombo{nullptr};
    QLineEdit    *m_filterEdit{nullptr};
    QListWidget  *m_deviceList{nullptr};

    // Sourced from AvrdudeBackend::instance().availableParts()
    QVector<PartInfo> m_allParts;
};
