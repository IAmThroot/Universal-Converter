#pragma once

#include <QString>
#include "../engines/MediaProbe.h"
#include "../engines/ImageInspector.h"

namespace uc::core {

struct MediaEstimationInput {
    QString filePath;
    qint64 sourceSizeBytes{0};
    double durationSeconds{0.0};
    int sourceWidth{0};
    int sourceHeight{0};
    int sourceBitrateKbps{0};
    QString sourceFormat;

    // Conversion target options
    QString targetFormatId;
    int targetWidth{0};
    int targetHeight{0};
    int targetVideoBitrateKbps{0};
    int targetAudioBitrateKbps{0};
    int targetQuality{85}; // Image quality 1-100
};

struct EstimationResult {
    bool hasEstimate{false};
    qint64 estimatedSizeBytes{0};
    QString formattedEstimatedSize;
    QString explanation;
};

class MediaEstimator {
public:
    static EstimationResult estimateMediaOutput(const MediaEstimationInput& input);
    static QString formatBytes(qint64 bytes);
};

} // namespace uc::core
