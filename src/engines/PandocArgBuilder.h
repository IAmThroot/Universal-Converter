#pragma once

#include <QString>
#include <QStringList>
#include "PandocCapabilities.h"

namespace uc::engines {

struct DocumentConversionOptions {
    QString targetFormatId;
    QString customInputFormat;   // e.g. "markdown", "docx"
    bool standalone{true};       // Generate complete self-contained file with header/body
    bool generateToc{false};     // Table of contents
    int tocDepth{3};             // Heading depth for table of contents
    QString documentTitle;
    QString customCssPath;
    PdfEngineType pdfEngine{PdfEngineType::None};
};

struct PandocArgumentResult {
    bool isValid{false};
    QStringList arguments;
    QString errorMessage;
};

class PandocArgBuilder {
public:
    static PandocArgumentResult buildArguments(const QString& inputPath,
                                               const QString& outputPath,
                                               const DocumentConversionOptions& options,
                                               const PandocCapabilities& capabilities);

    static QString mapExtensionToPandocFormat(const QString& ext);
};

} // namespace uc::engines
