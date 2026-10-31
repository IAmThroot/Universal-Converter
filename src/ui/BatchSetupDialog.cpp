#include "BatchSetupDialog.h"
#include "FormatSelectorDialog.h"
#include "ImagePreviewDialog.h"
#include "../core/EngineCapabilityResolver.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileInfo>
#include <QFileDialog>
#include <QLabel>
#include <QDir>
#include <QImageReader>
#include "../core/MediaEstimator.h"
#include "../core/OutputPathPlanner.h"
namespace uc::ui {
BatchSetupDialog::BatchSetupDialog(const QStringList &paths, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Review conversion batch");
    resize(1000, 620);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        "Select files, choose a format or preset, then review the destinations before converting.", this));
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({"Source", "Target", "Planned output", "Readiness", "Estimated size"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (const auto &path : paths) {
        m_entries.append({path, {}, {}});
        int row = m_table->rowCount();
        m_table->insertRow(row);
        for (int col = 0; col < 5; ++col)
            m_table->setItem(row, col, new QTableWidgetItem);
        m_table->item(row, 0)->setText(QFileInfo(path).fileName());
        m_table->item(row, 0)->setToolTip(path);
    }
    layout->addWidget(m_table, 1);
    auto *actions = new QHBoxLayout;
    auto *format = new QPushButton("Choose Format / Preset", this);
    auto *remove = new QPushButton("Remove Selected", this);
    auto *preview = new QPushButton("Image Preview", this);
    actions->addWidget(format);
    actions->addWidget(preview);
    actions->addWidget(remove);
    actions->addStretch();
    layout->addLayout(actions);
    auto selected = [this]() {
        QList<int> rows;
        for (const auto &idx : m_table->selectionModel()->selectedRows())
            rows << idx.row();
        return rows;
    };
    connect(format, &QPushButton::clicked, this, [this, selected]() {
        auto rows = selected();
        if (rows.isEmpty()) {
            for (int i = 0; i < m_entries.size(); ++i)
                rows << i;
        }
        if (rows.isEmpty())
            return;
        const auto *source = core::FormatRegistry::instance().findByFilePath(m_entries[rows.first()].input);
        if (!source)
            return;
        FormatSelectorDialog dialog(source->id, this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        for (int row : rows) {
            m_entries[row].output = dialog.selectedFormatId();
            m_entries[row].options = dialog.selectedPresetOptions();
        }
        refresh();
    });
    connect(remove, &QPushButton::clicked, this, [this, selected]() {
        auto rows = selected();
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int row : rows) {
            m_entries.removeAt(row);
            m_table->removeRow(row);
        }
        refresh();
    });
    connect(preview, &QPushButton::clicked, this, [this, selected]() {
        const auto rows = selected();
        if (rows.size() != 1)
            return;
        auto &entry = m_entries[rows.first()];
        const auto *fmt = core::FormatRegistry::instance().findByFilePath(entry.input);
        if (!fmt || fmt->category != core::MediaCategory::Image || entry.output.isEmpty())
            return;
        ImagePreviewDialog dialog(entry.input, entry.output, this);
        if (dialog.exec() == QDialog::Accepted)
            entry.options["quality"] = dialog.quality();
    });
    auto *dest = new QHBoxLayout;
    m_mode = new QComboBox(this);
    m_mode->addItem("Beside source", int(app::DestinationMode::SameAsSource));
    m_mode->addItem("Converted subfolder", int(app::DestinationMode::SourceConvertedSubfolder));
    m_mode->addItem("Custom folder", int(app::DestinationMode::CustomFolder));
    m_destination = new QLineEdit(app::AppSettings::instance().customDestinationPath(), this);
    m_destination->setPlaceholderText("Output folder");
    m_destination->setAccessibleName("Output folder");
    m_mode->setCurrentIndex(m_mode->findData(int(app::AppSettings::instance().destinationMode())));
    auto *browse = new QPushButton("Browse…", this);
    browse->setObjectName("browseDestination");
    dest->addWidget(m_mode);
    dest->addWidget(m_destination, 1);
    dest->addWidget(browse);
    layout->addLayout(dest);
    connect(browse, &QPushButton::clicked, this, [this]() {
        auto dir = QFileDialog::getExistingDirectory(this, "Output folder", destination());
        if (!dir.isEmpty())
            m_destination->setText(dir);
    });
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this]() { refresh(); });
    connect(m_destination, &QLineEdit::textChanged, this, [this]() { refresh(); });
    auto *footer = new QHBoxLayout;
    auto *cancel = new QPushButton("Cancel", this);
    m_convert = new QPushButton(this);
    m_convert->setObjectName("primaryButton");
    footer->addStretch();
    footer->addWidget(cancel);
    footer->addWidget(m_convert);
    layout->addLayout(footer);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_convert, &QPushButton::clicked, this, &QDialog::accept);
    connect(&core::EngineCapabilityResolver::instance(), &core::EngineCapabilityResolver::capabilitiesChanged,
            this, [this]() { refresh(); });
    if (m_table->rowCount())
        m_table->selectAll();
    refresh();
}
QList<BatchEntry> BatchSetupDialog::entries() const { return m_entries; }
app::DestinationMode BatchSetupDialog::destinationMode() const {
    return app::DestinationMode(m_mode->currentData().toInt());
}
QString BatchSetupDialog::destination() const { return m_destination->text().trimmed(); }
void BatchSetupDialog::refresh() {
    bool ready = !m_entries.isEmpty();
    const bool custom = destinationMode() == app::DestinationMode::CustomFolder;
    m_destination->setEnabled(custom);
    if(auto* browse=findChild<QPushButton*>("browseDestination"))browse->setEnabled(custom);
    if (custom && destination().isEmpty())
        ready = false;
    for (int row = 0; row < m_entries.size(); ++row) {
        const auto &entry = m_entries[row];
        const auto *fmt = core::FormatRegistry::instance().findByFilePath(entry.input);
        const auto route = core::EngineCapabilityResolver::instance().resolveRoute(
            fmt ? fmt->id : QString(), entry.output, entry.options);
        QString reason = !fmt                     ? "Unrecognized source format"
                         : entry.output.isEmpty() ? "Choose an output format"
                                                  : route.reason;
        bool valid = route.available && QFileInfo(entry.input).isFile();
        if (!QFileInfo(entry.input).isFile())
            reason = "Source missing";
        ready &= valid;
        m_table->item(row, 1)->setText(entry.output.toUpper());
        QString dir = custom ? destination() : QFileInfo(entry.input).absolutePath();
        if (destinationMode() == app::DestinationMode::SourceConvertedSubfolder)
            dir = QDir(dir).filePath("Converted");
        const auto *target = core::FormatRegistry::instance().findById(entry.output);
        QString output = entry.output.isEmpty()
                             ? QString()
                             : QDir(dir).filePath(QFileInfo(entry.input).completeBaseName() + "." +
                                                  (target ? target->defaultExtension : entry.output));
        m_table->item(row, 2)->setText(output);
        m_table->item(row, 2)->setToolTip(output + "\nA unique suffix will be added on collision.");
        core::MediaEstimationInput estimation;
        estimation.sourceSizeBytes = QFileInfo(entry.input).size();
        estimation.targetFormatId = entry.output;
        if (fmt && fmt->category == core::MediaCategory::Image) {
            QImageReader reader(entry.input);
            const auto size = reader.size();
            estimation.sourceWidth = size.width();
            estimation.sourceHeight = size.height();
        }
        estimation.targetQuality = entry.options.value("quality", 85).toInt();
        const auto estimated = core::MediaEstimator::estimateMediaOutput(estimation);
        m_table->item(row, 4)->setText(estimated.hasEstimate ? "~" + estimated.formattedEstimatedSize
                                                             : "Unavailable before probing");
        m_table->item(row, 4)->setToolTip(estimated.explanation);
        m_table->item(row, 3)->setText(valid ? "Ready" : reason);
        m_table->item(row, 3)->setToolTip(reason);
    }
    m_convert->setText(
        QString("Convert %1 %2").arg(m_entries.size()).arg(m_entries.size() == 1 ? "File" : "Files"));
    m_convert->setEnabled(ready);
}
} // namespace uc::ui
