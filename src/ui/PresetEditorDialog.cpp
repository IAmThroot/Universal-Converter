#include "PresetEditorDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QUuid>

namespace uc::ui {

PresetEditorDialog::PresetEditorDialog(core::MediaCategory sourceCategory,
                                       const core::ConversionPreset* existingPreset,
                                       QWidget* parent)
    : QDialog(parent)
    , m_sourceCategory(sourceCategory)
    , m_isEditMode(existingPreset != nullptr)
{
    setWindowTitle(m_isEditMode ? "Edit Conversion Preset" : "Custom Preset Builder");
    setMinimumWidth(500);
    setStyleSheet("background-color: #0f172a; color: #f8fafc;");

    setupUi();

    if (existingPreset) {
        loadFromPreset(*existingPreset);
    } else {
        m_presetId = QString("custom_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    }
}

void PresetEditorDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);

    // --- Basic Metadata ---
    auto* metaGroup = new QGroupBox("Preset Information", this);
    auto* metaForm = new QFormLayout(metaGroup);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("e.g. My Custom 4K ProRes Profile");
    metaForm->addRow("Preset Name:", m_nameEdit);

    m_descEdit = new QLineEdit(this);
    m_descEdit->setPlaceholderText("Brief description of profile settings and use-case");
    metaForm->addRow("Description:", m_descEdit);

    m_sourceCatCombo = new QComboBox(this);
    m_sourceCatCombo->addItem("Video", static_cast<int>(core::MediaCategory::Video));
    m_sourceCatCombo->addItem("Audio", static_cast<int>(core::MediaCategory::Audio));
    m_sourceCatCombo->addItem("Image", static_cast<int>(core::MediaCategory::Image));
    m_sourceCatCombo->addItem("Document", static_cast<int>(core::MediaCategory::Document));
    m_sourceCatCombo->addItem("Subtitle", static_cast<int>(core::MediaCategory::Subtitle));

    const int idx = m_sourceCatCombo->findData(static_cast<int>(m_sourceCategory));
    if (idx >= 0) m_sourceCatCombo->setCurrentIndex(idx);
    metaForm->addRow("Source Category:", m_sourceCatCombo);

    m_targetFormatEdit = new QLineEdit(this);
    m_targetFormatEdit->setPlaceholderText("e.g. mp4, mkv, webp, mp3, pdf");
    metaForm->addRow("Target Format Extension:", m_targetFormatEdit);

    mainLayout->addWidget(metaGroup);

    // --- Video/Audio Advanced Flags ---
    m_videoOptionsWidget = new QGroupBox("Video & Audio Transcode Flags", this);
    auto* videoForm = new QFormLayout(m_videoOptionsWidget);

    m_videoCodecEdit = new QLineEdit(this);
    m_videoCodecEdit->setPlaceholderText("e.g. libx264, libx265, libvpx-vp9, hevc_nvenc");
    videoForm->addRow("Video Codec (-c:v):", m_videoCodecEdit);

    m_crfSpin = new QSpinBox(this);
    m_crfSpin->setRange(0, 51);
    m_crfSpin->setValue(23);
    videoForm->addRow("CRF Rate Control (0-51):", m_crfSpin);

    m_speedCombo = new QComboBox(this);
    m_speedCombo->addItem("Ultrafast", 0);
    m_speedCombo->addItem("Fast", 1);
    m_speedCombo->addItem("Medium", 2);
    m_speedCombo->addItem("Slow", 3);
    m_speedCombo->addItem("Slower", 4);
    m_speedCombo->addItem("Very Slow", 5);
    m_speedCombo->setCurrentIndex(2);
    videoForm->addRow("Encoding Speed / Preset:", m_speedCombo);

    m_videoBitrateSpin = new QSpinBox(this);
    m_videoBitrateSpin->setRange(0, 100000);
    m_videoBitrateSpin->setSuffix(" kbps (0 = auto/crf)");
    m_videoBitrateSpin->setValue(0);
    videoForm->addRow("Video Bitrate:", m_videoBitrateSpin);

    m_audioCodecEdit = new QLineEdit(this);
    m_audioCodecEdit->setPlaceholderText("e.g. aac, libmp3lame, libopus, flac");
    videoForm->addRow("Audio Codec (-c:a):", m_audioCodecEdit);

    m_audioBitrateSpin = new QSpinBox(this);
    m_audioBitrateSpin->setRange(0, 1000);
    m_audioBitrateSpin->setSuffix(" kbps");
    m_audioBitrateSpin->setValue(192);
    videoForm->addRow("Audio Bitrate:", m_audioBitrateSpin);

    mainLayout->addWidget(m_videoOptionsWidget);

    // --- Image Options ---
    m_imageOptionsWidget = new QGroupBox("Image Optimization Flags", this);
    auto* imgForm = new QFormLayout(m_imageOptionsWidget);

    m_qualitySpin = new QSpinBox(this);
    m_qualitySpin->setRange(1, 100);
    m_qualitySpin->setValue(85);
    imgForm->addRow("Quality (1-100):", m_qualitySpin);

    m_stripMetadataCheck = new QCheckBox("Strip EXIF/Color Profile Metadata", this);
    m_stripMetadataCheck->setChecked(false);
    imgForm->addRow("", m_stripMetadataCheck);

    m_autoOrientCheck = new QCheckBox("Auto-Orient based on EXIF tag", this);
    m_autoOrientCheck->setChecked(true);
    imgForm->addRow("", m_autoOrientCheck);

    m_preserveAspectRatioCheck = new QCheckBox("Preserve Aspect Ratio", this);
    m_preserveAspectRatioCheck->setChecked(true);
    imgForm->addRow("", m_preserveAspectRatioCheck);

    mainLayout->addWidget(m_imageOptionsWidget);

    // Profile Import / Export Actions
    auto* ioRow = new QHBoxLayout();
    auto* expBtn = new QPushButton("Export Profiles to JSON...", this);
    auto* impBtn = new QPushButton("Import Profiles from JSON...", this);
    ioRow->addWidget(expBtn);
    ioRow->addWidget(impBtn);
    ioRow->addStretch();
    mainLayout->addLayout(ioRow);

    connect(expBtn, &QPushButton::clicked, this, &PresetEditorDialog::onExportClicked);
    connect(impBtn, &QPushButton::clicked, this, &PresetEditorDialog::onImportClicked);

    // --- Dialog Buttons ---
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &PresetEditorDialog::onSaveClicked);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);

    connect(m_sourceCatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PresetEditorDialog::onCategoryChanged);

    onCategoryChanged(m_sourceCatCombo->currentIndex());
}

void PresetEditorDialog::onCategoryChanged(int idx) {
    const auto cat = static_cast<core::MediaCategory>(m_sourceCatCombo->itemData(idx).toInt());
    m_videoOptionsWidget->setVisible(cat == core::MediaCategory::Video || cat == core::MediaCategory::Audio);
    m_imageOptionsWidget->setVisible(cat == core::MediaCategory::Image);
}

void PresetEditorDialog::loadFromPreset(const core::ConversionPreset& p) {
    m_presetId = p.id;
    m_nameEdit->setText(p.name);
    m_descEdit->setText(p.description);
    m_targetFormatEdit->setText(p.targetFormatId);
    m_existingOptions = p.options;

    const int idx = m_sourceCatCombo->findData(static_cast<int>(p.sourceCategory));
    if (idx >= 0) m_sourceCatCombo->setCurrentIndex(idx);

    // Extract options
    if (p.options.contains("customVideoCodec")) m_videoCodecEdit->setText(p.options["customVideoCodec"].toString());
    if (p.options.contains("crf")) m_crfSpin->setValue(p.options["crf"].toInt());
    if (p.options.contains("preset")) m_speedCombo->setCurrentIndex(qBound(0, p.options["preset"].toInt(), 5));
    if (p.options.contains("customVideoBitrateKbps")) m_videoBitrateSpin->setValue(p.options["customVideoBitrateKbps"].toInt());
    if (p.options.contains("customAudioCodec")) m_audioCodecEdit->setText(p.options["customAudioCodec"].toString());
    if (p.options.contains("customAudioBitrateKbps")) m_audioBitrateSpin->setValue(p.options["customAudioBitrateKbps"].toInt());

    if (p.options.contains("quality")) m_qualitySpin->setValue(p.options["quality"].toInt());
    if (p.options.contains("stripMetadata")) m_stripMetadataCheck->setChecked(p.options["stripMetadata"].toBool());
    if (p.options.contains("autoOrient")) m_autoOrientCheck->setChecked(p.options["autoOrient"].toBool());
    if (p.options.contains("preserveAspectRatio")) m_preserveAspectRatioCheck->setChecked(p.options["preserveAspectRatio"].toBool());
}

core::ConversionPreset PresetEditorDialog::preset() const {
    core::ConversionPreset p;
    p.id = m_presetId;
    p.name = m_nameEdit->text().trimmed();
    p.description = m_descEdit->text().trimmed();
    p.category = core::PresetCategory::Custom;
    p.sourceCategory = static_cast<core::MediaCategory>(m_sourceCatCombo->currentData().toInt());
    p.targetFormatId = m_targetFormatEdit->text().trimmed().toLower();
    p.isBuiltIn = false;

    QVariantMap opts = m_existingOptions;
    if (p.sourceCategory == core::MediaCategory::Video || p.sourceCategory == core::MediaCategory::Audio) {
        if (!m_videoCodecEdit->text().trimmed().isEmpty()) opts["customVideoCodec"] = m_videoCodecEdit->text().trimmed();
        opts["crf"] = m_crfSpin->value();
        opts["preset"] = m_speedCombo->currentIndex();
        if (m_videoBitrateSpin->value() > 0) opts["customVideoBitrateKbps"] = m_videoBitrateSpin->value();
        if (!m_audioCodecEdit->text().trimmed().isEmpty()) opts["customAudioCodec"] = m_audioCodecEdit->text().trimmed();
        opts["customAudioBitrateKbps"] = m_audioBitrateSpin->value();
    } else if (p.sourceCategory == core::MediaCategory::Image) {
        opts["quality"] = m_qualitySpin->value();
        opts["stripMetadata"] = m_stripMetadataCheck->isChecked();
        opts["autoOrient"] = m_autoOrientCheck->isChecked();
        opts["preserveAspectRatio"] = m_preserveAspectRatioCheck->isChecked();
    }
    p.options = opts;
    return p;
}

void PresetEditorDialog::onExportClicked() {
    const QString filePath = QFileDialog::getSaveFileName(this, "Export Preset Profiles to JSON", "universal_converter_presets.json", "JSON Files (*.json)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(core::PresetRegistry::instance().exportProfilesToJson().toUtf8());
        file.close();
        QMessageBox::information(this, "Export Succeeded", "Preset profiles successfully exported.");
    }
}

void PresetEditorDialog::onImportClicked() {
    const QString filePath = QFileDialog::getOpenFileName(this, "Import Preset Profiles from JSON", QString(), "JSON Files (*.json)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QString err;
        const bool success = core::PresetRegistry::instance().importProfilesFromJson(QString::fromUtf8(file.readAll()), &err);
        file.close();
        if (success) {
            QMessageBox::information(this, "Import Succeeded", "Presets successfully imported.");
        } else {
            QMessageBox::warning(this, "Import Failed", QString("Could not import presets:\n%1").arg(err));
        }
    }
}

void PresetEditorDialog::onSaveClicked() {
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Preset name cannot be empty.");
        return;
    }
    if (m_targetFormatEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Target format extension cannot be empty.");
        return;
    }

    core::PresetRegistry::instance().registerCustomPreset(preset());
    accept();
}

} // namespace uc::ui
