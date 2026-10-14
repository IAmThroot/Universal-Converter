#pragma once

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QMap>

namespace uc::engines {

struct ProgressSnapshot {
    qint64 outTimeUs{0};       // out_time_us from FFmpeg
    double outTimeSeconds{0.0};
    qint64 totalSize{0};
    qint64 frameNumber{0};
    double fps{0.0};
    double speed{0.0};
    bool isProgressEnd{false};
};

class FfmpegProgressParser {
public:
    explicit FfmpegProgressParser(double totalDurationSeconds = 0.0);

    void setTotalDuration(double durationSeconds);
    double totalDuration() const { return m_totalDuration; }

    // Feeds incoming standard output / standard error data chunks
    QList<ProgressSnapshot> feedChunk(const QByteArray& data);

    int calculatePercentage(const ProgressSnapshot& snapshot) const;

    void reset();

private:
    double m_totalDuration{0.0};
    QByteArray m_buffer;
    QMap<QString, QString> m_currentRecord;
};

} // namespace uc::engines
