#include "WatchFoldersDialog.h"
#include "../app/AppSettings.h"
#include "../core/FormatRegistry.h"
#include "../core/PresetRegistry.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMessageBox>
#include <QLabel>
#include <QTimer>
#include <QUuid>
#include <QDir>
#include <QFileDialog>
namespace uc::ui {
WatchFoldersDialog::WatchFoldersDialog(core::WatchFolderService *service, QWidget *parent)
    : QDialog(parent), m_service(service) {
    setWindowTitle("Watch Folders");
    resize(1050, 540);
    auto *layout = new QVBoxLayout(this);
    auto *help = new QLabel("Pause clears pending files and cancels queued jobs. Running jobs finish with "
                            "their original settings. Watching runs while this application is open.",
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);
    m_table = new QTableWidget(0, 7, this);
    m_table->setHorizontalHeaderLabels(
        {"Folder", "Target", "Destination", "Original", "State", "Pending", "Last activity"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_table, 1);
    auto *actions = new QHBoxLayout;
    layout->addLayout(actions);
    auto button = [&](const QString &text, auto fn) {
        auto *b = new QPushButton(text, this);
        actions->addWidget(b);
        connect(b, &QPushButton::clicked, this, fn);
    };
    button("Add Folder", [this]() { editFolder(true); });
    button("Edit", [this]() { editFolder(false); });
    button("Remove", [this]() {
        int row = m_table->currentRow();
        if (row < 0)
            return;
        m_service->removeFolder(m_table->item(row, 0)->data(Qt::UserRole).toString());
        save();
        refresh();
    });
    button("Pause / Resume", [this]() {
        int row = m_table->currentRow();
        if (row < 0)
            return;
        auto id = m_table->item(row, 0)->data(Qt::UserRole).toString();
        m_service->setFolderEnabled(id, !m_service->folderConfig(id).isEnabled);
        save();
        refresh();
    });
    button("Scan Now", [this]() {
        m_service->scanAllNow();
        refresh();
    });
    button("Retry Failures", [this]() {
        m_service->retryFailures();
        refresh();
    });
    button("Close", [this]() { accept(); });
    connect(service, &core::WatchFolderService::watchStatusChanged, this, &WatchFoldersDialog::refresh);
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &WatchFoldersDialog::refresh);
    timer->start(1000);
    refresh();
}
void WatchFoldersDialog::refresh() {
    QString selection;
    if (m_table->currentRow() >= 0)
        selection = m_table->item(m_table->currentRow(), 0)->data(Qt::UserRole).toString();
    const auto folders = m_service->configuredFolders();
    m_table->setRowCount(folders.size());
    for (int row = 0; row < folders.size(); ++row) {
        const auto &cfg = folders[row];
        QStringList values{
            cfg.folderPath,
            cfg.targetFormatId,
            cfg.destinationFolder.isEmpty() ? cfg.folderPath + "/converted" : cfg.destinationFolder,
            cfg.postAction == core::WatchPostAction::KeepOriginal      ? "Keep"
            : cfg.postAction == core::WatchPostAction::MoveToProcessed ? "Move to processed"
                                                                       : "Delete after success",
            cfg.isEnabled ? "Enabled" : "Paused",
            QString::number(m_service->pendingCount(cfg.id)),
            m_service->activity(cfg.id)};
        for (int col = 0; col < values.size(); ++col) {
            auto *item = new QTableWidgetItem(values[col]);
            item->setToolTip(values[col]);
            m_table->setItem(row, col, item);
        }
        m_table->item(row, 0)->setData(Qt::UserRole, cfg.id);
        if (cfg.id == selection)
            m_table->selectRow(row);
    }
}
void WatchFoldersDialog::save() {
    QJsonArray array;
    for (const auto &cfg : m_service->configuredFolders())
        array.append(cfg.toJson());
    auto &settings = app::AppSettings::instance();
    settings.setWatchFoldersJson(QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
    settings.sync();
}
void WatchFoldersDialog::editFolder(bool add) {
    core::WatchFolderConfig cfg;
    if (!add) {
        int row = m_table->currentRow();
        if (row < 0)
            return;
        cfg = m_service->folderConfig(m_table->item(row, 0)->data(Qt::UserRole).toString());
    } else
        cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDialog dialog(this);
    dialog.setWindowTitle(add ? "Add Watch Folder" : "Edit Watch Folder");
    dialog.resize(600, 380);
    auto *layout = new QFormLayout(&dialog);
    auto *root = new QLineEdit(cfg.folderPath, &dialog);
    auto *dest = new QLineEdit(cfg.destinationFolder, &dialog);
    dest->setPlaceholderText("Empty: converted subfolder");
    auto *target = new QComboBox(&dialog);
    for (const auto &fmt : core::FormatRegistry::instance().allFormats())
        if (fmt.canOutput)
            target->addItem(fmt.name + " (." + fmt.defaultExtension + ")", fmt.id);
    target->setCurrentIndex(qMax(0, target->findData(cfg.targetFormatId)));
    auto *preset = new QComboBox(&dialog);
    preset->addItem("No preset", QString());
    for (const auto &format : core::FormatRegistry::instance().allFormats()) {
        for (const auto &profile : core::PresetRegistry::instance().presetsForCategory(format.category))
            if (preset->findData(profile.id) < 0)
                preset->addItem(profile.name, profile.id);
    }
    preset->setCurrentIndex(qMax(0, preset->findData(cfg.presetId)));
    auto *post = new QComboBox(&dialog);
    post->addItems(
        {"Keep Original", "Move to Processed (unique names)", "Delete Original after successful conversion"});
    post->setCurrentIndex(int(cfg.postAction));
    auto *recursive = new QCheckBox("Include subfolders", &dialog);
    recursive->setChecked(cfg.isRecursive);
    auto *debounce = new QSpinBox(&dialog);
    debounce->setRange(1, 60);
    debounce->setSuffix(" seconds stable");
    debounce->setValue(cfg.debounceSeconds);
    auto folderRow = [&](QLineEdit *field) {
        auto *row = new QHBoxLayout;
        row->addWidget(field, 1);
        auto *browse = new QPushButton("Browse…", &dialog);
        row->addWidget(browse);
        connect(browse, &QPushButton::clicked, &dialog, [&, field]() {
            const auto path = QFileDialog::getExistingDirectory(&dialog, "Choose folder", field->text());
            if (!path.isEmpty())
                field->setText(path);
        });
        return row;
    };
    layout->addRow("Watch folder", folderRow(root));
    layout->addRow("Output format", target);
    layout->addRow("Destination", folderRow(dest));
    layout->addRow("Preset", preset);
    layout->addRow("Original files", post);
    layout->addRow(recursive);
    layout->addRow("Wait", debounce);
    auto *error = new QLabel(&dialog);
    error->setWordWrap(true);
    layout->addRow(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (!QDir(root->text()).exists()) {
            error->setText("Choose an existing folder.");
            return;
        }
        const QString canonical = QDir(root->text()).canonicalPath();
        for (const auto &other : m_service->configuredFolders()) {
            const QString path = QDir(other.folderPath).canonicalPath();
            if (other.id != cfg.id &&
                (path == canonical || path.startsWith(canonical + '/') || canonical.startsWith(path + '/'))) {
                error->setText("Watch folders must not overlap.");
                return;
            }
        }
        const auto *profile = core::PresetRegistry::instance().findById(preset->currentData().toString());
        if (profile && profile->targetFormatId != target->currentData().toString()) {
            error->setText("The preset target must match the output format.");
            return;
        }
        cfg.presetId = preset->currentData().toString();
        cfg.folderPath = canonical;
        cfg.destinationFolder = dest->text().trimmed();
        cfg.targetFormatId = target->currentData().toString();
        cfg.postAction = core::WatchPostAction(post->currentIndex());
        cfg.isRecursive = recursive->isChecked();
        cfg.debounceSeconds = debounce->value();
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (add)
        m_service->addFolder(cfg);
    else
        m_service->updateFolder(cfg);
    save();
    refresh();
}
} // namespace uc::ui
