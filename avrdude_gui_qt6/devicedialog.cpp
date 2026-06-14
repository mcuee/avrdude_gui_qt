#include "devicedialog.h"
#include "avrdude_backend.h"

#include <algorithm>
#include <utility>
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

    // ---- Populate device list from libavrdude ----
    m_allParts = AvrdudeBackend::instance().availableParts();
    if (m_allParts.isEmpty()) {
        // Fallback so the dialog is still usable if config wasn't loaded
        // (e.g. running without HAVE_LIBAVRDUDE for UI testing).
        const QStringList fallback = {
            "ATmega328P", "ATmega2560", "ATmega168", "ATtiny85", "ATtiny2313",
        };
        for (const auto &desc : fallback)
            m_allParts.push_back(PartInfo{desc.toLower(), desc});
    }
    std::sort(m_allParts.begin(), m_allParts.end(),
              [](const PartInfo &a, const PartInfo &b) {
                  return a.desc.compare(b.desc, Qt::CaseInsensitive) < 0;
              });

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

QString DeviceDialog::selectedPartId() const
{
    auto *item = m_deviceList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString{};
}

QString DeviceDialog::selectedPartDesc() const
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
    for (const PartInfo &part : std::as_const(m_allParts)) {
        bool familyOk = (family == "All") || part.desc.startsWith(family, Qt::CaseInsensitive);
        bool textOk   = text.isEmpty()
                        || part.desc.contains(text, Qt::CaseInsensitive)
                        || part.id.contains(text, Qt::CaseInsensitive);
        if (familyOk && textOk) {
            auto *item = new QListWidgetItem(part.desc);
            item->setData(Qt::UserRole, part.id);
            m_deviceList->addItem(item);
        }
    }
    if (m_deviceList->count() > 0)
        m_deviceList->setCurrentRow(0);
}
