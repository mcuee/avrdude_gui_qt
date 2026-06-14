#include "devinfodialog.h"
#include "avrdude_backend.h"

#include <QVBoxLayout>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QFont>

DevInfoDialog::DevInfoDialog(const QString &partDesc, const QString &partId, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Device Information – %1")
                       .arg(partDesc.isEmpty() ? tr("(none)") : partDesc));
    resize(520, 420);

    auto *vbox = new QVBoxLayout(this);

    auto *text = new QTextEdit(this);
    text->setReadOnly(true);
    text->setFont(QFont("Monospace", 9));

    if (partId.isEmpty()) {
        text->setPlainText(tr("No device selected.\n"
                              "Use File → Select Device to choose a device."));
    } else {
        DeviceDetails d = AvrdudeBackend::instance().partDetails(partId);
        if (d.desc.isEmpty()) {
            text->setPlainText(tr("Could not retrieve details for '%1' from libavrdude.\n"
                                  "Make sure avrdude.conf was loaded successfully "
                                  "(see the log on the main window).").arg(partId));
        } else {
            QString out;
            out += QString("Device:    %1 (id: %2)\n").arg(d.desc, partId);
            out += QString("Signature: %1\n\n").arg(d.signatureExpected);
            out += QString("Flash:     %1 bytes\n").arg(d.flashSize);
            out += QString("EEPROM:    %1 bytes\n\n").arg(d.eepromSize);
            out += "Memories:\n";
            for (const MemoryInfo &m : d.memories) {
                out += QString("  %1%2  size=%3")
                           .arg(m.name, -12)
                           .arg("", 0)
                           .arg(m.size);
                if (m.pageSize > 0)
                    out += QString("  page=%1  pages=%2").arg(m.pageSize).arg(m.numPages);
                out += "\n";
            }
            text->setPlainText(out);
        }
    }

    vbox->addWidget(text);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    vbox->addWidget(btns);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
