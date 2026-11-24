#include "WatchFolderService.h"
#include "ConversionOrchestrator.h"
#include "FormatRegistry.h"
#include "OutputPathPlanner.h"
#include "PresetRegistry.h"
#include "JobFactory.h"
#include "../app/ToolchainManager.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QUuid>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <filesystem>

namespace uc::core {

WatchFolderService::WatchFolderService(ConversionOrchestrator *orchestrator, QObject *parent)
    : QObject(parent), m_orchestrator(orchestrator) {
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &WatchFolderService::onDirectoryChanged);

    // Poll timer runs every 1000ms for stable-size debouncing
    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &WatchFolderService::onPollTimerTimeout);
    m_pollTimer.start();
}

WatchFolderService::~WatchFolderService() { m_scanPool.waitForDone(); }

static QString stateFile(const QString &id) {
    QString dir = QCoreApplication::instance()->property("testDataDirectory").toString();
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dir = QDir(dir).filePath("watch-state");
    QDir().mkpath(dir);
    return QDir(dir).filePath(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex() +
                              ".json");
}

bool WatchFolderService::saveState(const WatchFolderConfig &cfg) {
    QSaveFile file(stateFile(cfg.id));
    const auto data =
        QJsonDocument(QJsonObject{{"root", cfg.folderPath}, {"files", m_processedFiles.value(cfg.id)}})
            .toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

void WatchFolderService::pauseWithError(const QString &id, const QString &message) {
    m_storageBlocked.insert(id);
    setFolderEnabled(id, false);
    m_activity[id] = m_lastError = message;
    emit watchStatusChanged();
}

static bool containsPath(const QString &directory, const QString &path) {
    const auto relative = QDir(directory).relativeFilePath(path);
    return !QDir::isAbsolutePath(relative) && relative != ".." && !relative.startsWith("../");
}

static QString destinationRoot(const WatchFolderConfig &cfg) {
    const QFileInfo dest(cfg.destinationFolder.isEmpty() ? QDir(cfg.folderPath).filePath("converted")
                                                         : cfg.destinationFolder);
    return dest.exists() ? dest.canonicalFilePath() : QDir::cleanPath(dest.absoluteFilePath());
}

bool WatchFolderService::addFolder(const WatchFolderConfig &config, bool restored) {
    auto cfg = config;
    const QFileInfo root(cfg.folderPath);
    if (cfg.folderPath.trimmed().isEmpty() || (!root.isDir() && !(restored && root.isAbsolute()))) {
        m_lastError = "Choose an existing watch folder.";
        return false;
    }
    cfg.folderPath = root.isDir() ? root.canonicalFilePath() : root.absoluteFilePath();
    for (const auto &other : m_folders) {
        if (other.id != cfg.id && (containsPath(other.folderPath, cfg.folderPath) ||
                                   containsPath(cfg.folderPath, other.folderPath))) {
            m_lastError = "Watch folders must not overlap.";
            return false;
        }
    }
    if (!restored && containsPath(destinationRoot(cfg), cfg.folderPath)) {
        m_lastError =
            "The output directory contains the watch folder. Choose a separate destination or a subfolder.";
        return false;
    }
    if (cfg.id.isEmpty())
        cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const bool sameRoot = hasFolder(cfg.id) && m_folders.value(cfg.id).folderPath == cfg.folderPath;
    if (hasFolder(cfg.id)) {
        clearPending(cfg.id);
        unwatch(cfg.id);
    }
    ++m_generation[cfg.id];
    m_folders[cfg.id] = cfg;
    m_lastError.clear();
    if (!sameRoot) {
        m_storageBlocked.remove(cfg.id);
        m_processedFiles.remove(cfg.id);
        m_failedFiles.remove(cfg.id);
        m_needsBaseline.remove(cfg.id);
        QFile file(stateFile(cfg.id));
        if (restored && file.exists()) {
            const auto obj = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object()
                                                            : QJsonObject{};
            if (obj.value("root") == cfg.folderPath && obj.value("files").isObject()) {
                m_processedFiles[cfg.id] = obj.value("files").toObject();
            } else {
                m_needsBaseline.insert(cfg.id);
                pauseWithError(cfg.id, "Cannot read watch history. Resume to baseline existing files.");
                return true;
            }
        } else if (restored) {
            m_needsBaseline.insert(cfg.id);
        } else if (!saveState(cfg)) {
            m_needsBaseline.insert(cfg.id);
            pauseWithError(cfg.id, "Cannot save watch history. Check application data permissions.");
            return true;
        }
    }
    scanDirectory(cfg);
    emit watchStatusChanged();
    return true;
}

bool WatchFolderService::updateFolder(const WatchFolderConfig &config) {
    return hasFolder(config.id) && addFolder(config);
}

void WatchFolderService::unwatch(const QString &id) {
    for (const auto &path : m_pathToFolderId.keys(id)) {
        if (m_watcher.directories().contains(path))
            m_watcher.removePath(path);
        m_pathToFolderId.remove(path);
    }
}

void WatchFolderService::removeFolder(const QString &id) {
    if (!m_folders.remove(id))
        return;
    ++m_generation[id];
    clearPending(id);
    unwatch(id);
    m_storageBlocked.remove(id);
    m_needsBaseline.remove(id);
    m_processedFiles.remove(id);
    m_failedFiles.remove(id);
    m_activity.remove(id);
    QFile::remove(stateFile(id));
    emit watchStatusChanged();
}

void WatchFolderService::setFolderEnabled(const QString &id, bool enabled) {
    if (!hasFolder(id) || m_folders[id].isEnabled == enabled)
        return;
    ++m_generation[id];
    m_folders[id].isEnabled = enabled;
    if (enabled) {
        if (m_storageBlocked.remove(id) && !m_needsBaseline.contains(id) && !saveState(m_folders[id])) {
            pauseWithError(id, "Cannot save watch history. Check application data permissions.");
            return;
        }
        scanDirectory(m_folders[id]);
    } else {
        clearPending(id);
        unwatch(id);
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
        if (cfg.isEnabled || m_needsBaseline.contains(cfg.id)) {
            scanDirectory(cfg);
        }
    }
}

void WatchFolderService::clearHistory() {
    m_processedFiles.clear();
    for (const auto &cfg : m_folders)
        if (!m_needsBaseline.contains(cfg.id) && !saveState(cfg))
            pauseWithError(cfg.id, "Cannot clear saved watch history.");
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

void WatchFolderService::onDirectoryChanged(const QString &path) {
    const auto id = m_pathToFolderId.value(path);
    if (hasFolder(id))
        scanDirectory(m_folders[id], path);
}

struct DirectoryScan {
    QSet<QString> visited;
    QHash<QString, QJsonArray> files;
    QString error;
};

static DirectoryScan collectDirectory(const WatchFolderConfig &cfg, const QString &directory) {
    namespace fs = std::filesystem;
    const QDir root(cfg.folderPath);
    const QString output = destinationRoot(cfg);
    const QString processed = root.filePath("processed");
    QStringList todo{directory};
    DirectoryScan result;
    if (containsPath(output, directory)) {
        result.error =
            "The output directory contains the watch folder. Choose a separate destination or a subfolder.";
        return result;
    }
    while (!todo.isEmpty() && result.error.isEmpty()) {
        const QString path = todo.takeLast();
        if (QFileInfo(path).canonicalFilePath() != path) {
            result.error = "Cannot scan " + path + ": directory is missing or has been replaced.";
            break;
        }
        std::error_code ec;
        fs::directory_iterator it(QDir(path).filesystemPath(), ec), end;
        for (; !ec && it != end; it.increment(ec)) {
            const QFileInfo fi(it->path());
            if (fi.isSymLink() || fi.isHidden())
                continue;
            const QString abs = fi.absoluteFilePath();
            if (abs == output || abs == processed)
                continue;
            if (cfg.isRecursive && fi.isDir())
                todo.append(abs);
            if (fi.isFile() && fi.isReadable() && !abs.endsWith(".part") && !abs.endsWith(".tmp") &&
                !abs.endsWith(".crdownload"))
                result.files.insert(root.relativeFilePath(abs),
                                    {fi.size(), fi.lastModified().toMSecsSinceEpoch()});
        }
        if (ec)
            result.error = "Cannot scan " + path + ": " + QString::fromStdString(ec.message());
        else
            result.visited.insert(path);
    }
    return result;
}

void WatchFolderService::applyScan(const WatchFolderConfig &cfg, const QString &directory,
                                   const DirectoryScan &scan, bool baseline) {
    const QDir root(cfg.folderPath);
    const auto &visited = scan.visited;
    const auto &files = scan.files;
    auto &known = m_processedFiles[cfg.id];
    auto &failed = m_failedFiles[cfg.id];
    for (auto it = failed.begin(); it != failed.end();) {
        if (visited.contains(QFileInfo(root.filePath(it.key())).absolutePath()) && !files.contains(it.key()))
            it = failed.erase(it);
        else
            ++it;
    }
    bool changed = baseline;
    for (auto it = known.begin(); it != known.end();) {
        if (visited.contains(QFileInfo(root.filePath(it.key())).absolutePath()) &&
            !files.contains(it.key()) && !QFileInfo::exists(root.filePath(it.key()))) {
            it = known.erase(it);
            changed = true;
        } else
            ++it;
    }
    for (auto it = files.begin(); it != files.end(); ++it) {
        if (baseline) {
            known.insert(it.key(), it.value());
            continue;
        }
        const QString path = root.filePath(it.key());
        const auto size = it.value()[0].toInteger();
        const auto modified = it.value()[1].toInteger();
        if (known.value(it.key()) == it.value() || m_activeFiles.contains(path) ||
            m_pendingCandidates.contains(path) || failed.value(it.key()) == it.value())
            continue;
        const auto *fmt = FormatRegistry::instance().findByFilePath(path);
        if (!fmt || !(FormatRegistry::instance().isConversionSupported(fmt->id, cfg.targetFormatId) ||
                      (fmt->id == cfg.targetFormatId && !cfg.presetId.isEmpty())))
            continue;
        m_pendingCandidates.insert(path, {path, cfg.id, size, modified, 0, 0});
        emit fileDetected(path, cfg.id);
    }
    if (changed && !saveState(cfg)) {
        pauseWithError(cfg.id, "Cannot save watch history. Check application data permissions.");
    } else {
        m_needsBaseline.remove(cfg.id);
        if (baseline)
            m_activity[cfg.id] = QString("Recorded %1 existing files").arg(known.size());
        if (cfg.isEnabled) {
            const auto current = m_watcher.directories();
            const QSet<QString> watched(current.begin(), current.end());
            for (const auto &path : visited) {
                if (!watched.contains(path))
                    m_watcher.addPath(path);
                m_pathToFolderId[path] = cfg.id;
            }
            for (const auto &path : m_pathToFolderId.keys(cfg.id)) {
                if ((path == directory || path.startsWith(directory + '/')) && !visited.contains(path)) {
                    if (watched.contains(path))
                        m_watcher.removePath(path);
                    m_pathToFolderId.remove(path);
                }
            }
        }
    }
}

void WatchFolderService::scanDirectory(const WatchFolderConfig &cfg, QString directory) {
    const bool baseline = m_needsBaseline.contains(cfg.id);
    if (m_storageBlocked.contains(cfg.id) || (!cfg.isEnabled && !baseline))
        return;
    if (directory.isEmpty() || baseline)
        directory = cfg.folderPath;
    if (m_scanning.contains(cfg.id)) {
        m_rescan[cfg.id].insert(directory);
        return;
    }
    m_scanning.insert(cfg.id);
    const auto generation = m_generation.value(cfg.id);
    m_scanPool.start([this, cfg, directory, generation, baseline]() {
        const auto scan = collectDirectory(cfg, directory);
        QMetaObject::invokeMethod(
            this,
            [this, cfg, generation, baseline, directory, scan]() {
                m_scanning.remove(cfg.id);
                if (generation == m_generation.value(cfg.id) && hasFolder(cfg.id)) {
                    if (scan.error.isEmpty())
                        applyScan(cfg, directory, scan, baseline);
                    else
                        m_activity[cfg.id] = scan.error;
                    emit watchStatusChanged();
                }
                auto pending = m_rescan.take(cfg.id);
                if (hasFolder(cfg.id) && !pending.isEmpty()) {
                    const auto current = m_folders.value(cfg.id);
                    if (generation != m_generation.value(cfg.id) || pending.contains(current.folderPath))
                        scanDirectory(current);
                    else
                        for (const auto &path : pending)
                            scanDirectory(current, path);
                }
            },
            Qt::QueuedConnection);
    });
}

void WatchFolderService::onPollTimerTimeout() {
    if (++m_ticks % 5 == 0) {
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
        processStableCandidate(cand);
    }
}

void WatchFolderService::processStableCandidate(const PendingCandidate &candidate) {
    if (!m_orchestrator)
        return;
    if (!m_folders.contains(candidate.folderId))
        return;

    const auto cfg = m_folders[candidate.folderId];
    const QString relativePath = QDir(cfg.folderPath).relativeFilePath(candidate.filePath);
    const QJsonArray fingerprint{candidate.lastSize, candidate.lastModified};
    if (!cfg.isEnabled || m_needsBaseline.contains(cfg.id) || m_storageBlocked.contains(cfg.id) ||
        m_processedFiles.value(cfg.id).value(relativePath) == fingerprint ||
        m_activeFiles.contains(candidate.filePath))
        return;
    const QFileInfo source(candidate.filePath);
    if (!source.isFile() || source.canonicalFilePath() != candidate.filePath ||
        source.size() != candidate.lastSize ||
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
        if (preset) {
            options = preset->options;
            options.insert("presetTarget", preset->targetFormatId);
        }
    }
    auto result = JobFactory::create(candidate.filePath, cfg.targetFormatId, options,
                                     app::DestinationMode::CustomFolder, destDir);
    if (!result.job) {
        m_failedFiles[cfg.id].insert(relativePath, fingerprint);
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
        [this, candidate, cfg, jobId, completionConnection, relativePath, fingerprint](const QString &id,
                                                                                       JobState state) {
            if (id != jobId)
                return;
            disconnect(*completionConnection);
            m_activeFiles.remove(candidate.filePath);
            m_jobFolders.remove(id);
            const QFileInfo source(candidate.filePath);
            auto sourceUnchanged = [&]() {
                const QFileInfo current(candidate.filePath);
                return current.isFile() && current.canonicalFilePath() == candidate.filePath &&
                       current.size() == candidate.lastSize &&
                       current.lastModified().toMSecsSinceEpoch() == candidate.lastModified;
            };
            if (state == JobState::Completed) {
                if (sourceUnchanged() && hasFolder(cfg.id) &&
                    m_folders.value(cfg.id).folderPath == cfg.folderPath) {
                    m_processedFiles[cfg.id].insert(relativePath, fingerprint);
                    if (!saveState(cfg)) {
                        pauseWithError(cfg.id, "Cannot save conversion history; original retained.");
                        return;
                    }
                }
                const bool unchanged = sourceUnchanged();
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
                m_failedFiles[cfg.id].insert(relativePath, fingerprint);
                m_activity[cfg.id] = "Failed/cancelled: " + source.fileName() + " — use Retry failures";
            }
            emit watchStatusChanged();
        });

    m_orchestrator->addJob(job, true);
    emit jobDispatched(jobId, candidate.filePath);
}

} // namespace uc::core
