#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMap>
#include <QSet>
#include <memory>
#include "WatchFolderConfig.h"
#include "ConversionJob.h"

namespace uc::core {

class ConversionOrchestrator;

class WatchFolderService : public QObject {
    Q_OBJECT

public:
    explicit WatchFolderService(ConversionOrchestrator* orchestrator, QObject* parent = nullptr);
    ~WatchFolderService() override;

    void addFolder(const WatchFolderConfig& config);
    void updateFolder(const WatchFolderConfig& config);
    void removeFolder(const QString& folderId);
    void setFolderEnabled(const QString& folderId, bool enabled);

    QList<WatchFolderConfig> configuredFolders() const;
    WatchFolderConfig folderConfig(const QString& folderId) const;

    void scanAllNow();
    void clearHistory();

signals:
    void fileDetected(const QString& filePath, const QString& folderId);
    void jobDispatched(const QString& jobId, const QString& filePath);
    void watchStatusChanged();

private slots:
    void onDirectoryChanged(const QString& path);
    void onPollTimerTimeout();

private:
    struct PendingCandidate {
        QString filePath;
        QString folderId;
        qint64 lastSize{0};
        int stableCount{0};
    };

    void scanDirectory(const WatchFolderConfig& cfg);
    void processStableCandidate(const PendingCandidate& candidate);

    ConversionOrchestrator* m_orchestrator{nullptr};
    QFileSystemWatcher m_watcher;
    QTimer m_pollTimer;

    QMap<QString, WatchFolderConfig> m_folders;         // keyed by config id
    QMap<QString, QString> m_pathToFolderId;            // watched directory path -> folder id
    QMap<QString, PendingCandidate> m_pendingCandidates;// filePath -> PendingCandidate
    QSet<QString> m_processedFiles;                     // Avoid duplicate processing of same files
};

} // namespace uc::core
