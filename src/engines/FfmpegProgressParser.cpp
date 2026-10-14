#include "FfmpegProgressParser.h"
#include <QtGlobal>

namespace uc::engines {

FfmpegProgressParser::FfmpegProgressParser(double totalDurationSeconds)
    : m_totalDuration(totalDurationSeconds)
{
}

void FfmpegProgressParser::setTotalDuration(double durationSeconds) {
    m_totalDuration = qMax(0.0, durationSeconds);
}

void FfmpegProgressParser::reset() {
    m_buffer.clear();
    m_currentRecord.clear();
}

QList<ProgressSnapshot> FfmpegProgressParser::feedChunk(const QByteArray& data) {
    QList<ProgressSnapshot> snapshots;
    m_buffer.append(data);

    int newlineIndex = -1;
    while ((newlineIndex = m_buffer.indexOf('\n')) != -1) {
        const QByteArray lineBytes = m_buffer.left(newlineIndex).trimmed();
        m_buffer.remove(0, newlineIndex + 1);

        if (lineBytes.isEmpty()) continue;

        const QString line = QString::fromUtf8(lineBytes);
        const int eqIndex = line.indexOf('=');
        if (eqIndex == -1) continue;

        const QString key = line.left(eqIndex).trimmed();
        const QString value = line.mid(eqIndex + 1).trimmed();

        m_currentRecord[key] = value;

        if (key == "progress") {
            ProgressSnapshot snap;
            snap.outTimeUs = m_currentRecord.value("out_time_us", "0").toLongLong();
            snap.outTimeSeconds = static_cast<double>(snap.outTimeUs) / 1000000.0;
            snap.frameNumber = m_currentRecord.value("frame", "0").toLongLong();
            snap.fps = m_currentRecord.value("fps", "0").toDouble();
            snap.totalSize = m_currentRecord.value("total_size", "0").toLongLong();

            QString speedStr = m_currentRecord.value("speed", "0x");
            speedStr.remove('x');
            snap.speed = speedStr.toDouble();

            snap.isProgressEnd = (value == "end");
            snapshots.append(snap);
            m_currentRecord.clear();
        }
    }

    return snapshots;
}

#include <cmath>

int FfmpegProgressParser::calculatePercentage(const ProgressSnapshot& snapshot) const {
    if (snapshot.isProgressEnd) {
        return 100;
    }

    if (m_totalDuration <= 0.0 || snapshot.outTimeSeconds <= 0.0 ||
        std::isnan(m_totalDuration) || std::isinf(m_totalDuration) ||
        std::isnan(snapshot.outTimeSeconds) || std::isinf(snapshot.outTimeSeconds)) {
        return -1; // Indeterminate
    }

    const double pct = (snapshot.outTimeSeconds / m_totalDuration) * 100.0;
    return qBound(0, static_cast<int>(pct), 99);
}

} // namespace uc::engines
