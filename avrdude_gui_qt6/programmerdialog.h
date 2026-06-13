#pragma once
// programmerdialog.h – mirrors adgui.py programmer-selection dialog (programmer.ui)

#include <QDialog>

class QComboBox;
class QLineEdit;
class QListWidget;

class ProgrammerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProgrammerDialog(QWidget *parent = nullptr);
    ~ProgrammerDialog() override;

    QString selectedProgrammer() const;
    QString selectedPort() const;

private slots:
    void onProtocolChanged(int index);
    void onFilterChanged(const QString &text);

private:
    void applyFilter(const QString &protocol, const QString &text);

    QComboBox   *m_protocolCombo{nullptr};
    QLineEdit   *m_filterEdit{nullptr};
    QListWidget *m_progList{nullptr};
    QComboBox   *m_portCombo{nullptr};
    QLineEdit   *m_portEdit{nullptr};
};
