#pragma once

#include <QString>
#include "MediaProbe.h"

namespace uc::engines {

struct OutputValidationResult {
    bool isValid{false};
    qint64 fileSizeBytes{0};
    double durationSeconds{0.0};
    QString detectedFormat;
    QString errorMessage;
};

class OutputValidator {
public:
    static OutputValidationResult validate(const QString& ffprobePath,
                                           const QString& filePath,
                                           const QString& expectedFormatId,
                                           bool isAudioOnly = false);
};

} // namespace uc::engines
