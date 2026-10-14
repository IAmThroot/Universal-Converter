#include "MediaProbe.h"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include "../core/ProcessRunner.h"

namespace uc::engines {

MediaProbeResult MediaProbe::probeFile(const QString& ffprobePath, const QString& filePath, int timeoutMs) {
    MediaProbeResult result;
    result.filePath = filePath;

    if (ffprobePath.isEmpty() || !QFile::exists(ffprobePath)) {
        result.errorMessage = "FFprobe executable is not available or path is invalid.";
        return result;
    }

    if (!QFile::exists(filePath)) {
        result.errorMessage = QString("File to probe does not exist: %1").arg(filePath);
        return result;
    }

    const QStringList args = {
        "-v", "quiet",
        "-print_format", "json",
        "-show_format",
        "-show_streams",
        filePath
    };

    const auto res = core::ProcessRunner::executeSync(ffprobePath, args, timeoutMs);
    if (!res.isSuccess()) {
        result.errorMessage = QString("FFprobe failed to inspect file (exit code %1): %2")
                                  .arg(res.exitCode)
                                  .arg(res.errorMessage.isEmpty() ? "Unknown format or corrupted input" : res.errorMessage);
        return result;
    }

    return parseJsonOutput(res.standardOutput, filePath);
}

MediaProbeResult MediaProbe::parseJsonOutput(const QByteArray& jsonData, const QString& filePath) {
    MediaProbeResult result;
    result.filePath = filePath;

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        result.errorMessage = QString("Failed to parse FFprobe JSON output: %1").arg(parseErr.errorString());
        return result;
    }

    const QJsonObject root = doc.object();

    // Parse format info
    if (root.contains("format") && root["format"].isObject()) {
        const QJsonObject fmtObj = root["format"].toObject();
        result.formatName = fmtObj.value("format_name").toString();
        result.durationSeconds = fmtObj.value("duration").toString().toDouble();
        result.sizeBytes = fmtObj.value("size").toString().toLongLong();
        result.bitRate = fmtObj.value("bit_rate").toString().toLongLong();
    }

    // Parse stream info
    if (root.contains("streams") && root["streams"].isArray()) {
        const QJsonArray streamsArr = root["streams"].toArray();
        for (const auto& item : streamsArr) {
            if (!item.isObject()) continue;
            const QJsonObject stObj = item.toObject();

            MediaStreamInfo stream;
            stream.index = stObj.value("index").toInt();
            stream.codecType = stObj.value("codec_type").toString();
            stream.codecName = stObj.value("codec_name").toString();
            stream.profile = stObj.value("profile").toString();

            if (stream.codecType == "video") {
                stream.width = stObj.value("width").toInt();
                stream.height = stObj.value("height").toInt();
                stream.pixFmt = stObj.value("pix_fmt").toString();

                const QString rFrameRate = stObj.value("r_frame_rate").toString();
                if (!rFrameRate.isEmpty() && rFrameRate.contains('/')) {
                    const auto parts = rFrameRate.split('/');
                    const double num = parts.value(0).toDouble();
                    const double den = parts.value(1).toDouble();
                    if (den > 0.0) {
                        stream.frameRate = num / den;
                    }
                }
            } else if (stream.codecType == "audio") {
                stream.sampleRate = stObj.value("sample_rate").toString().toInt();
                stream.channels = stObj.value("channels").toInt();
                stream.channelLayout = stObj.value("channel_layout").toString();
            }

            stream.durationSeconds = stObj.value("duration").toString().toDouble();
            if (stream.durationSeconds <= 0.0 && result.durationSeconds > 0.0) {
                stream.durationSeconds = result.durationSeconds;
            }
            stream.bitRate = stObj.value("bit_rate").toString().toLongLong();

            result.streams.append(stream);
        }
    }

    result.isValid = true;
    return result;
}

} // namespace uc::engines
