#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QThreadPool>
#include <memory>
#include "WatchFolderConfig.h"
#include "ConversionJob.h"

namespace uc::core {

class ConversionOrchestrator;
struct DirectoryScan;

class WatchFolderService : public QObject {
    Q_OBJECT

  public:
    explicit WatchFolderService(ConversionOrchestrator *orchestrator, QObject *parent = nullptr);
    ~WatchFolderService() override;

    bool addFolder(const WatchFolderConfig &config, bool restored = false);
    bool updateFolder(const WatchFolderConfig &config);
    void removeFolder(const QString &folderId);
    void setFolderEnabled(const QString &folderId, bool enabled);

    QList<WatchFolderConfig> configuredFolders() const;
    WatchFolderConfig folderConfig(const QString &folderId) const;
    bool hasFolder(const QString &folderId) const;

    void scanAllNow();
    void clearHistory();
    void retryFailures();
    int pendingCount(const QString &id) const;
    QString activity(const QString &id) const { return m_activity.value(id); }
    QString lastError() const { return m_lastError; }

  signals:
    void fileDetected(const QString &filePath, const QString &folderId);
    void jobDispatched(const QString &jobId, const QString &filePath);
    void watchStatusChanged();

  private slots:
    void onDirectoryChanged(const QString &path);
    void onPollTimerTimeout();

  private:
    struct PendingCandidate {
        QString filePath;
        QString folderId;
        qint64 lastSize{0};
        qint64 lastModified{0};
        int stableCount{0};
        int zeroSizeCount{0};
    };

    void scanDirectory(const WatchFolderConfig &cfg, QString directory = {});
    void applyScan(const WatchFolderConfig &cfg, const QString &directory, const DirectoryScan &scan,
                   bool baseline);
    bool saveState(const WatchFolderConfig &cfg);
    void pauseWithError(const QString &id, const QString &message);
    void processStableCandidate(const PendingCandidate &candidate);

    void clearPending(const QString &id);
    void unwatch(const QString &id);
    QHash<QString, QString> m_activeFiles;
    QHash<QString, QString> m_jobFolders;
    QHash<QString, QHash<QString, QJsonArray>> m_failedFiles;
    QHash<QString, QString> m_activity;
    QString m_lastError;
    int m_ticks{0};
    ConversionOrchestrator *m_orchestrator{nullptr};
    QFileSystemWatcher m_watcher;
    QTimer m_pollTimer;

    QMap<QString, WatchFolderConfig> m_folders;          // keyed by config id
    QMap<QString, QString> m_pathToFolderId;             // watched directory path -> folder id
    QMap<QString, PendingCandidate> m_pendingCandidates; // filePath -> PendingCandidate
    QHash<QString, QJsonObject> m_processedFiles;
    QSet<QString> m_needsBaseline;
    QSet<QString> m_storageBlocked;
    QSet<QString> m_scanning;
    QHash<QString, QSet<QString>> m_rescan;
    QHash<QString, quint64> m_generation;
    QThreadPool m_scanPool;
};

} // namespace uc::core
