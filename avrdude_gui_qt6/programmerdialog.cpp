#include "programmerdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QLabel>
#include <QGroupBox>

// Representative programmer table  { id, description, protocol }
struct ProgEntry { const char *id; const char *desc; const char *proto; };
static const ProgEntry kProgrammers[] = {
    {"arduino",        "Arduino",                           "ISP"},
    {"avrispmkII",     "Atmel AVR ISP mkII",                "ISP"},
    {"usbasp",         "USBasp (www.fischl.de)",            "ISP"},
    {"usbtiny",        "USBtiny simple USB programmer",     "ISP"},
    {"stk500",         "Atmel STK500",                      "ISP"},
    {"stk500v2",       "Atmel STK500 V2",                   "ISP"},
    {"dragon_isp",     "Atmel AVR Dragon in ISP mode",      "ISP"},
    {"avrisp2",        "Atmel AVR ISP 2",                   "ISP"},
    {"snap_isp",       "Microchip SNAP (ISP mode)",         "ISP"},
    {"jtag2updi",      "JTAGv2 to UPDI bridge",             "UPDI"},
    {"serialupdi",     "Serial UPDI single-wire",           "UPDI"},
    {"pymcuprog",      "Microchip pymcuprog UPDI",          "UPDI"},
    {"atmelice_updi",  "Atmel-ICE (UPDI mode)",             "UPDI"},
    {"snap_updi",      "Microchip SNAP (UPDI mode)",        "UPDI"},
    {"avr910",         "Atmel Low Cost Serial Programmer",  "TPI"},
    {"avrdoper",       "AVR-Doper (HID version)",           "HV"},
    {nullptr, nullptr, nullptr}
};

ProgrammerDialog::ProgrammerDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Select Programmer"));
    resize(380, 480);

    auto *vbox = new QVBoxLayout(this);

    // Protocol filter
    auto *protoRow = new QHBoxLayout;
    protoRow->addWidget(new QLabel(tr("Protocol:")));
    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->addItems({"All", "ISP", "TPI", "PDI", "UPDI", "HV"});
    protoRow->addWidget(m_protocolCombo);
    vbox->addLayout(protoRow);

    // Text filter
    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Filter:")));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Type to filter…"));
    filterRow->addWidget(m_filterEdit);
    vbox->addLayout(filterRow);

    // Programmer list
    m_progList = new QListWidget(this);
    vbox->addWidget(m_progList);

    // Port group
    auto *portGroup = new QGroupBox(tr("Port"), this);
    auto *portForm  = new QFormLayout(portGroup);
    m_portEdit = new QLineEdit(this);
    m_portEdit->setPlaceholderText(tr("e.g. /dev/ttyUSB0 or COM3"));
    portForm->addRow(tr("Port:"), m_portEdit);
    vbox->addWidget(portGroup);

    // Buttons
    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    vbox->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProgrammerDialog::onProtocolChanged);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &ProgrammerDialog::onFilterChanged);

    applyFilter("All", {});
}

ProgrammerDialog::~ProgrammerDialog() = default;

QString ProgrammerDialog::selectedProgrammer() const
{
    auto *item = m_progList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString{};
}

QString ProgrammerDialog::selectedPort() const { return m_portEdit->text(); }

void ProgrammerDialog::onProtocolChanged(int) {
    applyFilter(m_protocolCombo->currentText(), m_filterEdit->text());
}
void ProgrammerDialog::onFilterChanged(const QString &t) {
    applyFilter(m_protocolCombo->currentText(), t);
}

void ProgrammerDialog::applyFilter(const QString &proto, const QString &text)
{
    m_progList->clear();
    for (int i = 0; kProgrammers[i].id; ++i) {
        const auto &e = kProgrammers[i];
        bool protoOk = (proto == "All") ||
                       QString(e.proto).contains(proto, Qt::CaseInsensitive);
        bool textOk  = text.isEmpty() ||
                       QString(e.id).contains(text, Qt::CaseInsensitive) ||
                       QString(e.desc).contains(text, Qt::CaseInsensitive);
        if (protoOk && textOk) {
            auto *item = new QListWidgetItem(
                QString("%1  —  %2").arg(e.id, e.desc));
            item->setData(Qt::UserRole, QString(e.id));
            m_progList->addItem(item);
        }
    }
    if (m_progList->count() > 0)
        m_progList->setCurrentRow(0);
}
