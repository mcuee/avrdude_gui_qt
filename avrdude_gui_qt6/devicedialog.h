#pragma once
// devicedialog.h – mirrors adgui.py device-selection dialog (device.ui)

#include <QDialog>
#include <QStringList>

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

    QString selectedPart() const;

private slots:
    void onFamilyChanged(int index);
    void onFilterChanged(const QString &text);

private:
    void populateDevices();
    void applyFilter(const QString &family, const QString &text);

    QComboBox    *m_familyCombo{nullptr};
    QLineEdit    *m_filterEdit{nullptr};
    QListWidget  *m_deviceList{nullptr};

    // Full device list sourced from libavrdude's avr_find_parts() at runtime.
    // For now populated with representative entries.
    QStringList m_allParts;
};
