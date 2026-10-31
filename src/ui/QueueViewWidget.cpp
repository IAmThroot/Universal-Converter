#include "QueueViewWidget.h"
#include <QHeaderView>
#include <QFileInfo>
#include <QSet>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QProgressBar>
#include <QDialog>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QMenu>
#include <QMessageBox>
#include "../core/JobFactory.h"

namespace uc::ui {

QueueViewWidget::QueueViewWidget(std::shared_ptr<core::ConversionOrchestrator> orchestrator, QWidget *parent)
    : QWidget(parent), m_orchestrator(orchestrator) {
    setupUi();
    refreshQueue();
    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &QueueViewWidget::refreshQueue);
    timer->start();

    if (m_orchestrator) {
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobAdded, this,
                &QueueViewWidget::onJobAdded);
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobStarted, this,
                &QueueViewWidget::onJobStarted);
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobProgressChanged, this,
                &QueueViewWidget::onJobProgressChanged);
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobStateChanged, this,
                &QueueViewWidget::onJobStateChanged);
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobFinished, this,
                &QueueViewWidget::onJobFinished);
        connect(m_orchestrator.get(), &core::ConversionOrchestrator::jobRemoved, this,
                [this](const QString &) { refreshQueue(); });
    }
}

void QueueViewWidget::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_treeWidget = new QTreeWidget(this);
    setObjectName("queueView");
    m_treeWidget->setHeaderLabels({"File", "Target", "Status", "Engine", "Progress", "Elapsed / ETA"});
    m_treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_treeWidget->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_treeWidget->setColumnWidth(2, 120);
    m_treeWidget->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_treeWidget->setStyleSheet("QTreeWidget {"
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
                                "}");
    m_treeWidget->setAccessibleName("Conversion queue");
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeWidget, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto *item = m_treeWidget->itemAt(pos);
        if (!item)
            return;
        auto job = m_orchestrator->findJob(item->data(0, Qt::UserRole).toString());
        if (!job)
            return;
        QMenu menu(this);
        auto *open = menu.addAction("Open Output");
        open->setEnabled(job->state() == core::JobState::Completed && QFile::exists(job->finalOutputPath()));
        auto *folder = menu.addAction("Open Folder");
        folder->setEnabled(QFileInfo(job->finalOutputPath()).dir().exists());
        auto *retry = menu.addAction("Retry");
        retry->setEnabled(job->isTerminal() && QFile::exists(job->primaryInputPath()));
        auto *cancel = menu.addAction("Cancel");
        cancel->setEnabled(!job->isTerminal());
        auto *details = menu.addAction("Details");
        auto *action = menu.exec(m_treeWidget->viewport()->mapToGlobal(pos));
        if (action == open)
            QDesktopServices::openUrl(QUrl::fromLocalFile(job->finalOutputPath()));
        if (action == folder)
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(job->finalOutputPath()).absolutePath()));
        if (action == cancel)
            m_orchestrator->cancelJob(job->id());
        if (action == retry) {
            auto &settings = app::AppSettings::instance();
            auto result =
                core::JobFactory::create(job->primaryInputPath(), job->outputFormatId(), job->options(),
                                         settings.destinationMode(), settings.customDestinationPath());
            if (result.job)
                m_orchestrator->addJob(result.job);
            else
                QMessageBox::warning(this, "Cannot retry", result.error);
        }
        if (action == details) {
            QDialog dialog(this);
            dialog.setWindowTitle("Conversion details");
            dialog.resize(760, 480);
            auto *layout = new QVBoxLayout(&dialog);
            auto *text = new QPlainTextEdit(&dialog);
            text->setReadOnly(true);
            text->setPlainText(job->primaryInputPath() + "\n→ " + job->finalOutputPath() + "\n\n" +
                               job->statusMessage() + "\n" + job->errorMessage() + "\n\n" +
                               job->executedCommand() + "\n\n" + job->diagnosticOutput());
            layout->addWidget(text);
            auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
            layout->addWidget(buttons);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            dialog.exec();
        }
    });
    layout->addWidget(m_treeWidget, 1);

    auto *bottomLayout = new QHBoxLayout();
    m_summaryLabel = new QLabel("Queue is empty", this);
    m_summaryLabel->setWordWrap(true);

    m_cancelBtn = new QPushButton("Cancel Selected", this);
    m_clearBtn = new QPushButton("Clear Finished", this);

    connect(m_cancelBtn, &QPushButton::clicked, this, &QueueViewWidget::onCancelSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &QueueViewWidget::onClearCompleted);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
        if (!item || !m_orchestrator)
            return;
        const QString id = item->data(0, Qt::UserRole).toString();
        auto job = m_orchestrator->findJob(id);
        if (job && job->state() == core::JobState::Completed) {
            const QString path = job->finalOutputPath();
            if (QFile::exists(path)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
            }
        }
    });

    bottomLayout->addWidget(m_summaryLabel, 1);
    auto* actions=new QPushButton("Actions…",this);
    actions->setToolTip("Open output, open folder, retry, or view diagnostics for the selected job");
    connect(actions,&QPushButton::clicked,this,[this](){
        auto* item=m_treeWidget->currentItem();if(!item)return;
        emit m_treeWidget->customContextMenuRequested(m_treeWidget->visualItemRect(item).center());
    });
    connect(m_treeWidget,&QTreeWidget::itemSelectionChanged,this,[this,actions](){actions->setEnabled(m_treeWidget->currentItem()!=nullptr);});
    actions->setEnabled(false);
    bottomLayout->addWidget(actions);
    bottomLayout->addWidget(m_cancelBtn);
    bottomLayout->addWidget(m_clearBtn);

    layout->addLayout(bottomLayout);
}

void QueueViewWidget::refreshQueue() {
    if (!m_orchestrator)
        return;

    // Incremental update: reuse existing rows keyed by job id instead of
    // full clear() + new QTreeWidgetItem per signal (jank at 100s of jobs).
    const auto jobs = m_orchestrator->allJobs();
    QSet<QString> liveIds;
    liveIds.reserve(jobs.size());
    for (const auto &job : jobs) {
        liveIds.insert(job->id());
    }
    // Remove stale rows.
    for (int i = m_treeWidget->topLevelItemCount() - 1; i >= 0; --i) {
        auto *item = m_treeWidget->topLevelItem(i);
        if (!liveIds.contains(item->data(0, Qt::UserRole).toString())) {
            delete m_treeWidget->takeTopLevelItem(i);
        }
    }
    QHash<QString, QTreeWidgetItem *> existing;
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        auto *item = m_treeWidget->topLevelItem(i);
        existing.insert(item->data(0, Qt::UserRole).toString(), item);
    }
    // Upsert rows in queue order.
    for (int row = 0; row < jobs.size(); ++row) {
        const auto &job = jobs.at(row);
        QTreeWidgetItem *item = nullptr;
        item = existing.value(job->id(), nullptr);
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
        item->setText(2, core::jobStateToString(job->state()));
        item->setToolTip(2, job->statusMessage());
        auto *progress = qobject_cast<QProgressBar *>(m_treeWidget->itemWidget(item, 4));
        if (!progress) {
            progress = new QProgressBar(m_treeWidget);
            m_treeWidget->setItemWidget(item, 4, progress);
        }
        progress->setRange(0, job->hasKnownProgress() || job->isTerminal() ? 100 : 0);
        progress->setValue(job->state() == core::JobState::Completed ? 100
                                                                     : qMax(0, job->progressPercentage()));
        if (job->startedAt().isValid()) {
            const auto end = job->completedAt().isValid() ? job->completedAt() : QDateTime::currentDateTime();
            QString timing = QString("%1s").arg(job->startedAt().secsTo(end));
            if (!job->isTerminal() && job->etaSeconds() >= 0)
                timing += QString(" / ~%1s remaining").arg(job->etaSeconds());
            if (!job->isTerminal() && job->speed() > 0)
                timing += QString(" · %1×").arg(job->speed(), 0, 'f', 1);
            item->setText(5, timing);
        }
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

    m_summaryLabel->setText(jobs.isEmpty() ? "Queue is empty" : m_orchestrator->summary());
    m_cancelBtn->setEnabled(!m_orchestrator->activeJobs().isEmpty() ||
                            !m_orchestrator->queuedJobs().isEmpty());
    m_clearBtn->setEnabled(!m_orchestrator->completedJobs().isEmpty());
}

static QTreeWidgetItem *findItemByJobId(QTreeWidget *tree, const QString &jobId) {
    if (!tree)
        return nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto *item = tree->topLevelItem(i);
        if (item && item->data(0, Qt::UserRole).toString() == jobId) {
            return item;
        }
    }
    return nullptr;
}

void QueueViewWidget::onJobAdded(const QString & /*jobId*/) { refreshQueue(); }

void QueueViewWidget::onJobStarted(const QString &jobId) {
    auto *item = findItemByJobId(m_treeWidget, jobId);
    if (item) {
        item->setText(2, "Running...");
        item->setForeground(2, QColor("#38bdf8"));
    }
}

void QueueViewWidget::onJobProgressChanged(const QString &jobId, int progress, const QString &statusMessage) {
    auto *item = findItemByJobId(m_treeWidget, jobId);
    if (!item)
        return;

    QString text;
    if (progress >= 0) {
        text = statusMessage.isEmpty() ? QString("%1%").arg(progress)
                                       : QString("%1% - %2").arg(progress).arg(statusMessage);
    } else {
        text = statusMessage.isEmpty() ? "Processing..." : statusMessage;
    }
    item->setText(2, core::jobStateToString(m_orchestrator->findJob(jobId)->state()));
    item->setToolTip(2, text);
    item->setForeground(2, QColor("#38bdf8"));
}

void QueueViewWidget::onJobStateChanged(const QString &jobId, core::JobState state, const QString &message) {
    auto *item = findItemByJobId(m_treeWidget, jobId);
    if (!item)
        return;

    QString text = message;
    switch (state) {
    case core::JobState::Queued:
        if (text.isEmpty())
            text = "Queued";
        item->setForeground(2, QColor("#a1a1aa"));
        break;
    case core::JobState::Preparing:
        if (text.isEmpty())
            text = "Preparing...";
        item->setForeground(2, QColor("#facc15"));
        break;
    case core::JobState::Running:
        if (text.isEmpty())
            text = "Running...";
        item->setForeground(2, QColor("#38bdf8"));
        break;
    case core::JobState::Completed:
        if (text.isEmpty())
            text = "Completed";
        item->setForeground(2, QColor("#10b981"));
        break;
    case core::JobState::Failed:
        if (text.isEmpty())
            text = "Failed";
        item->setForeground(2, QColor("#ef4444"));
        break;
    case core::JobState::Cancelled:
        if (text.isEmpty())
            text = "Cancelled";
        item->setForeground(2, QColor("#71717a"));
        break;
    }
    item->setText(2, core::jobStateToString(m_orchestrator->findJob(jobId)->state()));
    item->setToolTip(2, text);
}

void QueueViewWidget::onJobFinished(const QString & /*jobId*/, core::JobState /*state*/) { refreshQueue(); }

void QueueViewWidget::onCancelSelected() {
    if (!m_orchestrator)
        return;
    const auto items = m_treeWidget->selectedItems();
    for (auto *item : items) {
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
