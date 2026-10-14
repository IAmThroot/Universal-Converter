#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include "../core/ConversionHistoryManager.h"

namespace uc::ui {

class HistoryDialog : public QDialog {
    Q_OBJECT

public:
    explicit HistoryDialog(QWidget* parent = nullptr);

signals:
    void retryJobRequested(const core::HistoryRecord& record);

private slots:
    void onRetryClicked();
    void onClearHistoryClicked();
    void onOpenOutputClicked();
    void onCopyDiagnosticsClicked();

private:
    void setupUi();
    void populateTable();

    QTableWidget* m_tableWidget{nullptr};
    QPushButton* m_retryBtn{nullptr};
    QPushButton* m_openBtn{nullptr};
    QPushButton* m_diagBtn{nullptr};
    QPushButton* m_clearBtn{nullptr};
    QLabel* m_summaryLabel{nullptr};

    QList<core::HistoryRecord> m_records;
};

} // namespace uc::ui
