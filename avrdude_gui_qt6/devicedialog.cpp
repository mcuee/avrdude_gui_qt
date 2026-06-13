#include "devicedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QLabel>

// ---------------------------------------------------------------------------
DeviceDialog::DeviceDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Select Device"));
    resize(360, 440);

    // ---- Populate representative device list ----
    // At runtime replace this with a call to avr_find_parts() via libavrdude
    m_allParts = {
        "ATmega328P", "ATmega328PB", "ATmega2560", "ATmega1280",
        "ATmega168", "ATmega168P", "ATmega8", "ATmega16", "ATmega32",
        "ATmega48", "ATmega88", "ATmega48P", "ATmega88P",
        "ATmega1284P", "ATmega644P", "ATmega128",
        "ATtiny13",  "ATtiny13A", "ATtiny25",  "ATtiny45",  "ATtiny85",
        "ATtiny24",  "ATtiny44",  "ATtiny84",  "ATtiny2313", "ATtiny4313",
        "ATtiny261", "ATtiny461", "ATtiny861",
        "AT90S1200", "AT90S2313", "AT90S8515",
        "ATxmega128A1", "ATxmega64A1",
    };
    m_allParts.sort(Qt::CaseInsensitive);

    // ---- Layout ----
    auto *vbox = new QVBoxLayout(this);

    // Family filter
    auto *famRow = new QHBoxLayout;
    famRow->addWidget(new QLabel(tr("Family:"), this));
    m_familyCombo = new QComboBox(this);
    m_familyCombo->addItems({"All", "AT90", "ATmega", "ATtiny", "ATxmega"});
    famRow->addWidget(m_familyCombo);
    vbox->addLayout(famRow);

    // Text filter
    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Filter:"), this));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Type to filter…"));
    filterRow->addWidget(m_filterEdit);
    vbox->addLayout(filterRow);

    // Device list
    m_deviceList = new QListWidget(this);
    vbox->addWidget(m_deviceList);

    // OK / Cancel
    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    vbox->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Connections
    connect(m_familyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DeviceDialog::onFamilyChanged);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &DeviceDialog::onFilterChanged);

    populateDevices();
}

DeviceDialog::~DeviceDialog() = default;

QString DeviceDialog::selectedPart() const
{
    auto *item = m_deviceList->currentItem();
    return item ? item->text() : QString{};
}

void DeviceDialog::onFamilyChanged(int /*index*/)
{
    applyFilter(m_familyCombo->currentText(), m_filterEdit->text());
}

void DeviceDialog::onFilterChanged(const QString &text)
{
    applyFilter(m_familyCombo->currentText(), text);
}

void DeviceDialog::populateDevices()
{
    applyFilter("All", QString{});
}

void DeviceDialog::applyFilter(const QString &family, const QString &text)
{
    m_deviceList->clear();
    for (const QString &part : std::as_const(m_allParts)) {
        bool familyOk = (family == "All") || part.startsWith(family, Qt::CaseInsensitive);
        bool textOk   = text.isEmpty() || part.contains(text, Qt::CaseInsensitive);
        if (familyOk && textOk)
            m_deviceList->addItem(part);
    }
    if (m_deviceList->count() > 0)
        m_deviceList->setCurrentRow(0);
}
