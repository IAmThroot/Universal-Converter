#include "ConversionHistoryManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace uc::core {

ConversionHistoryManager& ConversionHistoryManager::instance() {
    static ConversionHistoryManager s_instance;
    return s_instance;
}

ConversionHistoryManager::ConversionHistoryManager() {
    load();
}

ConversionHistoryManager::~ConversionHistoryManager() {
    save();
}

QString ConversionHistoryManager::historyFilePath() const {
    static QString s_cached;
    if (!s_cached.isEmpty()) return s_cached;
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        QDir().mkpath(appData);
        s_cached = QDir(appData).filePath("conversion_history.json");
    } else {
        s_cached = QDir::current().filePath("conversion_history.json");
    }
    return s_cached;
}

void ConversionHistoryManager::recordJob(const ConversionJob* job, qint64 durationMs) {
    if (!job) return;

    QMutexLocker locker(&m_mutex);

    HistoryRecord record;
    record.id = job->id();
    record.primaryInputPath = job->primaryInputPath();
    record.inputFileName = QFileInfo(record.primaryInputPath).fileName();
    record.inputFileSizeBytes = QFileInfo(record.primaryInputPath).size();
    record.inputFormatId = job->inputFormatId();
    record.outputFormatId = job->outputFormatId();
    record.finalOutputPath = job->finalOutputPath();
    record.outputFileSizeBytes = QFileInfo(record.finalOutputPath).size();
    
    switch (job->engine()) {
        case EngineType::FFmpeg: record.engineName = "FFmpeg"; break;
        case EngineType::ImageMagick: record.engineName = "ImageMagick"; break;
        case EngineType::Pandoc: record.engineName = "Pandoc"; break;
        default: record.engineName = "Unknown"; break;
    }

    record.finalState = job->state();
    record.errorMessage = job->errorMessage();
    record.executedCommand = job->executedCommand();
    record.diagnosticOutput = job->diagnosticOutput();
    record.timestamp = job->completedAt().isValid() ? job->completedAt() : QDateTime::currentDateTime();
    record.durationMs = durationMs;
    record.options = job->options();

    // Prepend (newest first)
    m_records.prepend(record);

    // Enforce retention limit
    while (m_records.size() > m_maxRecords) {
        m_records.removeLast();
    }

    locker.unlock();
    save();
}

QList<HistoryRecord> ConversionHistoryManager::records() const {
    QMutexLocker locker(&m_mutex);
    return m_records;
}

void ConversionHistoryManager::clearHistory() {
    {
        QMutexLocker locker(&m_mutex);
        m_records.clear();
    }
    save();
}

void ConversionHistoryManager::removeRecord(const QString& id) {
    {
        QMutexLocker locker(&m_mutex);
        for (int i = 0; i < m_records.size(); ++i) {
            if (m_records[i].id == id) {
                m_records.removeAt(i);
                break;
            }
        }
    }
    save();
}

void ConversionHistoryManager::setMaxRecords(int count) {
    QMutexLocker locker(&m_mutex);
    m_maxRecords = qMax(10, count);
    while (m_records.size() > m_maxRecords) {
        m_records.removeLast();
    }
}

void ConversionHistoryManager::save() {
    QMutexLocker locker(&m_mutex);
    QJsonArray array;
    for (const auto& rec : m_records) {
        QJsonObject obj;
        obj["id"] = rec.id;
        obj["primaryInputPath"] = rec.primaryInputPath;
        obj["inputFileName"] = rec.inputFileName;
        obj["inputFileSizeBytes"] = rec.inputFileSizeBytes;
        obj["inputFormatId"] = rec.inputFormatId;
        obj["outputFormatId"] = rec.outputFormatId;
        obj["finalOutputPath"] = rec.finalOutputPath;
        obj["outputFileSizeBytes"] = rec.outputFileSizeBytes;
        obj["engineName"] = rec.engineName;
        obj["finalState"] = static_cast<int>(rec.finalState);
        obj["errorMessage"] = rec.errorMessage;
        obj["executedCommand"] = rec.executedCommand;
        obj["diagnosticOutput"] = rec.diagnosticOutput;
        obj["timestamp"] = rec.timestamp.toString(Qt::ISODate);
        obj["durationMs"] = rec.durationMs;
        obj["options"] = QJsonObject::fromVariantMap(rec.options);
        array.append(obj);
    }

    QFile file(historyFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // Compact JSON: full Indented rewrite on every job is O(history)
        // and measurably slower on the GUI thread; compact halves I/O.
        file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    }
}

void ConversionHistoryManager::load() {
    QMutexLocker locker(&m_mutex);
    m_records.clear();

    QFile file(historyFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QByteArray data = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;

    const QJsonArray array = doc.array();
    for (const auto& val : array) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();

        HistoryRecord rec;
        rec.id = obj["id"].toString();
        rec.primaryInputPath = obj["primaryInputPath"].toString();
        rec.inputFileName = obj["inputFileName"].toString();
        rec.inputFileSizeBytes = obj["inputFileSizeBytes"].toInteger();
        rec.inputFormatId = obj["inputFormatId"].toString();
        rec.outputFormatId = obj["outputFormatId"].toString();
        rec.finalOutputPath = obj["finalOutputPath"].toString();
        rec.outputFileSizeBytes = obj["outputFileSizeBytes"].toInteger();
        rec.engineName = obj["engineName"].toString();
        const int stateInt = obj["finalState"].toInt(-1);
        if (stateInt < static_cast<int>(JobState::Queued) ||
            stateInt > static_cast<int>(JobState::Cancelled)) {
            continue; // Skip corrupt record rather than propagating invalid enum.
        }
        rec.finalState = static_cast<JobState>(stateInt);
        rec.errorMessage = obj["errorMessage"].toString();
        rec.executedCommand = obj["executedCommand"].toString();
        rec.diagnosticOutput = obj["diagnosticOutput"].toString();
        rec.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        if (!rec.timestamp.isValid()) {
            rec.timestamp = QDateTime::currentDateTime();
        }
        rec.durationMs = obj["durationMs"].toInteger();
        rec.options = obj["options"].toObject().toVariantMap();

        m_records.append(rec);
    }
}

} // namespace uc::core
