#pragma once
// logleveldialog.h – mirrors loglevel.ui
#include <QDialog>
class QSlider;
class QLabel;

class LogLevelDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LogLevelDialog(int currentLevel, QWidget *parent = nullptr);
    int logLevel() const;
private:
    QSlider *m_slider{nullptr};
    QLabel  *m_label{nullptr};
};
