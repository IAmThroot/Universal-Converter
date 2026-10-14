#pragma once

#include <QString>

namespace uc::engines {

struct DocumentValidationResult {
    bool isValid{false};
    qint64 fileSizeBytes{0};
    QString detectedType;
    QString errorMessage;
};

class DocumentValidator {
public:
    static DocumentValidationResult validate(const QString& filePath, const QString& expectedFormatId);
};

} // namespace uc::engines
