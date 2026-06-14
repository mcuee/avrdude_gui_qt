#include "programmerdialog.h"
#include "avrdude_backend.h"

#include <utility>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QLabel>
#include <QGroupBox>

// Programmer list is loaded from libavrdude at construction time and stored
// in m_allProgrammers (see header).

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

    m_allProgrammers = AvrdudeBackend::instance().availableProgrammers();
    if (m_allProgrammers.isEmpty()) {
        // Fallback list for UI testing without HAVE_LIBAVRDUDE
        m_allProgrammers = {
            {"arduino", "Arduino", "ISP"},
            {"usbasp",  "USBasp (www.fischl.de)", "ISP"},
            {"avrispmkII", "Atmel AVR ISP mkII", "ISP"},
        };
    }

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
    for (const ProgrammerInfo &e : std::as_const(m_allProgrammers)) {
        bool protoOk = (proto == "All") ||
                       e.type.contains(proto, Qt::CaseInsensitive) ||
                       e.id.contains(proto, Qt::CaseInsensitive) ||
                       e.desc.contains(proto, Qt::CaseInsensitive);
        bool textOk  = text.isEmpty() ||
                       e.id.contains(text, Qt::CaseInsensitive) ||
                       e.desc.contains(text, Qt::CaseInsensitive);
        if (protoOk && textOk) {
            auto *item = new QListWidgetItem(
                QString("%1  —  %2").arg(e.id, e.desc));
            item->setData(Qt::UserRole, e.id);
            m_progList->addItem(item);
        }
    }
    if (m_progList->count() > 0)
        m_progList->setCurrentRow(0);
}
