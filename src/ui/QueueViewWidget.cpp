#include "QueueViewWidget.h"
#include <QHeaderView>
#include <QFileInfo>
#include <QSet>
#include <QDesktopServices>
#include <QUrl>

namespace uc::ui {

QueueViewWidget::QueueViewWidget(std::shared_ptr<core::ConversionOrchestrator> orchestrator, QWidget* parent)
    : QWidget(parent)
    , m_orchestrator(orchestrator)
{
    setupUi();

    if (m_orchestrator) {
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobAdded, this, &QueueViewWidget::onJobAdded);
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobStarted, this, &QueueViewWidget::onJobStarted);
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobProgressChanged, this, &QueueViewWidget::onJobProgressChanged);
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobStateChanged, this, &QueueViewWidget::onJobStateChanged);
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobFinished, this, &QueueViewWidget::onJobFinished);
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobRemoved, this, [this](const QString&) { refreshQueue(); });
    }
}

void QueueViewWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabels({"File", "Target", "Status", "Engine"});
    m_treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_treeWidget->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_treeWidget->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_treeWidget->setStyleSheet(
        "QTreeWidget {"
        "    background-color: #121215;"
        "    border: 1px solid #27272a;"
        "    border-radius: 10px;"
        "    color: #f4f4f5;"
        "    font-size: 13px;"
        "}"
        "QHeaderView::section {"
        "    background-color: #18181b;"
        "    color: #a1a1aa;"
        "    padding: 8px 12px;"
        "    border: none;"
        "    border-bottom: 1px solid #27272a;"
        "    font-weight: 600;"
        "    font-size: 12px;"
        "}"
        "QTreeWidget::item {"
        "    padding: 8px 6px;"
        "    border-bottom: 1px solid #1a1a1e;"
        "}"
        "QTreeWidget::item:hover {"
        "    background-color: #18181b;"
        "}"
        "QTreeWidget::item:selected {"
        "    background-color: #27272a;"
        "    color: #ffffff;"
        "}"
    );
    layout->addWidget(m_treeWidget, 1);

    auto* bottomLayout = new QHBoxLayout();
    m_summaryLabel = new QLabel("Queue is empty", this);
    m_summaryLabel->setStyleSheet("color: #64748b; font-size: 12px;");

    m_cancelBtn = new QPushButton("Cancel Selected", this);
    m_clearBtn = new QPushButton("Clear Completed", this);

    connect(m_cancelBtn, &QPushButton::clicked, this, &QueueViewWidget::onCancelSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &QueueViewWidget::onClearCompleted);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item || !m_orchestrator) return;
        const QString id = item->data(0, Qt::UserRole).toString();
        auto job = m_orchestrator->findJob(id);
        if (job && job->state() == core::JobState::Completed) {
            const QString path = job->finalOutputPath();
            if (QFile::exists(path)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
            }
        }
    });

    bottomLayout->addWidget(m_summaryLabel);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_cancelBtn);
    bottomLayout->addWidget(m_clearBtn);

    layout->addLayout(bottomLayout);
}

void QueueViewWidget::refreshQueue() {
    if (!m_orchestrator) return;

    // Incremental update: reuse existing rows keyed by job id instead of
    // full clear() + new QTreeWidgetItem per signal (jank at 100s of jobs).
    const auto jobs = m_orchestrator->allJobs();
    QSet<QString> liveIds;
    liveIds.reserve(jobs.size());
    for (const auto& job : jobs) {
        liveIds.insert(job->id());
    }
    // Remove stale rows.
    for (int i = m_treeWidget->topLevelItemCount() - 1; i >= 0; --i) {
        auto* item = m_treeWidget->topLevelItem(i);
        if (!liveIds.contains(item->data(0, Qt::UserRole).toString())) {
            delete m_treeWidget->takeTopLevelItem(i);
        }
    }
    // Upsert rows in queue order.
    for (int row = 0; row < jobs.size(); ++row) {
        const auto& job = jobs.at(row);
        QTreeWidgetItem* item = nullptr;
        // Find existing row by id (linear scan is fine; rows are UI-bounded).
        for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
            auto* cand = m_treeWidget->topLevelItem(i);
            if (cand->data(0, Qt::UserRole).toString() == job->id()) {
                item = cand;
                break;
            }
        }
        if (!item) {
            item = new QTreeWidgetItem();
            item->setData(0, Qt::UserRole, job->id());
            m_treeWidget->insertTopLevelItem(row, item);
        } else {
            const int cur = m_treeWidget->indexOfTopLevelItem(item);
            if (cur != row) {
                m_treeWidget->takeTopLevelItem(cur);
                m_treeWidget->insertTopLevelItem(qMin(row, m_treeWidget->topLevelItemCount()), item);
            }
        }
        const QFileInfo fi(job->primaryInputPath());
        item->setText(0, fi.fileName());
        item->setText(1, job->outputFormatId().toUpper());
        item->setText(2, job->statusMessage());
        item->setText(3, core::engineTypeToString(job->engine()));

        if (job->state() == core::JobState::Completed) {
            item->setForeground(2, QColor("#10b981"));
        } else if (job->state() == core::JobState::Failed) {
            item->setForeground(2, QColor("#ef4444"));
        } else if (job->state() == core::JobState::Cancelled) {
            item->setForeground(2, QColor("#94a3b8"));
        } else {
            item->setForeground(2, QColor("#38bdf8"));
        }
    }

    if (jobs.isEmpty()) {
        m_summaryLabel->setText("Queue is empty");
    } else {
        const int active = m_orchestrator->activeJobCount();
        const int queued = m_orchestrator->queuedJobs().size();
        if (active > 0 || queued > 0) {
            m_summaryLabel->setText(QString("%1 job(s) in queue (%2 active, %3 waiting)")
                                        .arg(jobs.size())
                                        .arg(active)
                                        .arg(queued));
        } else {
            m_summaryLabel->setText(QString("All %1 job(s) completed").arg(jobs.size()));
        }
    }
}

static QTreeWidgetItem* findItemByJobId(QTreeWidget* tree, const QString& jobId) {
    if (!tree) return nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto* item = tree->topLevelItem(i);
        if (item && item->data(0, Qt::UserRole).toString() == jobId) {
            return item;
        }
    }
    return nullptr;
}

void QueueViewWidget::onJobAdded(const QString& /*jobId*/) {
    refreshQueue();
}

void QueueViewWidget::onJobStarted(const QString& jobId) {
    auto* item = findItemByJobId(m_treeWidget, jobId);
    if (item) {
        item->setText(2, "Running...");
        item->setForeground(2, QColor("#38bdf8"));
    }
}

void QueueViewWidget::onJobProgressChanged(const QString& jobId, int progress, const QString& statusMessage) {
    auto* item = findItemByJobId(m_treeWidget, jobId);
    if (!item) return;

    QString text;
    if (progress >= 0) {
        text = statusMessage.isEmpty() ? QString("%1%").arg(progress)
                                       : QString("%1% - %2").arg(progress).arg(statusMessage);
    } else {
        text = statusMessage.isEmpty() ? "Processing..." : statusMessage;
    }
    item->setText(2, text);
    item->setForeground(2, QColor("#38bdf8"));
}

void QueueViewWidget::onJobStateChanged(const QString& jobId, core::JobState state, const QString& message) {
    auto* item = findItemByJobId(m_treeWidget, jobId);
    if (!item) return;

    QString text = message;
    switch (state) {
        case core::JobState::Queued:
            if (text.isEmpty()) text = "Queued";
            item->setForeground(2, QColor("#a1a1aa"));
            break;
        case core::JobState::Preparing:
            if (text.isEmpty()) text = "Preparing...";
            item->setForeground(2, QColor("#facc15"));
            break;
        case core::JobState::Running:
            if (text.isEmpty()) text = "Running...";
            item->setForeground(2, QColor("#38bdf8"));
            break;
        case core::JobState::Completed:
            if (text.isEmpty()) text = "Completed";
            item->setForeground(2, QColor("#10b981"));
            break;
        case core::JobState::Failed:
            if (text.isEmpty()) text = "Failed";
            item->setForeground(2, QColor("#ef4444"));
            break;
        case core::JobState::Cancelled:
            if (text.isEmpty()) text = "Cancelled";
            item->setForeground(2, QColor("#71717a"));
            break;
    }
    item->setText(2, text);
}

void QueueViewWidget::onJobFinished(const QString& /*jobId*/, core::JobState /*state*/) {
    refreshQueue();
}

void QueueViewWidget::onCancelSelected() {
    if (!m_orchestrator) return;
    const auto items = m_treeWidget->selectedItems();
    for (auto* item : items) {
        if (item) {
            const QString jobId = item->data(0, Qt::UserRole).toString();
            m_orchestrator->cancelJob(jobId);
        }
    }
}

void QueueViewWidget::onClearCompleted() {
    if (m_orchestrator) {
        m_orchestrator->clearCompletedJobs();
    }
}

} // namespace uc::ui
