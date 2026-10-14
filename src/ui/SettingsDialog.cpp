#include "SettingsDialog.h"
#include "../app/AppSettings.h"
#include "../app/ToolchainManager.h"
#include "../core/WatchFolderConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>

namespace uc::ui {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Universal Converter Settings");
    setMinimumWidth(560);
    setStyleSheet("background-color: #0f172a; color: #f8fafc;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);

    // --- General Settings ---
    auto* genGroup = new QGroupBox("General Options", this);
    auto* genForm = new QFormLayout(genGroup);

    m_concurrencySpin = new QSpinBox(this);
    m_concurrencySpin->setRange(1, 64);
    genForm->addRow("Max Concurrent Jobs:", m_concurrencySpin);

    m_hwAccelCheck = new QCheckBox("Enable Hardware Acceleration (NVENC / VA-API / QSV)", this);
    genForm->addRow("", m_hwAccelCheck);

    mainLayout->addWidget(genGroup);

    // --- Destination Settings ---
    auto* destGroup = new QGroupBox("Output Destination", this);
    auto* destForm = new QFormLayout(destGroup);

    m_destModeCombo = new QComboBox(this);
    m_destModeCombo->addItem("Create 'Converted/' subfolder next to source file", 1);
    m_destModeCombo->addItem("Save directly beside source file", 0);
    m_destModeCombo->addItem("Save to custom output directory", 2);
    destForm->addRow("Destination Mode:", m_destModeCombo);

    auto* customDestLayout = new QHBoxLayout();
    m_customDestEdit = new QLineEdit(this);
    m_browseCustomDestBtn = new QPushButton("Browse...", this);
    customDestLayout->addWidget(m_customDestEdit);
    customDestLayout->addWidget(m_browseCustomDestBtn);
    destForm->addRow("Custom Directory:", customDestLayout);

    mainLayout->addWidget(destGroup);

    // --- Watch Folders Automation ---
    auto* watchGroup = new QGroupBox("Watch Folders Daemon (Automated Ingestion)", this);
    auto* watchLayout = new QVBoxLayout(watchGroup);

    m_watchTable = new QTableWidget(this);
    m_watchTable->setColumnCount(4);
    m_watchTable->setHorizontalHeaderLabels({"Watched Directory", "Target Format", "Status", "Post Action"});
    m_watchTable->horizontalHeader()->setStretchLastSection(true);
    m_watchTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_watchTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_watchTable->setMinimumHeight(130);
    watchLayout->addWidget(m_watchTable);

    auto* watchBtnRow = new QHBoxLayout();
    m_addWatchBtn = new QPushButton("Add Folder...", this);
    m_removeWatchBtn = new QPushButton("Remove", this);
    m_toggleWatchBtn = new QPushButton("Toggle Enable", this);
    watchBtnRow->addWidget(m_addWatchBtn);
    watchBtnRow->addWidget(m_removeWatchBtn);
    watchBtnRow->addWidget(m_toggleWatchBtn);
    watchBtnRow->addStretch();
    watchLayout->addLayout(watchBtnRow);

    connect(m_addWatchBtn, &QPushButton::clicked, this, &SettingsDialog::onAddWatchFolder);
    connect(m_removeWatchBtn, &QPushButton::clicked, this, &SettingsDialog::onRemoveWatchFolder);
    connect(m_toggleWatchBtn, &QPushButton::clicked, this, &SettingsDialog::onToggleWatchFolder);

    mainLayout->addWidget(watchGroup);

    // --- Toolchain Executable Overrides ---
    auto* toolsGroup = new QGroupBox("Conversion Engine Overrides (Optional)", this);
    auto* toolsForm = new QFormLayout(toolsGroup);

    auto makeBrowseRow = [this](QLineEdit*& edit, const char* browseSlot) {
        auto* row = new QHBoxLayout();
        edit = new QLineEdit(this);
        edit->setPlaceholderText("Auto-detected via PATH or tools/ folder");
        auto* btn = new QPushButton("Browse...", this);
        connect(btn, SIGNAL(clicked()), this, browseSlot);
        row->addWidget(edit);
        row->addWidget(btn);
        return row;
    };

    toolsForm->addRow("FFmpeg Executable:", makeBrowseRow(m_ffmpegEdit, SLOT(onBrowseFfmpeg())));
    toolsForm->addRow("ImageMagick Executable:", makeBrowseRow(m_magickEdit, SLOT(onBrowseMagick())));
    toolsForm->addRow("Pandoc Executable:", makeBrowseRow(m_pandocEdit, SLOT(onBrowsePandoc())));

    mainLayout->addWidget(toolsGroup);

    // --- Dialog Buttons ---
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onSaveClicked);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);

    connect(m_browseCustomDestBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseCustomDest);
    connect(m_destModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
        m_customDestEdit->setEnabled(idx == 2);
        m_browseCustomDestBtn->setEnabled(idx == 2);
    });

    loadSettings();
}

void SettingsDialog::refreshWatchFoldersTable() {
    m_watchTable->setRowCount(0);
    const QString jsonStr = app::AppSettings::instance().watchFoldersJson();
    const auto doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray()) return;

    const auto arr = doc.array();
    for (int i = 0; i < arr.size(); ++i) {
        const auto cfg = core::WatchFolderConfig::fromJson(arr[i].toObject());
        m_watchTable->insertRow(i);
        m_watchTable->setItem(i, 0, new QTableWidgetItem(cfg.folderPath));
        m_watchTable->setItem(i, 1, new QTableWidgetItem(cfg.targetFormatId.toUpper()));
        m_watchTable->setItem(i, 2, new QTableWidgetItem(cfg.isEnabled ? "Active" : "Paused"));
        m_watchTable->setItem(i, 3, new QTableWidgetItem(
            cfg.postAction == core::WatchPostAction::MoveToProcessed ? "Move to processed/" :
            cfg.postAction == core::WatchPostAction::DeleteOriginal ? "Delete original" : "Keep original"));
    }
}

void SettingsDialog::onAddWatchFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Directory to Automatically Watch");
    if (dir.isEmpty()) return;

    bool ok = false;
    const QString format = QInputDialog::getText(this, "Target Format",
                                                "Enter target format extension (e.g. mp4, mp3, webp, png, pdf):",
                                                QLineEdit::Normal, "mp4", &ok);
    if (!ok || format.trimmed().isEmpty()) return;

    core::WatchFolderConfig cfg;
    cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    cfg.folderPath = dir;
    cfg.targetFormatId = format.trimmed().toLower();
    cfg.isEnabled = true;
    cfg.postAction = core::WatchPostAction::MoveToProcessed;

    const QString jsonStr = app::AppSettings::instance().watchFoldersJson();
    auto doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    auto arr = doc.isArray() ? doc.array() : QJsonArray();
    arr.append(cfg.toJson());

    app::AppSettings::instance().setWatchFoldersJson(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    refreshWatchFoldersTable();
}

void SettingsDialog::onRemoveWatchFolder() {
    const int row = m_watchTable->currentRow();
    if (row < 0) return;

    const QString jsonStr = app::AppSettings::instance().watchFoldersJson();
    auto doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray()) return;

    auto arr = doc.array();
    if (row < arr.size()) {
        arr.removeAt(row);
        app::AppSettings::instance().setWatchFoldersJson(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
        refreshWatchFoldersTable();
    }
}

void SettingsDialog::onToggleWatchFolder() {
    const int row = m_watchTable->currentRow();
    if (row < 0) return;

    const QString jsonStr = app::AppSettings::instance().watchFoldersJson();
    auto doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray()) return;

    auto arr = doc.array();
    if (row < arr.size()) {
        auto obj = arr[row].toObject();
        auto cfg = core::WatchFolderConfig::fromJson(obj);
        cfg.isEnabled = !cfg.isEnabled;
        arr[row] = cfg.toJson();
        app::AppSettings::instance().setWatchFoldersJson(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
        refreshWatchFoldersTable();
    }
}

void SettingsDialog::loadSettings() {
    auto& s = app::AppSettings::instance();
    m_concurrencySpin->setValue(s.maxConcurrentJobs());
    m_hwAccelCheck->setChecked(s.hardwareAccelerationEnabled());

    const int modeIdx = m_destModeCombo->findData(static_cast<int>(s.destinationMode()));
    m_destModeCombo->setCurrentIndex(modeIdx >= 0 ? modeIdx : 0);
    m_customDestEdit->setText(s.customDestinationPath());
    m_customDestEdit->setEnabled(s.destinationMode() == app::DestinationMode::CustomFolder);
    m_browseCustomDestBtn->setEnabled(s.destinationMode() == app::DestinationMode::CustomFolder);

    m_ffmpegEdit->setText(s.ffmpegPathOverride());
    m_magickEdit->setText(s.magickPathOverride());
    m_pandocEdit->setText(s.pandocPathOverride());

    refreshWatchFoldersTable();
}

void SettingsDialog::onSaveClicked() {
    auto& s = app::AppSettings::instance();
    s.setMaxConcurrentJobs(m_concurrencySpin->value());
    s.setHardwareAccelerationEnabled(m_hwAccelCheck->isChecked());
    s.setDestinationMode(static_cast<app::DestinationMode>(m_destModeCombo->currentData().toInt()));
    s.setCustomDestinationPath(m_customDestEdit->text().trimmed());

    s.setFfmpegPathOverride(m_ffmpegEdit->text().trimmed());
    s.setMagickPathOverride(m_magickEdit->text().trimmed());
    s.setPandocPathOverride(m_pandocEdit->text().trimmed());

    s.sync();
    app::ToolchainManager::instance().scanAll();
    accept();
}

void SettingsDialog::onBrowseCustomDest() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Custom Output Directory", m_customDestEdit->text());
    if (!dir.isEmpty()) {
        m_customDestEdit->setText(dir);
    }
}

void SettingsDialog::onBrowseFfmpeg() {
    const QString f = QFileDialog::getOpenFileName(this, "Select FFmpeg Binary");
    if (!f.isEmpty()) m_ffmpegEdit->setText(f);
}

void SettingsDialog::onBrowseMagick() {
    const QString f = QFileDialog::getOpenFileName(this, "Select ImageMagick (magick) Binary");
    if (!f.isEmpty()) m_magickEdit->setText(f);
}

void SettingsDialog::onBrowsePandoc() {
    const QString f = QFileDialog::getOpenFileName(this, "Select Pandoc Binary");
    if (!f.isEmpty()) m_pandocEdit->setText(f);
}

} // namespace uc::ui
