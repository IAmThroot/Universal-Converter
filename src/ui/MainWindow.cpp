#include "MainWindow.h"
#include "BatchSetupDialog.h"
#include "WatchFoldersDialog.h"
#include "../engines/PandocCapabilities.h"
#include "../core/JobFactory.h"
#include <QCloseEvent>
#include "AppIcon.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_orchestrator(std::make_shared<core::ConversionOrchestrator>(
                               app::AppSettings::instance().maxConcurrentJobs())) {
    m_orchestrator->setFfmpegEngine(std::make_shared<engines::FfmpegEngine>(nullptr));
    m_orchestrator->setImageMagickEngine(std::make_shared<engines::ImageMagickEngine>(nullptr));
    m_orchestrator->setPandocEngine(std::make_shared<engines::PandocEngine>(nullptr));

    m_watchFolderService = std::make_shared<core::WatchFolderService>(m_orchestrator.get(), this);
    syncWatchFolders();

    setWindowTitle("Universal Converter");
    setWindowIcon(appIcon());
    resize(980, 700);
    setMinimumSize(720, 520);

    setupUi();
    setupMenuBar();
    setupStatusBar();

    connect(&app::ToolchainManager::instance(), &app::ToolchainManager::toolchainStatusChanged, this,
            &MainWindow::updateToolchainStatus);

    auto updateQueueStatus = [this]() {
        const QString storageError = core::ConversionHistoryManager::instance().storageError();
        m_statusLabel->setText(m_orchestrator->summary() +
                               (storageError.isEmpty() ? QString() : " · History could not be saved"));
        m_statusLabel->setToolTip(storageError);
        m_dropZone->setFixedHeight(m_orchestrator->totalJobCount() ? 80 : 140);
    };

    connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobAdded, this, updateQueueStatus);
    connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobStarted, this, updateQueueStatus);
    connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobFinished, this,
            [updateQueueStatus](const QString &, core::JobState) { updateQueueStatus(); });
    connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobRemoved, this,
            [updateQueueStatus](const QString &) { updateQueueStatus(); });
    connect(m_orchestrator.get(), &core::ConversionOrchestrator::queueDrained, this, updateQueueStatus);

    updateToolchainStatus();
}

void MainWindow::setupUi() {
    auto *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");

    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);

    // Header layout
    auto *headerLayout = new QHBoxLayout();

    auto *brandLayout = new QHBoxLayout();
    brandLayout->setSpacing(10);
    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(appIcon().pixmap(28, 28));
    auto *titleLabel = new QLabel("Universal Converter", this);
    titleLabel->setObjectName("brandTitle");

    brandLayout->addWidget(iconLabel);
    brandLayout->addWidget(titleLabel);

    m_historyBtn = new QPushButton("History", this);

    connect(m_historyBtn, &QPushButton::clicked, this, &MainWindow::onOpenHistoryClicked);

    m_settingsBtn = new QPushButton("Settings", this);
    m_settingsBtn->setIcon(QIcon(":/icons/settings.svg"));
    m_historyBtn->setIcon(QIcon(":/icons/history.svg"));

    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onOpenSettingsClicked);

    headerLayout->addLayout(brandLayout);
    headerLayout->addStretch();
    auto *watches = new QPushButton("Watch Folders", this);
    watches->setIcon(QIcon(":/icons/folder.svg"));
    connect(watches, &QPushButton::clicked, this, [this]() {
        WatchFoldersDialog dialog(m_watchFolderService.get(), this);
        dialog.exec();
    });
    headerLayout->addWidget(watches);
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
    auto *fileMenu = menuBar()->addMenu("&File");
    auto *openAction =
        fileMenu->addAction("&Open Files...", QKeySequence::Open, this, &MainWindow::onBrowseFilesClicked);
    Q_UNUSED(openAction);
    fileMenu->addSeparator();
    auto *exitAction = fileMenu->addAction("E&xit", QKeySequence::Quit, this, &QWidget::close);
    Q_UNUSED(exitAction);

    auto *toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction("&Conversion History...", QKeySequence(Qt::CTRL | Qt::Key_H), this,
                         &MainWindow::onOpenHistoryClicked);
    toolsMenu->addAction("&Settings...", QKeySequence::Preferences, this, &MainWindow::onOpenSettingsClicked);

    auto *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About Universal Converter", [this]() {
        QMessageBox::about(
            this, "About Universal Converter",
            "<h3>Universal Converter " UC_VERSION "</h3>"
            "<p><b>Created & Developed by IAmThroot</b></p>"
            "<p>A high-performance, offline desktop converter powered by C++20 and Qt 6.</p>"
            "<p><a href=\"https://github.com/IAmThroot/Universal-Converter#readme\">Documentation & Source "
            "Code</a></p>");
    });
}

void MainWindow::setupStatusBar() {
    m_statusLabel = new QLabel("Ready", this);
    m_toolchainStatusLabel = new QLabel(this);
    connect(m_toolchainStatusLabel, &QLabel::linkActivated, this, [this]() {
        QMessageBox::information(this, "Conversion tools",
                                 m_toolchainStatusLabel->toolTip() +
                                     "\n\nConfigure executable paths in Settings. Documents to PDF require "
                                     "Pandoc and a PDF typesetter.");
    });

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_toolchainStatusLabel);

    updateToolchainStatus();
}

void MainWindow::updateToolchainStatus() {
    const auto &tc = app::ToolchainManager::instance();
    if (tc.isScanning()) {
        m_toolchainStatusLabel->setText("Scanning tools...");
        return;
    }

    auto formatTool = [](const char *name, const app::ResolvedTool &tool) -> QString {
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

    const bool pdf = engines::PandocCapabilities::instance().hasAnyPdfEngine();
    const int ready =
        int(tc.ffmpeg().isAvailable) + int(tc.magick().isAvailable) + int(tc.pandoc().isAvailable) + int(pdf);
    m_toolchainStatusLabel->setText(
        QString("<a style=\"color:#34d399\" href=\"tools\">Tools: %1/4 ready</a>").arg(ready));
    statusParts << QString("PDF: %1").arg(pdf ? "Ready" : "Missing typesetter");
    m_toolchainStatusLabel->setToolTip(statusParts.join("\n"));
}

void MainWindow::onFilesDropped(const QStringList &filePaths) {
    if (filePaths.isEmpty())
        return;

    BatchSetupDialog dialog(filePaths, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QList<std::shared_ptr<core::ConversionJob>> jobs;
    for (const auto &entry : dialog.entries()) {
        auto result = core::JobFactory::create(entry.input, entry.output, entry.options,
                                               dialog.destinationMode(), dialog.destination());
        if (!result.job) {
            for (const auto &job : jobs)
                core::OutputPathPlanner::releasePath(job->finalOutputPath());
            QMessageBox::warning(this, "Batch needs attention", result.error);
            return;
        }
        jobs << result.job;
    }
    for (const auto &job : jobs)
        m_orchestrator->addJob(job);
}

void MainWindow::onBrowseFilesClicked() {
    const QStringList files =
        QFileDialog::getOpenFileNames(this, "Select Media or Document Files to Convert");
    if (!files.isEmpty()) {
        onFilesDropped(files);
    }
}

void MainWindow::onOpenSettingsClicked() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_orchestrator->setMaxConcurrency(app::AppSettings::instance().maxConcurrentJobs());
    }
}

void MainWindow::syncWatchFolders() {
    if (!m_watchFolderService)
        return;

    const QString jsonStr = app::AppSettings::instance().watchFoldersJson();
    const auto doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray())
        return;

    const auto arr = doc.array();
    QSet<QString> activeFolderIds;
    for (const auto &val : arr) {
        const auto cfg = core::WatchFolderConfig::fromJson(val.toObject());
        activeFolderIds.insert(cfg.id);
        if (m_watchFolderService->hasFolder(cfg.id)) {
            m_watchFolderService->updateFolder(cfg);
        } else {
            m_watchFolderService->addFolder(cfg);
        }
        m_watchFolderService->setFolderEnabled(cfg.id, cfg.isEnabled);
    }

    // Remove any folders that were removed in Settings
    for (const auto &existing : m_watchFolderService->configuredFolders()) {
        if (!activeFolderIds.contains(existing.id)) {
            m_watchFolderService->removeFolder(existing.id);
        }
    }
}

void MainWindow::onOpenHistoryClicked() {
    HistoryDialog dlg(this);
    connect(&dlg, &HistoryDialog::retryJobRequested, this, &MainWindow::onRetryHistoryJob);
    dlg.exec();
}

void MainWindow::onRetryHistoryJob(const core::HistoryRecord &record) {
    if (!QFile::exists(record.primaryInputPath)) {
        QMessageBox::warning(this, "File Not Found", "The original source file could not be found.");
        return;
    }

    const auto &settings = app::AppSettings::instance();
    auto result = core::JobFactory::create(record.primaryInputPath, record.outputFormatId, record.options,
                                           settings.destinationMode(), settings.customDestinationPath());
    if (!result.job) {
        QMessageBox::warning(this, "Cannot retry", result.error);
        return;
    }
    m_orchestrator->addJob(result.job);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_orchestrator->activeJobCount() || !m_orchestrator->queuedJobs().isEmpty()) {
        QMessageBox box(QMessageBox::Question, "Conversions are active",
                        "Keep the application open, or cancel all jobs and exit?", QMessageBox::NoButton,
                        this);
        auto *keep = box.addButton("Keep Open", QMessageBox::RejectRole);
        auto *exit = box.addButton("Cancel Jobs and Exit", QMessageBox::DestructiveRole);
        box.setDefaultButton(keep);
        box.exec();
        if (box.clickedButton() != exit) {
            event->ignore();
            return;
        }
    }
    for (const auto &cfg : m_watchFolderService->configuredFolders())
        m_watchFolderService->setFolderEnabled(cfg.id, false);
    m_orchestrator->shutdown();
    event->accept();
}

} // namespace uc::ui
