#include "WatchFolderService.h"
#include "ConversionOrchestrator.h"
#include "FormatRegistry.h"
#include "OutputPathPlanner.h"
#include "PresetRegistry.h"
#include "JobFactory.h"
#include "../app/ToolchainManager.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QUuid>
#include <algorithm>

namespace uc::core {

WatchFolderService::WatchFolderService(ConversionOrchestrator *orchestrator, QObject *parent)
    : QObject(parent), m_orchestrator(orchestrator) {
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &WatchFolderService::onDirectoryChanged);

    // Poll timer runs every 1000ms for stable-size debouncing
    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &WatchFolderService::onPollTimerTimeout);
    m_pollTimer.start();
}

WatchFolderService::~WatchFolderService() { m_pollTimer.stop(); }

void WatchFolderService::addFolder(const WatchFolderConfig &config) {
    if (config.folderPath.isEmpty() || !QDir(config.folderPath).exists()) {
        return;
    }

    WatchFolderConfig cfg = config;
    if (cfg.id.isEmpty()) {
        cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    const QString root = QDir(cfg.folderPath).canonicalPath();
    for (const auto &existing : m_folders) {
        const QString other = QDir(existing.folderPath).canonicalPath();
        if (existing.id != cfg.id &&
            (root == other || root.startsWith(other + '/') || other.startsWith(root + '/'))) {
            m_lastError = "Watch folders must not overlap.";
            emit watchStatusChanged();
            return;
        }
    }
    m_folders.insert(cfg.id, cfg);
    m_pathToFolderId.insert(QDir(cfg.folderPath).canonicalPath(), cfg.id);

    if (cfg.isEnabled) {
        m_watcher.addPath(cfg.folderPath);
    }

    rebuildWatches();
    emit watchStatusChanged();
    scanDirectory(cfg);
}

void WatchFolderService::updateFolder(const WatchFolderConfig &config) {
    if (!m_folders.contains(config.id))
        return;
    clearPending(config.id);
    const auto oldCfg = m_folders[config.id];
    m_watcher.removePath(oldCfg.folderPath);
    m_pathToFolderId.remove(QDir(oldCfg.folderPath).canonicalPath());

    addFolder(config);
}

void WatchFolderService::removeFolder(const QString &folderId) {
    if (!m_folders.contains(folderId))
        return;
    clearPending(folderId);
    const auto cfg = m_folders.take(folderId);
    m_watcher.removePath(cfg.folderPath);
    m_pathToFolderId.remove(QDir(cfg.folderPath).canonicalPath());
    emit watchStatusChanged();
}

void WatchFolderService::setFolderEnabled(const QString &folderId, bool enabled) {
    if (!m_folders.contains(folderId))
        return;
    if (!enabled)
        clearPending(folderId);
    m_folders[folderId].isEnabled = enabled;
    if (enabled) {
        m_watcher.addPath(m_folders[folderId].folderPath);
        scanDirectory(m_folders[folderId]);
    } else {
        m_watcher.removePath(m_folders[folderId].folderPath);
    }
    emit watchStatusChanged();
}

QList<WatchFolderConfig> WatchFolderService::configuredFolders() const { return m_folders.values(); }

WatchFolderConfig WatchFolderService::folderConfig(const QString &folderId) const {
    return m_folders.value(folderId);
}

bool WatchFolderService::hasFolder(const QString &folderId) const { return m_folders.contains(folderId); }

void WatchFolderService::scanAllNow() {
    for (const auto &cfg : m_folders) {
        if (cfg.isEnabled) {
            scanDirectory(cfg);
        }
    }
}

void WatchFolderService::clearHistory() {
    m_processedFiles.clear();
    m_pendingCandidates.clear();
    m_failedFiles.clear();
}

void WatchFolderService::clearPending(const QString &id) {
    for (auto it = m_pendingCandidates.begin(); it != m_pendingCandidates.end();) {
        if (it->folderId == id)
            it = m_pendingCandidates.erase(it);
        else
            ++it;
    }
    const auto jobs = m_jobFolders.keys();
    for (const auto &jobId : jobs) {
        auto job = m_orchestrator->findJob(jobId);
        if (m_jobFolders.value(jobId) == id && job && job->state() == JobState::Queued)
            m_orchestrator->cancelJob(jobId);
    }
    rebuildWatches();
}

int WatchFolderService::pendingCount(const QString &id) const {
    int count = 0;
    for (const auto &candidate : m_pendingCandidates)
        count += candidate.folderId == id;
    return count;
}

void WatchFolderService::retryFailures() {
    m_failedFiles.clear();
    scanAllNow();
}

void WatchFolderService::rebuildWatches() {
    const auto paths = m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    m_pathToFolderId.clear();
    for (const auto &cfg : m_folders) {
        if (!cfg.isEnabled)
            continue;
        QStringList dirs{cfg.folderPath};
        if (cfg.isRecursive) {
            QDirIterator it(cfg.folderPath, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                            QDirIterator::Subdirectories);
            while (it.hasNext())
                dirs << it.next();
        }
        for (const auto &dir : dirs) {
            m_watcher.addPath(dir);
            m_pathToFolderId.insert(QDir(dir).canonicalPath(), cfg.id);
        }
    }
}

void WatchFolderService::onDirectoryChanged(const QString &) { scanAllNow(); }

void WatchFolderService::scanDirectory(const WatchFolderConfig &cfg) {
    if (!cfg.isEnabled || !QDir(cfg.folderPath).exists())
        return;

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

    const QString outputRoot =
        QDir(cfg.destinationFolder.isEmpty() ? QDir(cfg.folderPath).filePath("converted")
                                             : cfg.destinationFolder)
            .absolutePath();
    const QString processedRoot = QDir(cfg.folderPath).absoluteFilePath("processed");
    for (const auto &fi : entries) {
        const QString absPath = fi.absoluteFilePath();
        if (absPath.startsWith(outputRoot + '/') || absPath.startsWith(processedRoot + '/'))
            continue;
        if (m_activeFiles.contains(absPath))
            continue;
        const qint64 mtime = fi.lastModified().toMSecsSinceEpoch();
        if (m_failedFiles.contains(absPath) && m_failedFiles.value(absPath) == mtime)
            continue;
        auto procIt = m_processedFiles.find(absPath);
        if (procIt != m_processedFiles.end() && procIt.value() == mtime) {
            continue;
        }

        // Ignore temporary partial downloads
        const QString name = fi.fileName();
        if (name.startsWith('.') || name.endsWith(".crdownload") || name.endsWith(".part") ||
            name.endsWith(".tmp")) {
            continue;
        }

        // Check format compatibility
        const auto *inFmt = FormatRegistry::instance().findByFilePath(absPath);
        if (!inFmt || !inFmt->canInput)
            continue;

        if (!FormatRegistry::instance().isConversionSupported(inFmt->id, cfg.targetFormatId)) {
            continue;
        }

        // Queue as pending debounce candidate
        if (!m_pendingCandidates.contains(absPath)) {
            PendingCandidate cand;
            cand.filePath = absPath;
            cand.folderId = cfg.id;
            cand.lastSize = fi.size();
            cand.lastModified = mtime;
            cand.stableCount = 0;
            cand.zeroSizeCount = 0;
            m_pendingCandidates.insert(absPath, cand);
            emit fileDetected(absPath, cfg.id);
        }
    }
}

void WatchFolderService::onPollTimerTimeout() {
    if (++m_ticks % 5 == 0) {
        rebuildWatches();
        scanAllNow();
    }
    if (m_pendingCandidates.isEmpty())
        return;

    if (app::ToolchainManager::instance().isScanning())
        return;

    QList<QString> toProcess;
    QList<QString> toRemove;

    for (auto it = m_pendingCandidates.begin(); it != m_pendingCandidates.end(); ++it) {
        const QString &path = it.key();
        PendingCandidate &cand = it.value();

        const QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) {
            toRemove.append(path);
            continue;
        }

        if (!m_folders.contains(cand.folderId) || !m_folders.value(cand.folderId).isEnabled) {
            toRemove << path;
            continue;
        }
        const qint64 currentSize = fi.size();
        const qint64 modified = fi.lastModified().toMSecsSinceEpoch();
        if (currentSize == 0) {
            cand.zeroSizeCount++;
            // Evict files that remain 0-byte for over 60 poll intervals (60s)
            if (cand.zeroSizeCount > 60) {
                toRemove.append(path);
                continue;
            }
        }
        if (currentSize == cand.lastSize && modified == cand.lastModified && currentSize > 0) {
            cand.stableCount++;
            const auto cfg = m_folders.value(cand.folderId);
            const int requiredDebounce = qMax(1, cfg.debounceSeconds);
            if (cand.stableCount >= requiredDebounce) {
                toProcess.append(path);
            }
        } else {
            cand.lastSize = currentSize;
            cand.lastModified = modified;
            cand.stableCount = 0;
        }
    }

    for (const auto &path : toRemove) {
        m_pendingCandidates.remove(path);
    }

    for (const auto &path : toProcess) {
        PendingCandidate cand = m_pendingCandidates.take(path);
        const qint64 mtime = QFileInfo(path).lastModified().toMSecsSinceEpoch();
        Q_UNUSED(mtime);
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

void WatchFolderService::processStableCandidate(const PendingCandidate &candidate) {
    if (!m_orchestrator)
        return;
    if (!m_folders.contains(candidate.folderId))
        return;

    const auto cfg = m_folders[candidate.folderId];
    if (!cfg.isEnabled || m_activeFiles.contains(candidate.filePath))
        return;
    const QFileInfo source(candidate.filePath);
    if (!source.isFile() || source.size() != candidate.lastSize ||
        source.lastModified().toMSecsSinceEpoch() != candidate.lastModified)
        return;
    const auto *inFmt = FormatRegistry::instance().findByFilePath(candidate.filePath);
    if (!inFmt)
        return;

    // Plan output destination
    QString destDir = cfg.destinationFolder;
    if (destDir.isEmpty()) {
        destDir = QDir(cfg.folderPath).filePath("converted");
    }
    QDir().mkpath(destDir);

    QVariantMap options;
    if (!cfg.presetId.isEmpty()) {
        const auto *preset = PresetRegistry::instance().findById(cfg.presetId);
        if (preset)
            options = preset->options;
    }
    auto result = JobFactory::create(candidate.filePath, cfg.targetFormatId, options,
                                     app::DestinationMode::CustomFolder, destDir);
    if (!result.job) {
        m_failedFiles.insert(candidate.filePath, candidate.lastModified);
        m_activity[cfg.id] = result.error;
        emit watchStatusChanged();
        return;
    }
    auto job = result.job;
    const QString jobId = job->id();

    m_activeFiles.insert(candidate.filePath, jobId);
    m_jobFolders.insert(jobId, cfg.id);
    m_activity[cfg.id] = "Converting " + source.fileName();
    auto completionConnection = std::make_shared<QMetaObject::Connection>();
    *completionConnection = connect(
        m_orchestrator, &ConversionOrchestrator::jobFinished, this,
        [this, candidate, cfg, jobId, completionConnection](const QString &id, JobState state) {
            if (id != jobId)
                return;
            disconnect(*completionConnection);
            m_activeFiles.remove(candidate.filePath);
            m_jobFolders.remove(id);
            const QFileInfo source(candidate.filePath);
            const bool unchanged = source.exists() && source.size() == candidate.lastSize &&
                                   source.lastModified().toMSecsSinceEpoch() == candidate.lastModified;
            if (state == JobState::Completed) {
                if (unchanged)
                    m_processedFiles.insert(candidate.filePath, candidate.lastModified);
                QString error;
                if (!unchanged && cfg.postAction != WatchPostAction::KeepOriginal)
                    error = "Original changed; post-action skipped";
                if (unchanged && cfg.postAction == WatchPostAction::MoveToProcessed) {
                    const QString dir = QDir(cfg.folderPath).filePath("processed");
                    QDir().mkpath(dir);
                    const QString dest =
                        OutputPathPlanner::resolveUniquePath(QDir(dir).filePath(source.fileName()));
                    if (!QFile::rename(candidate.filePath, dest))
                        error = "Converted; could not archive original";
                } else if (unchanged && cfg.postAction == WatchPostAction::DeleteOriginal) {
                    if (!QFile::remove(candidate.filePath))
                        error = "Converted; could not delete original";
                }
                m_activity[cfg.id] = error.isEmpty()
                                         ? "Succeeded at " + QDateTime::currentDateTime().toString("HH:mm:ss")
                                         : error;
            } else {
                m_failedFiles.insert(candidate.filePath, candidate.lastModified);
                m_activity[cfg.id] = "Failed/cancelled: " + source.fileName() + " — use Retry failures";
            }
            emit watchStatusChanged();
        });

    m_orchestrator->addJob(job);
    emit jobDispatched(jobId, candidate.filePath);
}

} // namespace uc::core
