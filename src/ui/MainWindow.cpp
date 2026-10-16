#include "MainWindow.h"
#include "DropZoneWidget.h"
#include "SettingsDialog.h"
#include "FormatSelectorDialog.h"
#include "HistoryDialog.h"
#include "ImagePreviewDialog.h"
#include "QueueViewWidget.h"
#include "../app/AppSettings.h"
#include "../app/ToolchainManager.h"
#include "../core/FormatRegistry.h"
#include "../core/ConversionJob.h"
#include "../core/OutputPathPlanner.h"
#include "../engines/FfmpegEngine.h"
#include "../engines/ImageMagickEngine.h"
#include "../engines/PandocEngine.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include "../core/WatchFolderService.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>

namespace uc::ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_orchestrator(std::make_shared<core::ConversionOrchestrator>(app::AppSettings::instance().maxConcurrentJobs()))
{
    m_orchestrator->setFfmpegEngine(std::make_shared<engines::FfmpegEngine>(nullptr));
    m_orchestrator->setImageMagickEngine(std::make_shared<engines::ImageMagickEngine>(nullptr));
    m_orchestrator->setPandocEngine(std::make_shared<engines::PandocEngine>(nullptr));

    m_watchFolderService = std::make_shared<core::WatchFolderService>(m_orchestrator.get(), this);
    syncWatchFolders();

    setWindowTitle("Universal Converter");
    setWindowIcon(QIcon(":/app_icon.png"));
    resize(980, 700);
    setMinimumSize(720, 520);

    setupUi();
    setupMenuBar();
    setupStatusBar();

    connect(&app::ToolchainManager::instance(), &app::ToolchainManager::toolchainStatusChanged,
            this, &MainWindow::updateToolchainStatus);

    auto updateQueueStatus = [this]() {
        const int active = m_orchestrator->activeJobCount();
        const int queued = m_orchestrator->queuedJobs().size();
        const int total = m_orchestrator->totalJobCount();

        if (active > 0 || queued > 0) {
            m_statusLabel->setText(QString("Converting %1 item(s) (%2 queued)").arg(active).arg(queued));
        } else if (total > 0) {
            m_statusLabel->setText(QString("All %1 job(s) completed").arg(total));
        } else {
            m_statusLabel->setText("Ready");
        }
    };

    connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobAdded, this, updateQueueStatus);
    connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobStarted, this, updateQueueStatus);
    connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobFinished, this, [updateQueueStatus](const QString&, core::JobState) {
        updateQueueStatus();
    });
    connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobRemoved, this, [updateQueueStatus](const QString&) {
        updateQueueStatus();
    });
    connect(m_orchestrator.get(), &core::ConversionOrchestrator::queueDrained, this, updateQueueStatus);

    updateToolchainStatus();
}

void MainWindow::setupUi() {
    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");

    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);

    // Header layout
    auto* headerLayout = new QHBoxLayout();

    auto* brandLayout = new QHBoxLayout();
    brandLayout->setSpacing(10);
    auto* iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon(":/app_icon.png").pixmap(28, 28));
    auto* titleLabel = new QLabel("Universal Converter", this);
    titleLabel->setStyleSheet("font-size: 19px; font-weight: 700; color: #f4f4f5; letter-spacing: -0.3px;");
    brandLayout->addWidget(iconLabel);
    brandLayout->addWidget(titleLabel);

    m_historyBtn = new QPushButton("📜 History", this);
    m_historyBtn->setStyleSheet("background-color: #1e293b; color: #f8fafc; border: 1px solid #334155; padding: 6px 14px; border-radius: 6px;");
    connect(m_historyBtn, &QPushButton::clicked, this, &MainWindow::onOpenHistoryClicked);

    m_settingsBtn = new QPushButton("⚙ Settings", this);
    m_settingsBtn->setStyleSheet("background-color: #1e293b; color: #f8fafc; border: 1px solid #334155; padding: 6px 14px; border-radius: 6px;");
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onOpenSettingsClicked);

    headerLayout->addLayout(brandLayout);
    headerLayout->addStretch();
    headerLayout->addWidget(m_historyBtn);
    headerLayout->addWidget(m_settingsBtn);
    mainLayout->addLayout(headerLayout);

    // Drag and Drop Zone (compact top landing)
    m_dropZone = new DropZoneWidget(this);
    m_dropZone->setFixedHeight(140);
    connect(m_dropZone, &DropZoneWidget::filesDropped, this, &MainWindow::onFilesDropped);
    connect(m_dropZone, &DropZoneWidget::clicked, this, &MainWindow::onBrowseFilesClicked);
    mainLayout->addWidget(m_dropZone);

    // Queue View Widget (bottom list)
    m_queueView = new QueueViewWidget(m_orchestrator, this);
    mainLayout->addWidget(m_queueView, 1);

    setCentralWidget(centralWidget);
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu("&File");
    auto* openAction = fileMenu->addAction("&Open Files...", QKeySequence::Open, this, &MainWindow::onBrowseFilesClicked);
    Q_UNUSED(openAction);
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction("E&xit", QKeySequence::Quit, this, &QWidget::close);
    Q_UNUSED(exitAction);

    auto* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction("&Conversion History...", QKeySequence(Qt::CTRL | Qt::Key_H), this, &MainWindow::onOpenHistoryClicked);
    toolsMenu->addAction("&Settings...", QKeySequence::Preferences, this, &MainWindow::onOpenSettingsClicked);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About Universal Converter", [this]() {
        QMessageBox::about(this, "About Universal Converter",
            "<h3>Universal Converter 1.1</h3>"
            "<p><b>Created & Developed by IAmThroot</b></p>"
            "<p>A high-performance, offline desktop converter powered by C++20 and Qt 6.</p>"
            "<p><a href=\"https://github.com/IAmThroot/Universal-Converter#readme\">Documentation & Source Code</a></p>");
    });
}

void MainWindow::setupStatusBar() {
    m_statusLabel = new QLabel("Ready", this);
    m_toolchainStatusLabel = new QLabel(this);

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_toolchainStatusLabel);
}

void MainWindow::updateToolchainStatus() {
    const auto& tc = app::ToolchainManager::instance();
    auto formatTool = [](const char* name, const app::ResolvedTool& tool) -> QString {
        if (tool.isAvailable) {
            return QString("%1: ✓ (%2)").arg(name, tool.sourceDescription());
        } else {
            return QString("%1: ✗ (Missing)").arg(name);
        }
    };

    QStringList statusParts;
    statusParts << formatTool("FFmpeg", tc.ffmpeg());
    statusParts << formatTool("Magick", tc.magick());
    statusParts << formatTool("Pandoc", tc.pandoc());

    m_toolchainStatusLabel->setText(statusParts.join("  |  "));
}

void MainWindow::onFilesDropped(const QStringList& filePaths) {
    if (filePaths.isEmpty()) return;

    QStringList validPaths;
    QStringList unrecognizedPaths;

    for (const QString& path : filePaths) {
        if (core::FormatRegistry::instance().findByFilePath(path)) {
            validPaths << path;
        } else {
            unrecognizedPaths << QFileInfo(path).fileName();
        }
    }

    if (!unrecognizedPaths.isEmpty()) {
        const QString sample = unrecognizedPaths.mid(0, 5).join("\n• ");
        const QString extra = unrecognizedPaths.size() > 5 ? QString("\n... and %1 more").arg(unrecognizedPaths.size() - 5) : "";
        QMessageBox::warning(this, "Unrecognized Format(s)",
                             QString("The following %1 file(s) are not supported and will be skipped:\n• %2%3")
                                 .arg(unrecognizedPaths.size())
                                 .arg(sample, extra));
    }

    if (validPaths.isEmpty()) return;

    QString batchTargetFmt;
    QVariantMap batchOptions;
    bool batchApplied = false;

    for (int i = 0; i < validPaths.size(); ++i) {
        const QString& path = validPaths[i];
        const auto* inFmt = core::FormatRegistry::instance().findByFilePath(path);
        if (!inFmt) continue;

        QString targetFmt;
        QVariantMap options;

        // Check if user selected "Apply to all remaining compatible files"
        if (batchApplied) {
            const auto comp = core::FormatRegistry::instance().checkCompatibility(inFmt->id, batchTargetFmt);
            if (comp.isSupported) {
                targetFmt = batchTargetFmt;
                options = batchOptions;
            }
        }

        if (targetFmt.isEmpty()) {
            const int remainingCount = validPaths.size() - i;
            FormatSelectorDialog dlg(inFmt->id, this, remainingCount);
            if (dlg.exec() != QDialog::Accepted) {
                // User cancelled format selection; abort processing the rest of the batch
                break;
            }

            targetFmt = dlg.selectedFormatId();
            if (targetFmt.isEmpty()) break;

            options = dlg.selectedPresetOptions();

            // If converting an image to a lossy target without preset, offer interactive before/after preview
            if (inFmt->category == core::MediaCategory::Image &&
                (targetFmt == "webp" || targetFmt == "jpg" || targetFmt == "jpeg" || targetFmt == "avif") &&
                dlg.selectedPresetId().isEmpty()) {
                ImagePreviewDialog previewDlg(path, targetFmt, this);
                if (previewDlg.exec() == QDialog::Accepted) {
                    options["quality"] = previewDlg.quality();
                } else {
                    // User cancelled the preview / conversion
                    break;
                }
            }

            if (dlg.applyToAllRemaining() && remainingCount > 1) {
                batchApplied = true;
                batchTargetFmt = targetFmt;
                batchOptions = options;
            }
        }

        const auto& settings = app::AppSettings::instance();
        const auto plan = core::OutputPathPlanner::planOutputPath(
            path, targetFmt, settings.destinationMode(), settings.customDestinationPath());

        if (!plan.isValid) {
            QMessageBox::warning(this, "Output Path Error", plan.errorMessage);
            continue;
        }

        const auto comp = core::FormatRegistry::instance().checkCompatibility(inFmt->id, targetFmt);
        core::EngineType engineType = core::EngineType::Unknown;
        if (comp.responsibleEngine == "FFmpeg") engineType = core::EngineType::FFmpeg;
        else if (comp.responsibleEngine == "ImageMagick") engineType = core::EngineType::ImageMagick;
        else if (comp.responsibleEngine == "Pandoc") engineType = core::EngineType::Pandoc;

        const QString jobId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto job = std::make_shared<core::ConversionJob>(
            jobId, QStringList{path}, inFmt->id, targetFmt, plan.targetDirectory, engineType);
        job->setFinalOutputPath(plan.finalFilePath);
        job->setOptions(options);

        m_orchestrator->addJob(job);
    }
}

void MainWindow::onBrowseFilesClicked() {
    const QStringList files = QFileDialog::getOpenFileNames(this, "Select Media or Document Files to Convert");
    if (!files.isEmpty()) {
        onFilesDropped(files);
    }
}

void MainWindow::onOpenSettingsClicked() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_orchestrator->setMaxConcurrency(app::AppSettings::instance().maxConcurrentJobs());
        syncWatchFolders();
    }
}

void MainWindow::syncWatchFolders() {
    if (!m_watchFolderService) return;

    const QString jsonStr = app::AppSettings::instance().watchFoldersJson();
    const auto doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray()) return;

    const auto arr = doc.array();
    for (const auto& val : arr) {
        const auto cfg = core::WatchFolderConfig::fromJson(val.toObject());
        m_watchFolderService->addFolder(cfg);
    }
}

void MainWindow::onOpenHistoryClicked() {
    HistoryDialog dlg(this);
    connect(&dlg, &HistoryDialog::retryJobRequested, this, &MainWindow::onRetryHistoryJob);
    dlg.exec();
}

void MainWindow::onRetryHistoryJob(const core::HistoryRecord& record) {
    if (!QFile::exists(record.primaryInputPath)) {
        QMessageBox::warning(this, "File Not Found", "The original source file could not be found.");
        return;
    }

    const auto* inFmt = core::FormatRegistry::instance().findByFilePath(record.primaryInputPath);
    if (!inFmt) return;

    const auto& settings = app::AppSettings::instance();
    const auto plan = core::OutputPathPlanner::planOutputPath(
        record.primaryInputPath, record.outputFormatId, settings.destinationMode(), settings.customDestinationPath());

    if (!plan.isValid) {
        QMessageBox::warning(this, "Output Path Error", plan.errorMessage);
        return;
    }

    const auto comp = core::FormatRegistry::instance().checkCompatibility(inFmt->id, record.outputFormatId);
    core::EngineType engineType = core::EngineType::Unknown;
    if (comp.responsibleEngine == "FFmpeg") engineType = core::EngineType::FFmpeg;
    else if (comp.responsibleEngine == "ImageMagick") engineType = core::EngineType::ImageMagick;
    else if (comp.responsibleEngine == "Pandoc") engineType = core::EngineType::Pandoc;

    const QString jobId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto job = std::make_shared<core::ConversionJob>(
        jobId, QStringList{record.primaryInputPath}, inFmt->id, record.outputFormatId, plan.targetDirectory, engineType);
    job->setFinalOutputPath(plan.finalFilePath);
    job->setOptions(record.options);

    m_orchestrator->addJob(job);
    m_statusLabel->setText(QString("Retrying job: %1").arg(record.inputFileName));
}

} // namespace uc::ui
