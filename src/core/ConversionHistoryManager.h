#pragma once

#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QList>
#include <QMutex>
#include "ConversionJob.h"

namespace uc::core {

struct HistoryRecord {
    QString id;
    QString primaryInputPath;
    QString inputFileName;
    qint64 inputFileSizeBytes{0};
    QString inputFormatId;
    QString outputFormatId;
    QString finalOutputPath;
    qint64 outputFileSizeBytes{0};
    QString engineName;
    JobState finalState{JobState::Queued};
    QString errorMessage;
    QString executedCommand;
    QString diagnosticOutput;
    QDateTime timestamp;
    qint64 durationMs{0};
    QVariantMap options;
};

class ConversionHistoryManager {
public:
    static ConversionHistoryManager& instance();

    void recordJob(const ConversionJob* job, qint64 durationMs = 0);
    QList<HistoryRecord> records() const;
    void clearHistory();
    void removeRecord(const QString& id);

    int maxRecords() const { return m_maxRecords; }
    void setMaxRecords(int count);

    void save();
    void load();

private:
    ConversionHistoryManager();
    ~ConversionHistoryManager();

    QString historyFilePath() const;

    mutable QMutex m_mutex;
    QList<HistoryRecord> m_records;
    int m_maxRecords{200};
};

} // namespace uc::core
