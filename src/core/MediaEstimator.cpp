#include "MediaEstimator.h"
#include <QtMath>

namespace uc::core {

QString MediaEstimator::formatBytes(qint64 bytes) {
    if (bytes <= 0) return "0 B";
    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }
    return QString("%1 %2").arg(QString::number(size, 'f', (unitIndex == 0 ? 0 : 1)), units[unitIndex]);
}

EstimationResult MediaEstimator::estimateMediaOutput(const MediaEstimationInput& input) {
    EstimationResult result;

    const QString tgt = input.targetFormatId.toLower();

    // 1. Audio/Video estimation via duration and target bitrates
    if (input.durationSeconds > 0.05) {
        int totalBitrateKbps = 0;

        if (input.targetVideoBitrateKbps > 0) {
            totalBitrateKbps += input.targetVideoBitrateKbps;
        } else if (tgt == "mp4" || tgt == "mkv" || tgt == "mov" || tgt == "webm" || tgt == "avi") {
            // Estimate default video bitrate based on resolution
            int w = (input.targetWidth > 0) ? input.targetWidth : ((input.sourceWidth > 0) ? input.sourceWidth : 1280);
            int h = (input.targetHeight > 0) ? input.targetHeight : ((input.sourceHeight > 0) ? input.sourceHeight : 720);
            qint64 pixels = static_cast<qint64>(w) * h;
            if (pixels <= 640 * 480) {
                totalBitrateKbps += 1000;
            } else if (pixels <= 1280 * 720) {
                totalBitrateKbps += 2500;
            } else if (pixels <= 1920 * 1080) {
                totalBitrateKbps += 5000;
            } else {
                totalBitrateKbps += 14000;
            }
        }

        if (input.targetAudioBitrateKbps > 0) {
            totalBitrateKbps += input.targetAudioBitrateKbps;
        } else if (tgt == "mp3" || tgt == "m4a" || tgt == "aac" || tgt == "ogg") {
            totalBitrateKbps += 192;
        } else if (tgt == "flac" || tgt == "wav") {
            // Uncompressed/lossless ~ 800 - 1411 kbps for stereo 44.1/48kHz
            totalBitrateKbps += 900;
        } else if (totalBitrateKbps > 0) {
            // Container with audio included
            totalBitrateKbps += 128;
        }

        if (totalBitrateKbps > 0) {
            // File size in bytes = (Bitrate in kbps * 1000 / 8) * duration_seconds * container overhead (1.02)
            double bytesPerSec = (totalBitrateKbps * 1000.0) / 8.0;
            double estimatedBytes = bytesPerSec * input.durationSeconds * 1.02;
            result.hasEstimate = true;
            result.estimatedSizeBytes = static_cast<qint64>(estimatedBytes);
            result.formattedEstimatedSize = formatBytes(result.estimatedSizeBytes);
            result.explanation = QString("Estimated based on %1s duration at ~%2 kbps aggregate stream bitrate.")
                                     .arg(QString::number(input.durationSeconds, 'f', 1))
                                     .arg(totalBitrateKbps);
            return result;
        }
    }

    // 2. Image estimation via resolution scale and format compression ratio
    if (input.sourceSizeBytes > 0 && (input.sourceWidth > 0 || input.targetWidth > 0)) {
        int srcW = input.sourceWidth > 0 ? input.sourceWidth : 1920;
        int srcH = input.sourceHeight > 0 ? input.sourceHeight : 1080;
        int dstW = input.targetWidth > 0 ? input.targetWidth : srcW;
        int dstH = input.targetHeight > 0 ? input.targetHeight : srcH;
        const int quality = qBound(1, input.targetQuality, 100);

        const double scaleFactor = (static_cast<double>(dstW) * static_cast<double>(dstH)) /
                                   (static_cast<double>(srcW) * static_cast<double>(srcH));
        double formatRatio = 1.0;

        if (tgt == "webp" || tgt == "avif") {
            formatRatio = 0.55 * (quality / 85.0);
        } else if (tgt == "jpg" || tgt == "jpeg") {
            formatRatio = 0.75 * (quality / 85.0);
        } else if (tgt == "png") {
            formatRatio = 1.2;
        }

        double estBytes = static_cast<double>(input.sourceSizeBytes) * scaleFactor * formatRatio;
        result.hasEstimate = true;
        result.estimatedSizeBytes = qMax<qint64>(512, static_cast<qint64>(estBytes));
        result.formattedEstimatedSize = formatBytes(result.estimatedSizeBytes);
        result.explanation = QString("Estimated image size based on quality factor %1% and dimension scaling (%2x%3).")
                                 .arg(quality)
                                 .arg(dstW)
                                 .arg(dstH);
        return result;
    }

    // Fallback: estimate unknown
    result.hasEstimate = false;
    result.formattedEstimatedSize = "Variable";
    result.explanation = "Size depends on codec complexity and input entropy.";
    return result;
}

} // namespace uc::core
