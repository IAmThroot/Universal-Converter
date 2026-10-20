#include "WatchFolderService.h"
#include "ConversionOrchestrator.h"
#include "FormatRegistry.h"
#include "OutputPathPlanner.h"
#include "PresetRegistry.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QUuid>
#include <algorithm>

namespace uc::core {

WatchFolderService::WatchFolderService(ConversionOrchestrator* orchestrator, QObject* parent)
    : QObject(parent)
    , m_orchestrator(orchestrator)
{
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &WatchFolderService::onDirectoryChanged);

    // Poll timer runs every 1000ms for stable-size debouncing
    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &WatchFolderService::onPollTimerTimeout);
    m_pollTimer.start();
}

WatchFolderService::~WatchFolderService() {
    m_pollTimer.stop();
}

void WatchFolderService::addFolder(const WatchFolderConfig& config) {
    if (config.folderPath.isEmpty() || !QDir(config.folderPath).exists()) {
        return;
    }

    WatchFolderConfig cfg = config;
    if (cfg.id.isEmpty()) {
        cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    m_folders.insert(cfg.id, cfg);
    m_pathToFolderId.insert(QDir(cfg.folderPath).canonicalPath(), cfg.id);

    if (cfg.isEnabled) {
        m_watcher.addPath(cfg.folderPath);
    }

    emit watchStatusChanged();
    scanDirectory(cfg);
}

void WatchFolderService::updateFolder(const WatchFolderConfig& config) {
    if (!m_folders.contains(config.id)) return;
    const auto oldCfg = m_folders[config.id];
    m_watcher.removePath(oldCfg.folderPath);
    m_pathToFolderId.remove(QDir(oldCfg.folderPath).canonicalPath());

    addFolder(config);
}

void WatchFolderService::removeFolder(const QString& folderId) {
    if (!m_folders.contains(folderId)) return;
    const auto cfg = m_folders.take(folderId);
    m_watcher.removePath(cfg.folderPath);
    m_pathToFolderId.remove(QDir(cfg.folderPath).canonicalPath());
    emit watchStatusChanged();
}

void WatchFolderService::setFolderEnabled(const QString& folderId, bool enabled) {
    if (!m_folders.contains(folderId)) return;
    m_folders[folderId].isEnabled = enabled;
    if (enabled) {
        m_watcher.addPath(m_folders[folderId].folderPath);
        scanDirectory(m_folders[folderId]);
    } else {
        m_watcher.removePath(m_folders[folderId].folderPath);
    }
    emit watchStatusChanged();
}

QList<WatchFolderConfig> WatchFolderService::configuredFolders() const {
    return m_folders.values();
}

WatchFolderConfig WatchFolderService::folderConfig(const QString& folderId) const {
    return m_folders.value(folderId);
}

bool WatchFolderService::hasFolder(const QString& folderId) const {
    return m_folders.contains(folderId);
}

void WatchFolderService::scanAllNow() {
    for (const auto& cfg : m_folders) {
        if (cfg.isEnabled) {
            scanDirectory(cfg);
        }
    }
}

void WatchFolderService::clearHistory() {
    m_processedFiles.clear();
    m_pendingCandidates.clear();
}

void WatchFolderService::onDirectoryChanged(const QString& path) {
    const QString canon = QDir(path).canonicalPath();
    const QString folderId = m_pathToFolderId.value(canon);
    if (!folderId.isEmpty() && m_folders.contains(folderId)) {
        scanDirectory(m_folders[folderId]);
    }
}

void WatchFolderService::scanDirectory(const WatchFolderConfig& cfg) {
    if (!cfg.isEnabled || !QDir(cfg.folderPath).exists()) return;

    QDir dir(cfg.folderPath);
    QDir::Filters filters = QDir::Files | QDir::NoSymLinks | QDir::Readable;
    QFileInfoList entries;
    if (cfg.isRecursive) {
        QDirIterator it(cfg.folderPath, filters, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            entries.append(it.fileInfo());
        }
    } else {
        entries = dir.entryInfoList(filters, QDir::Time);
    }

    for (const auto& fi : entries) {
        const QString absPath = fi.absoluteFilePath();
        const qint64 mtime = fi.lastModified().toMSecsSinceEpoch();
        auto procIt = m_processedFiles.find(absPath);
        if (procIt != m_processedFiles.end() && procIt.value() == mtime) {
            continue;
        }

        // Ignore temporary partial downloads
        const QString name = fi.fileName();
        if (name.startsWith('.') || name.endsWith(".crdownload") || name.endsWith(".part") || name.endsWith(".tmp")) {
            continue;
        }

        // Check format compatibility
        const auto* inFmt = FormatRegistry::instance().findByFilePath(absPath);
        if (!inFmt || !inFmt->canInput) continue;

        if (!FormatRegistry::instance().isConversionSupported(inFmt->id, cfg.targetFormatId)) {
            continue;
        }

        // Queue as pending debounce candidate
        if (!m_pendingCandidates.contains(absPath)) {
            PendingCandidate cand;
            cand.filePath = absPath;
            cand.folderId = cfg.id;
            cand.lastSize = fi.size();
            cand.stableCount = 0;
            cand.zeroSizeCount = 0;
            m_pendingCandidates.insert(absPath, cand);
            emit fileDetected(absPath, cfg.id);
        }
    }
}

void WatchFolderService::onPollTimerTimeout() {
    if (m_pendingCandidates.isEmpty()) return;

    QList<QString> toProcess;
    QList<QString> toRemove;

    for (auto it = m_pendingCandidates.begin(); it != m_pendingCandidates.end(); ++it) {
        const QString& path = it.key();
        PendingCandidate& cand = it.value();

        const QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) {
            toRemove.append(path);
            continue;
        }

        const qint64 currentSize = fi.size();
        if (currentSize == 0) {
            cand.zeroSizeCount++;
            // Evict files that remain 0-byte for over 60 poll intervals (60s)
            if (cand.zeroSizeCount > 60) {
                toRemove.append(path);
                continue;
            }
        }
        if (currentSize == cand.lastSize && currentSize > 0) {
            cand.stableCount++;
            const auto cfg = m_folders.value(cand.folderId);
            const int requiredDebounce = qMax(1, cfg.debounceSeconds);
            if (cand.stableCount >= requiredDebounce) {
                toProcess.append(path);
            }
        } else {
            cand.lastSize = currentSize;
            cand.stableCount = 0;
        }
    }

    for (const auto& path : toRemove) {
        m_pendingCandidates.remove(path);
    }

    for (const auto& path : toProcess) {
        PendingCandidate cand = m_pendingCandidates.take(path);
        const qint64 mtime = QFileInfo(path).lastModified().toMSecsSinceEpoch();
        m_processedFiles.insert(path, mtime);
        // Bound memory: prune entries that no longer exist on disk (deleted/moved)
        if (m_processedFiles.size() > 10000) {
            for (auto it = m_processedFiles.begin(); it != m_processedFiles.end();) {
                if (!QFile::exists(it.key())) {
                    it = m_processedFiles.erase(it);
                } else {
                    ++it;
                }
            }
            // If still exceeding 10,000, prune oldest entries by mtime
            if (m_processedFiles.size() > 10000) {
                QList<QPair<qint64, QString>> entries;
                entries.reserve(m_processedFiles.size());
                for (auto it = m_processedFiles.constBegin(); it != m_processedFiles.constEnd(); ++it) {
                    entries.append({it.value(), it.key()});
                }
                std::sort(entries.begin(), entries.end());
                int removeCount = m_processedFiles.size() - 8000;
                for (int i = 0; i < removeCount; ++i) {
                    m_processedFiles.remove(entries[i].second);
                }
            }
        }
        processStableCandidate(cand);
    }
}

void WatchFolderService::processStableCandidate(const PendingCandidate& candidate) {
    if (!m_orchestrator) return;
    if (!m_folders.contains(candidate.folderId)) return;

    const auto cfg = m_folders[candidate.folderId];
    const auto* inFmt = FormatRegistry::instance().findByFilePath(candidate.filePath);
    if (!inFmt) return;

    // Plan output destination
    QString destDir = cfg.destinationFolder;
    if (destDir.isEmpty()) {
        destDir = QDir(cfg.folderPath).filePath("converted");
    }
    QDir().mkpath(destDir);

    const auto plan = OutputPathPlanner::planOutputPath(
        candidate.filePath, cfg.targetFormatId, app::DestinationMode::CustomFolder, destDir);

    if (!plan.isValid) return;

    const auto comp = FormatRegistry::instance().checkCompatibility(inFmt->id, cfg.targetFormatId);
    EngineType engineType = EngineType::Unknown;
    if (comp.responsibleEngine == "FFmpeg") engineType = EngineType::FFmpeg;
    else if (comp.responsibleEngine == "ImageMagick") engineType = EngineType::ImageMagick;
    else if (comp.responsibleEngine == "Pandoc") engineType = EngineType::Pandoc;

    const QString jobId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto job = std::make_shared<ConversionJob>(
        jobId, QStringList{candidate.filePath}, inFmt->id, cfg.targetFormatId, plan.targetDirectory, engineType);
    job->setFinalOutputPath(plan.finalFilePath);

    if (!cfg.presetId.isEmpty()) {
        const auto* preset = PresetRegistry::instance().findById(cfg.presetId);
        if (preset) {
            job->setOptions(preset->options);
        }
    }

    // Connect post-action on completion (e.g. MoveToProcessed or DeleteOriginal)
    if (cfg.postAction != WatchPostAction::KeepOriginal) {
        connect(job.get(), &ConversionJob::stateChanged, this, [candidate, cfg](JobState newState) {
            if (newState == JobState::Completed) {
                if (cfg.postAction == WatchPostAction::MoveToProcessed) {
                    const QString processedDir = QDir(cfg.folderPath).filePath("processed");
                    QDir().mkpath(processedDir);
                    const QString targetMovedPath = QDir(processedDir).filePath(QFileInfo(candidate.filePath).fileName());
                    if (QFile::exists(targetMovedPath)) {
                        QFile::remove(targetMovedPath);
                    }
                    QFile::rename(candidate.filePath, targetMovedPath);
                } else if (cfg.postAction == WatchPostAction::DeleteOriginal) {
                    QFile::remove(candidate.filePath);
                }
            }
        });
    }

    m_orchestrator->addJob(job);
    emit jobDispatched(jobId, candidate.filePath);
}

} // namespace uc::core
