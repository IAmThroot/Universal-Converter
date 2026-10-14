#include "OutputValidator.h"
#include <QFileInfo>
#include <QFile>

namespace uc::engines {

OutputValidationResult OutputValidator::validate(const QString& ffprobePath,
                                                 const QString& filePath,
                                                 const QString& expectedFormatId,
                                                 bool isAudioOnly)
{
    OutputValidationResult result;
    Q_UNUSED(expectedFormatId);

    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        result.errorMessage = QString("Target output file was not created: %1").arg(filePath);
        return result;
    }

    result.fileSizeBytes = fi.size();
    if (result.fileSizeBytes <= 128) {
        result.errorMessage = QString("Target output file is empty or suspiciously small (%1 bytes).").arg(result.fileSizeBytes);
        return result;
    }

    // If ffprobe is not available, we cannot run deep structural checks, but file existence & size are valid
    if (ffprobePath.isEmpty() || !QFile::exists(ffprobePath)) {
        result.isValid = true;
        result.errorMessage = "Output exists and is non-empty (FFprobe was unavailable for deep structural validation).";
        return result;
    }

    const auto probe = MediaProbe::probeFile(ffprobePath, filePath);
    if (!probe.isValid) {
        result.errorMessage = QString("Produced output file failed media validation: %1").arg(probe.errorMessage);
        return result;
    }

    result.durationSeconds = probe.durationSeconds;
    result.detectedFormat = probe.formatName;

    // Check streams
    if (isAudioOnly) {
        if (!probe.hasAudio()) {
            result.errorMessage = "Output file was expected to contain audio streams, but none were detected.";
            return result;
        }
    } else {
        if (!probe.hasVideo() && !probe.hasAudio()) {
            result.errorMessage = "Output file contains neither valid audio nor video streams.";
            return result;
        }
    }

    result.isValid = true;
    return result;
}

} // namespace uc::engines
