#include "helpdialog.h"

#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QDialogButtonBox>

// ---------------------------------------------------------------------------
// HelpDialog
// ---------------------------------------------------------------------------
HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Help – AVRDUDE GUI"));
    resize(520, 400);
    auto *vbox = new QVBoxLayout(this);

    auto *text = new QTextEdit(this);
    text->setReadOnly(true);
    text->setHtml(
        "<h3>AVRDUDE GUI – Quick Start</h3>"
        "<ol>"
        "<li><b>File → Select Device</b> – choose the AVR part you want to program.</li>"
        "<li><b>File → Select Programmer</b> – choose the programmer and communication port.</li>"
        "<li><b>File → Connect</b> – open the connection.  Watch the log for confirmation.</li>"
        "<li><b>Device → Memory Operations</b> – read, write or verify Flash / EEPROM / fuses.</li>"
        "<li><b>File → Disconnect</b> when done.</li>"
        "</ol>"
        "<p>All output from libavrdude appears colour-coded in the log area:<br>"
        "&nbsp;&nbsp;<span style='color:red'>Red</span> = error, "
        "<span style='color:orange'>Orange</span> = warning, "
        "Black = info, Grey = debug.</p>"
        "<p>Adjust verbosity with <b>Settings → Log Level</b>.</p>"
        "<p>For full documentation see the "
        "<a href='https://avrdudes.github.io/avrdude/'>AVRDUDE manual</a>.</p>");

    vbox->addWidget(text);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    vbox->addWidget(btns);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ---------------------------------------------------------------------------
// AboutDialog
// ---------------------------------------------------------------------------
AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("About AVRDUDE GUI"));
    resize(380, 240);
    auto *vbox = new QVBoxLayout(this);

    auto *label = new QLabel(
        "<h2>AVRDUDE GUI (C++ / Qt6)</h2>"
        "<p>C++ port of the original Python/PySide6 GUI<br>"
        "bundled with AVRDUDE ≥ 8.</p>"
        "<p>Built on top of <b>libavrdude</b> and <b>Qt 6</b>.</p>"
        "<p><a href='https://github.com/avrdudes/avrdude'>"
        "github.com/avrdudes/avrdude</a></p>",
        this);
    label->setOpenExternalLinks(true);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);

    vbox->addStretch();
    vbox->addWidget(label);
    vbox->addStretch();

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    vbox->addWidget(btns);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
