#pragma once
// devinfodialog.h – mirrors devinfo.ui
#include <QDialog>

class DevInfoDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DevInfoDialog(const QString &part, QWidget *parent = nullptr);
};
