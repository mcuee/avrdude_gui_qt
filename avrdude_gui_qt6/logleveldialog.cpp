#include "logleveldialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QDialogButtonBox>

static const char *kLevelNames[] = {
    "0 – Error", "1 – Warning", "2 – Info", "3 – Debug",
    "4 – Trace", "5 – Verbose"
};

LogLevelDialog::LogLevelDialog(int currentLevel, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Log Level"));
    auto *vbox = new QVBoxLayout(this);

    vbox->addWidget(new QLabel(tr("Select verbosity level:")));

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 5);
    m_slider->setValue(qBound(0, currentLevel, 5));
    m_slider->setTickPosition(QSlider::TicksBelow);
    m_slider->setTickInterval(1);
    vbox->addWidget(m_slider);

    m_label = new QLabel(kLevelNames[m_slider->value()], this);
    m_label->setAlignment(Qt::AlignCenter);
    vbox->addWidget(m_label);

    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        m_label->setText(kLevelNames[v]);
    });

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    vbox->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

int LogLevelDialog::logLevel() const { return m_slider->value(); }
