#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <memory>
#include "../core/ConversionOrchestrator.h"

namespace uc::ui {

class QueueViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit QueueViewWidget(std::shared_ptr<core::ConversionOrchestrator> orchestrator, QWidget* parent = nullptr);

    void refreshQueue();

private slots:
    void onJobAdded(const QString& jobId);
    void onJobFinished(const QString& jobId, core::JobState state);
    void onCancelSelected();
    void onClearCompleted();

private:
    void setupUi();

    std::shared_ptr<core::ConversionOrchestrator> m_orchestrator;
    QTreeWidget* m_treeWidget;
    QLabel* m_summaryLabel;
    QPushButton* m_cancelBtn;
    QPushButton* m_clearBtn;
};

} // namespace uc::ui
