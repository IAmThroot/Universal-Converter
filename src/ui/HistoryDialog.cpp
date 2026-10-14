#include "HistoryDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>
#include "../core/MediaEstimator.h"

namespace uc::ui {

HistoryDialog::HistoryDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Conversion History & Retry");
    resize(820, 500);
    setStyleSheet("background-color: #0f172a; color: #f8fafc; font-size: 13px;");

    setupUi();
    populateTable();
}

void HistoryDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(18, 18, 18, 18);

    auto* topLayout = new QHBoxLayout();
    auto* titleLabel = new QLabel("Recent Conversions", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #f1f5f9;");
    topLayout->addWidget(titleLabel);
    topLayout->addStretch();

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setStyleSheet("color: #94a3b8; font-size: 12px;");
    topLayout->addWidget(m_summaryLabel);
    layout->addLayout(topLayout);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(6);
    m_tableWidget->setHorizontalHeaderLabels({"Timestamp", "File Name", "Target", "Status", "Duration", "Output Size"});
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setStyleSheet(
        "QTableWidget {"
        "    background-color: #090d16;"
        "    border: 1px solid #334155;"
        "    border-radius: 6px;"
        "    gridline-color: #1e293b;"
        "    color: #f8fafc;"
        "}"
        "QHeaderView::section {"
        "    background-color: #1e293b;"
        "    color: #94a3b8;"
        "    padding: 8px;"
        "    border: none;"
        "    font-weight: 600;"
        "}"
        "QTableWidget::item:selected {"
        "    background-color: #334155;"
        "}"
    );
    layout->addWidget(m_tableWidget, 1);

    auto* btnLayout = new QHBoxLayout();
    m_clearBtn = new QPushButton("Clear History", this);
    m_clearBtn->setStyleSheet("background-color: #ef4444; color: white; padding: 6px 14px; border-radius: 6px; font-weight: 600;");
    btnLayout->addWidget(m_clearBtn);

    btnLayout->addStretch();

    m_diagBtn = new QPushButton("Diagnostics", this);
    m_diagBtn->setStyleSheet("background-color: #475569; color: white; padding: 6px 14px; border-radius: 6px;");
    btnLayout->addWidget(m_diagBtn);

    m_openBtn = new QPushButton("Open Output Folder", this);
    m_openBtn->setStyleSheet("background-color: #334155; color: white; padding: 6px 14px; border-radius: 6px;");
    btnLayout->addWidget(m_openBtn);

    m_retryBtn = new QPushButton("Retry Job", this);
    m_retryBtn->setStyleSheet("background-color: #10b981; color: #090d16; padding: 6px 18px; border-radius: 6px; font-weight: bold;");
    btnLayout->addWidget(m_retryBtn);

    layout->addLayout(btnLayout);

    connect(m_clearBtn, &QPushButton::clicked, this, &HistoryDialog::onClearHistoryClicked);
    connect(m_diagBtn, &QPushButton::clicked, this, &HistoryDialog::onCopyDiagnosticsClicked);
    connect(m_openBtn, &QPushButton::clicked, this, &HistoryDialog::onOpenOutputClicked);
    connect(m_retryBtn, &QPushButton::clicked, this, &HistoryDialog::onRetryClicked);
}

void HistoryDialog::populateTable() {
    m_records = core::ConversionHistoryManager::instance().records();
    m_tableWidget->setRowCount(m_records.size());

    m_summaryLabel->setText(QString("%1 conversion records").arg(m_records.size()));

    for (int i = 0; i < m_records.size(); ++i) {
        const auto& rec = m_records[i];

        auto* itemTime = new QTableWidgetItem(rec.timestamp.toString("yyyy-MM-dd hh:mm"));
        auto* itemName = new QTableWidgetItem(rec.inputFileName);
        auto* itemTarget = new QTableWidgetItem(rec.outputFormatId.toUpper());

        QString statusText;
        QColor statusColor;
        switch (rec.finalState) {
            case core::JobState::Completed:
                statusText = "Completed";
                statusColor = QColor("#34d399");
                break;
            case core::JobState::Failed:
                statusText = "Failed";
                statusColor = QColor("#f87171");
                break;
            case core::JobState::Cancelled:
                statusText = "Cancelled";
                statusColor = QColor("#fbbf24");
                break;
            default:
                statusText = "Unknown";
                statusColor = QColor("#94a3b8");
                break;
        }

        auto* itemStatus = new QTableWidgetItem(statusText);
        itemStatus->setForeground(statusColor);

        QString durationStr = (rec.durationMs > 0) ? QString("%1s").arg(QString::number(rec.durationMs / 1000.0, 'f', 1)) : "-";
        auto* itemDuration = new QTableWidgetItem(durationStr);

        QString sizeStr = (rec.outputFileSizeBytes > 0) ? core::MediaEstimator::formatBytes(rec.outputFileSizeBytes) : "-";
        auto* itemSize = new QTableWidgetItem(sizeStr);

        m_tableWidget->setItem(i, 0, itemTime);
        m_tableWidget->setItem(i, 1, itemName);
        m_tableWidget->setItem(i, 2, itemTarget);
        m_tableWidget->setItem(i, 3, itemStatus);
        m_tableWidget->setItem(i, 4, itemDuration);
        m_tableWidget->setItem(i, 5, itemSize);
    }

    if (m_records.size() > 0) {
        m_tableWidget->selectRow(0);
    }
}

void HistoryDialog::onClearHistoryClicked() {
    if (QMessageBox::question(this, "Clear History", "Are you sure you want to clear all conversion history?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        core::ConversionHistoryManager::instance().clearHistory();
        populateTable();
    }
}

void HistoryDialog::onOpenOutputClicked() {
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_records.size()) return;

    const auto& rec = m_records[row];
    if (!rec.finalOutputPath.isEmpty() && QFile::exists(rec.finalOutputPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(rec.finalOutputPath).absolutePath()));
    } else {
        QMessageBox::information(this, "File Not Found", "Output file no longer exists or conversion failed.");
    }
}

void HistoryDialog::onRetryClicked() {
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_records.size()) return;

    const auto& rec = m_records[row];
    if (!QFile::exists(rec.primaryInputPath)) {
        QMessageBox::warning(this, "Input Missing", "Original input file was deleted or moved.");
        return;
    }

    emit retryJobRequested(rec);
    accept();
}

void HistoryDialog::onCopyDiagnosticsClicked() {
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_records.size()) return;

    const auto& rec = m_records[row];
    QString diagText = QString("Job ID: %1\nInput: %2\nOutput Format: %3\nState: %4\nEngine: %5\nDuration: %6 ms\n")
                           .arg(rec.id, rec.primaryInputPath, rec.outputFormatId,
                                core::jobStateToString(rec.finalState), rec.engineName,
                                QString::number(rec.durationMs));

    if (!rec.executedCommand.isEmpty()) {
        diagText += QString("\nCommand Executed:\n%1\n").arg(rec.executedCommand);
    }
    if (!rec.errorMessage.isEmpty()) {
        diagText += QString("\nError Message:\n%1\n").arg(rec.errorMessage);
    }
    if (!rec.diagnosticOutput.isEmpty()) {
        diagText += QString("\nDiagnostic / Stderr Output:\n%1\n").arg(rec.diagnosticOutput);
    }

    QGuiApplication::clipboard()->setText(diagText);
    QMessageBox::information(this, "Diagnostics Copied",
                             "Job diagnostic information has been copied to your clipboard.\n\n"
                             + (rec.errorMessage.isEmpty() ? "Status: OK" : "Error: " + rec.errorMessage.left(200)));
}

} // namespace uc::ui
