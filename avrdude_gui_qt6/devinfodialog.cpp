#include "devinfodialog.h"

#include <QVBoxLayout>
#include <QTextEdit>
#include <QDialogButtonBox>

DevInfoDialog::DevInfoDialog(const QString &part, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Device Information – %1").arg(part.isEmpty() ? tr("(none)") : part));
    resize(500, 380);

    auto *vbox = new QVBoxLayout(this);

    auto *text = new QTextEdit(this);
    text->setReadOnly(true);
    text->setFont(QFont("Monospace", 9));

    if (part.isEmpty()) {
        text->setPlainText(tr("No device selected.\n"
                              "Use File → Select Device to choose a device."));
    } else {
        // At runtime populate this from avr_locate_part() and iterate its
        // memory descriptors and config entries.
        text->setPlainText(
            tr("Device: %1\n\n"
               "[Connect to the device and click Device → Device Information\n"
               " to display live data from libavrdude.]\n\n"
               "Typical information shown here:\n"
               "  - Signature bytes\n"
               "  - Flash / EEPROM / SRAM sizes\n"
               "  - Supported programming modes\n"
               "  - Fuse byte descriptions\n"
               "  - Lock bit descriptions").arg(part));
    }

    vbox->addWidget(text);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    vbox->addWidget(btns);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
