#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../core/ConversionHistoryManager.h"

namespace uc::core {
    class ConversionOrchestrator;
    class WatchFolderService;
}

namespace uc::ui {

class DropZoneWidget;
class QueueViewWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onFilesDropped(const QStringList& filePaths);
    void onBrowseFilesClicked();
    void onOpenSettingsClicked();
    void onOpenHistoryClicked();
    void onRetryHistoryJob(const core::HistoryRecord& record);
    void updateToolchainStatus();

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void syncWatchFolders();

    std::shared_ptr<core::ConversionOrchestrator> m_orchestrator;
    std::shared_ptr<core::WatchFolderService> m_watchFolderService;
    DropZoneWidget* m_dropZone{nullptr};
    QueueViewWidget* m_queueView{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_toolchainStatusLabel{nullptr};
    QPushButton* m_historyBtn{nullptr};
    QPushButton* m_settingsBtn{nullptr};
};

} // namespace uc::ui
