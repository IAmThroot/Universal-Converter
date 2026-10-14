#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <optional>

namespace uc::engines {

struct MediaStreamInfo {
    int index{-1};
    QString codecType;     // "video", "audio", "subtitle"
    QString codecName;     // "h264", "aac", etc.
    QString profile;

    // Video specific
    int width{0};
    int height{0};
    double frameRate{0.0};
    QString pixFmt;

    // Audio specific
    int sampleRate{0};
    int channels{0};
    QString channelLayout;

    double durationSeconds{0.0};
    qint64 bitRate{0};
};

struct MediaProbeResult {
    bool isValid{false};
    QString filePath;
    QString formatName;     // e.g. "mov,mp4,m4a,3gp,3g2,mj2"
    double durationSeconds{0.0};
    qint64 sizeBytes{0};
    qint64 bitRate{0};

    QList<MediaStreamInfo> streams;
    QString errorMessage;

    bool hasVideo() const {
        for (const auto& s : streams) if (s.codecType == "video") return true;
        return false;
    }

    bool hasAudio() const {
        for (const auto& s : streams) if (s.codecType == "audio") return true;
        return false;
    }

    std::optional<MediaStreamInfo> primaryVideoStream() const {
        for (const auto& s : streams) if (s.codecType == "video") return s;
        return std::nullopt;
    }

    std::optional<MediaStreamInfo> primaryAudioStream() const {
        for (const auto& s : streams) if (s.codecType == "audio") return s;
        return std::nullopt;
    }
};

class MediaProbe {
public:
    static MediaProbeResult probeFile(const QString& ffprobePath, const QString& filePath, int timeoutMs = 5000);
    static MediaProbeResult parseJsonOutput(const QByteArray& jsonData, const QString& filePath);
};

} // namespace uc::engines
