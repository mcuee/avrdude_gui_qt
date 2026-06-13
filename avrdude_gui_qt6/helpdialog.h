#pragma once
// helpdialog.h – mirrors help.ui
#include <QDialog>
class HelpDialog : public QDialog {
    Q_OBJECT
public: explicit HelpDialog(QWidget *parent = nullptr);
};

// aboutdialog.h – mirrors about.ui
class AboutDialog : public QDialog {
    Q_OBJECT
public: explicit AboutDialog(QWidget *parent = nullptr);
};
