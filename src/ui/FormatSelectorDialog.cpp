#include "FormatSelectorDialog.h"
#include "PresetEditorDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPushButton>
#include <QCheckBox>

namespace uc::ui {

FormatSelectorDialog::FormatSelectorDialog(const QString& sourceFormatId, QWidget* parent, int batchCount)
    : QDialog(parent)
    , m_sourceFormatId(sourceFormatId)
{
    if (batchCount > 1) {
        setWindowTitle(QString("Batch Conversion - Select Target Format (%1 files)").arg(batchCount));
    } else {
        setWindowTitle("Select Target Format & Presets");
    }
    resize(560, 640);
    setStyleSheet("background-color: #0f172a; color: #f8fafc; font-size: 13px;");

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(18, 18, 18, 18);

    const auto* srcFmt = core::FormatRegistry::instance().findById(sourceFormatId);
    const QString srcName = srcFmt ? srcFmt->name : sourceFormatId.toUpper();

    m_sourceInfoLabel = new QLabel(QString("Source format: <b>%1</b> (.%2)").arg(srcName, sourceFormatId), this);
    m_sourceInfoLabel->setStyleSheet("color: #94a3b8; font-size: 13px;");
    layout->addWidget(m_sourceInfoLabel);

    setupPresetSection(layout);

    auto* searchLayout = new QVBoxLayout();
    auto* searchTitle = new QLabel("All Compatible Formats", this);
    searchTitle->setStyleSheet("font-weight: 600; color: #e2e8f0;");
    searchLayout->addWidget(searchTitle);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search target format (e.g. mp4, webp, pdf, flac)...");
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "    background-color: #1e293b;"
        "    border: 1px solid #334155;"
        "    border-radius: 6px;"
        "    padding: 7px 10px;"
        "    color: #f8fafc;"
        "}"
        "QLineEdit:focus {"
        "    border-color: #10b981;"
        "}"
    );
    searchLayout->addWidget(m_searchEdit);
    layout->addLayout(searchLayout);

    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet(
        "QListWidget {"
        "    background-color: #090d16;"
        "    border: 1px solid #334155;"
        "    border-radius: 6px;"
        "    padding: 6px;"
        "}"
        "QListWidget::item {"
        "    padding: 8px 12px;"
        "    border-radius: 4px;"
        "    color: #f8fafc;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #1e293b;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #10b981;"
        "    color: #090d16;"
        "    font-weight: bold;"
        "}"
    );
    layout->addWidget(m_listWidget, 1);

    m_detailLabel = new QLabel(this);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setStyleSheet("background-color: #1e293b; border-radius: 6px; padding: 10px; color: #94a3b8; font-size: 12px;");
    layout->addWidget(m_detailLabel);

    if (batchCount > 1) {
        m_applyToAllCheck = new QCheckBox(
            QString("Apply this output format and preset to all remaining compatible files (%1 files)").arg(batchCount), this);
        m_applyToAllCheck->setChecked(true); // Checked by default for convenient batch conversion
        m_applyToAllCheck->setStyleSheet(
            "QCheckBox {"
            "    color: #38bdf8;"
            "    font-weight: 600;"
            "    padding: 4px 0px;"
            "}"
            "QCheckBox::indicator:checked {"
            "    background-color: #10b981;"
            "    border: 1px solid #10b981;"
            "}"
        );
        layout->addWidget(m_applyToAllCheck);
    }

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        auto* item = m_listWidget->currentItem();
        if (item) {
            m_selectedFormatId = item->data(Qt::UserRole).toString();
            if (m_applyToAllCheck) {
                m_applyToAllRemaining = m_applyToAllCheck->isChecked();
            }
            accept();
        }
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &FormatSelectorDialog::onFilterTextChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &FormatSelectorDialog::onItemDoubleClicked);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &FormatSelectorDialog::onItemSelectionChanged);

    populateFormats();
}

void FormatSelectorDialog::setupPresetSection(QVBoxLayout* layout) {
    const auto* srcFmt = core::FormatRegistry::instance().findById(m_sourceFormatId);
    if (!srcFmt) return;

    const auto presets = core::PresetRegistry::instance().presetsForCategory(srcFmt->category);
    if (presets.isEmpty()) return;

    auto* presetBox = new QGroupBox("Conversion Presets", this);
    presetBox->setStyleSheet(
        "QGroupBox {"
        "    border: 1px solid #334155;"
        "    border-radius: 6px;"
        "    margin-top: 10px;"
        "    padding-top: 10px;"
        "    font-weight: 600;"
        "    color: #38bdf8;"
        "}"
    );
    auto* boxLayout = new QVBoxLayout(presetBox);

    auto* comboRow = new QHBoxLayout();
    m_presetCombo = new QComboBox(presetBox);
    comboRow->addWidget(m_presetCombo, 1);

    m_editPresetBtn = new QPushButton("Edit", presetBox);
    m_editPresetBtn->setEnabled(false);
    m_editPresetBtn->setStyleSheet("background-color: #475569; color: white; padding: 6px 12px; border-radius: 4px; font-weight: bold;");
    connect(m_editPresetBtn, &QPushButton::clicked, this, &FormatSelectorDialog::onEditPresetClicked);
    comboRow->addWidget(m_editPresetBtn);

    m_deletePresetBtn = new QPushButton("Delete", presetBox);
    m_deletePresetBtn->setEnabled(false);
    m_deletePresetBtn->setStyleSheet("background-color: #dc2626; color: white; padding: 6px 12px; border-radius: 4px; font-weight: bold;");
    connect(m_deletePresetBtn, &QPushButton::clicked, this, &FormatSelectorDialog::onDeletePresetClicked);
    comboRow->addWidget(m_deletePresetBtn);

    auto* newPresetBtn = new QPushButton("+ New", presetBox);
    newPresetBtn->setStyleSheet("background-color: #3b82f6; color: white; padding: 6px 12px; border-radius: 4px; font-weight: bold;");
    connect(newPresetBtn, &QPushButton::clicked, this, &FormatSelectorDialog::onNewPresetClicked);
    comboRow->addWidget(newPresetBtn);

    boxLayout->addLayout(comboRow);

    m_presetDescLabel = new QLabel(presetBox);
    m_presetDescLabel->setStyleSheet("color: #94a3b8; font-size: 11px;");
    m_presetDescLabel->setWordWrap(true);
    boxLayout->addWidget(m_presetDescLabel);

    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatSelectorDialog::onPresetSelected);

    layout->addWidget(presetBox);
    refreshPresets();
}

void FormatSelectorDialog::refreshPresets() {
    if (!m_presetCombo) return;
    m_presetCombo->clear();
    m_presetCombo->addItem("-- Select a Quick Preset (Optional) --", "");

    const auto* srcFmt = core::FormatRegistry::instance().findById(m_sourceFormatId);
    if (!srcFmt) return;

    const auto presets = core::PresetRegistry::instance().presetsForCategory(srcFmt->category);
    for (const auto& p : presets) {
        const QString icon = p.isBuiltIn ? "⚡" : "⭐";
        m_presetCombo->addItem(QString("%1 %2 (%3)").arg(icon, p.name, p.targetFormatId.toUpper()), p.id);
    }

    if (m_editPresetBtn) m_editPresetBtn->setEnabled(false);
    if (m_deletePresetBtn) m_deletePresetBtn->setEnabled(false);
}

void FormatSelectorDialog::onNewPresetClicked() {
    const auto* srcFmt = core::FormatRegistry::instance().findById(m_sourceFormatId);
    const auto cat = srcFmt ? srcFmt->category : core::MediaCategory::Video;

    PresetEditorDialog dlg(cat, nullptr, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshPresets();
        // Select newly created preset
        const auto p = dlg.preset();
        for (int i = 0; i < m_presetCombo->count(); ++i) {
            if (m_presetCombo->itemData(i).toString() == p.id) {
                m_presetCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void FormatSelectorDialog::onEditPresetClicked() {
    if (!m_presetCombo || m_presetCombo->currentIndex() <= 0) return;
    const QString presetId = m_presetCombo->currentData().toString();
    const auto* preset = core::PresetRegistry::instance().findById(presetId);
    if (!preset || preset->isBuiltIn) return;

    const auto* srcFmt = core::FormatRegistry::instance().findById(m_sourceFormatId);
    const auto cat = srcFmt ? srcFmt->category : core::MediaCategory::Video;

    PresetEditorDialog dlg(cat, preset, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshPresets();
        for (int i = 0; i < m_presetCombo->count(); ++i) {
            if (m_presetCombo->itemData(i).toString() == presetId) {
                m_presetCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void FormatSelectorDialog::onDeletePresetClicked() {
    if (!m_presetCombo || m_presetCombo->currentIndex() <= 0) return;
    const QString presetId = m_presetCombo->currentData().toString();
    const auto* preset = core::PresetRegistry::instance().findById(presetId);
    if (!preset || preset->isBuiltIn) return;

    core::PresetRegistry::instance().removeCustomPreset(presetId);
    m_selectedPresetId.clear();
    m_selectedPresetOptions.clear();
    refreshPresets();
}

void FormatSelectorDialog::onPresetSelected(int index) {
    if (!m_presetCombo || index <= 0) {
        if (m_presetDescLabel) m_presetDescLabel->clear();
        m_selectedPresetId.clear();
        m_selectedPresetOptions.clear();
        if (m_editPresetBtn) m_editPresetBtn->setEnabled(false);
        if (m_deletePresetBtn) m_deletePresetBtn->setEnabled(false);
        return;
    }

    const QString presetId = m_presetCombo->currentData().toString();
    const auto* preset = core::PresetRegistry::instance().findById(presetId);
    if (!preset) return;

    m_selectedPresetId = preset->id;
    m_selectedPresetOptions = preset->options;
    m_presetDescLabel->setText(preset->description);

    if (m_editPresetBtn) m_editPresetBtn->setEnabled(!preset->isBuiltIn);
    if (m_deletePresetBtn) m_deletePresetBtn->setEnabled(!preset->isBuiltIn);

    // Select target format in list
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto* item = m_listWidget->item(i);
        if (item && item->data(Qt::UserRole).toString() == preset->targetFormatId) {
            m_listWidget->setCurrentItem(item);
            break;
        }
    }
}

void FormatSelectorDialog::populateFormats() {
    m_listWidget->clear();
    m_availableFormats = core::FormatRegistry::instance().availableOutputsForInput(m_sourceFormatId);

    const QString filter = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : QString();
    auto& resolver = core::EngineCapabilityResolver::instance();

    for (const auto& fmt : m_availableFormats) {
        if (!filter.isEmpty() && !fmt.id.contains(filter) && !fmt.name.toLower().contains(filter)) {
            continue;
        }

        const auto cap = resolver.resolveFormat(fmt.id);
        QString statusTag;
        switch (cap.readiness) {
            case core::FormatReadiness::Verified:
                statusTag = "✓ Verified";
                break;
            case core::FormatReadiness::Available:
                statusTag = "● Available";
                break;
            case core::FormatReadiness::Recognized:
            default:
                statusTag = "○ Recognized";
                break;
        }

        const QString label = QString("%1 (.%2)  [%3]")
                                  .arg(fmt.name)
                                  .arg(fmt.defaultExtension)
                                  .arg(statusTag);

        auto* item = new QListWidgetItem(label, m_listWidget);
        item->setData(Qt::UserRole, fmt.id);

        if (cap.readiness == core::FormatReadiness::Recognized && !cap.isWritableOnHost) {
            item->setForeground(QColor("#64748b"));
        } else if (cap.readiness == core::FormatReadiness::Verified) {
            item->setForeground(QColor("#34d399"));
        }
    }

    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
        onItemSelectionChanged();
    }
}

void FormatSelectorDialog::onItemSelectionChanged() {
    auto* item = m_listWidget->currentItem();
    if (!item) return;

    const QString formatId = item->data(Qt::UserRole).toString();
    updateDetailPanel(formatId);

    // If current format does not match the active preset's targetFormatId, clear stale preset options
    if (!m_selectedPresetId.isEmpty()) {
        const auto* preset = core::PresetRegistry::instance().findById(m_selectedPresetId);
        if (!preset || preset->targetFormatId != formatId) {
            m_selectedPresetId.clear();
            m_selectedPresetOptions.clear();
            if (m_presetCombo) {
                m_presetCombo->blockSignals(true);
                m_presetCombo->setCurrentIndex(0);
                m_presetCombo->blockSignals(false);
            }
            if (m_presetDescLabel) {
                m_presetDescLabel->clear();
            }
            if (m_editPresetBtn) m_editPresetBtn->setEnabled(false);
            if (m_deletePresetBtn) m_deletePresetBtn->setEnabled(false);
        }
    }
}

void FormatSelectorDialog::updateDetailPanel(const QString& formatId) {
    if (!m_detailLabel) return;

    auto& resolver = core::EngineCapabilityResolver::instance();
    const auto cap = resolver.resolveFormat(formatId);

    QString text = QString("<b>%1</b> (Extension: .%2)<br>")
                       .arg(cap.definition.name)
                       .arg(cap.definition.defaultExtension);

    text += QString("Responsible Engine: <b>%1</b><br>").arg(cap.activeEngineName.isEmpty() ? "Unknown" : cap.activeEngineName);
    text += QString("Capability Status: %1<br>").arg(
        cap.readiness == core::FormatReadiness::Verified ? "<span style='color:#34d399;'>Verified (Full test coverage)</span>" :
        (cap.readiness == core::FormatReadiness::Available ? "<span style='color:#38bdf8;'>Available on Host</span>" :
         "<span style='color:#f87171;'>Recognized (Engine/Delegates missing)</span>"));

    if (!cap.engineStatusDetail.isEmpty()) {
        text += QString("Details: %1").arg(cap.engineStatusDetail);
    }

    m_detailLabel->setText(text);
}

void FormatSelectorDialog::onFilterTextChanged(const QString& text) {
    const QString filter = text.trimmed().toLower();
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto* item = m_listWidget->item(i);
        if (!item) continue;
        const QString fmtId = item->data(Qt::UserRole).toString().toLower();
        const QString textLabel = item->text().toLower();
        const bool match = filter.isEmpty() || fmtId.contains(filter) || textLabel.contains(filter);
        item->setHidden(!match);
    }
}

void FormatSelectorDialog::onItemDoubleClicked(QListWidgetItem* item) {
    if (item) {
        m_selectedFormatId = item->data(Qt::UserRole).toString();
        if (m_applyToAllCheck) {
            m_applyToAllRemaining = m_applyToAllCheck->isChecked();
        }
        accept();
    }
}

} // namespace uc::ui
