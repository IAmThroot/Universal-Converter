#include "PandocArgBuilder.h"
#include <QFileInfo>

namespace uc::engines {

QString PandocArgBuilder::mapExtensionToPandocFormat(const QString& ext) {
    const QString e = ext.toLower().remove('.');
    if (e == "md" || e == "markdown") return "markdown";
    if (e == "htm" || e == "html") return "html";
    if (e == "tex" || e == "latex") return "latex";
    if (e == "docx") return "docx";
    if (e == "odt") return "odt";
    if (e == "epub") return "epub";
    if (e == "rst") return "rst";
    if (e == "rtf") return "rtf";
    if (e == "txt") return "markdown";
    if (e == "pdf") return "pdf";
    if (e == "adoc" || e == "asciidoc") return "asciidoc";
    if (e == "org") return "org";
    if (e == "textile") return "textile";
    if (e == "wiki" || e == "mediawiki") return "mediawiki";
    if (e == "opml") return "opml";
    return e;
}

PandocArgumentResult PandocArgBuilder::buildArguments(const QString& inputPath,
                                                     const QString& outputPath,
                                                     const DocumentConversionOptions& options,
                                                     const PandocCapabilities& capabilities)
{
    PandocArgumentResult result;

    if (inputPath.isEmpty() || outputPath.isEmpty()) {
        result.errorMessage = "Input or output path is empty.";
        return result;
    }

    if (QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath()) {
        result.errorMessage = "Input and output path resolve to the exact same file.";
        return result;
    }

    const QString inExt = QFileInfo(inputPath).suffix();
    const QString outExt = QFileInfo(outputPath).suffix();

    const QString fromFormat = options.customInputFormat.isEmpty() ?
                                   mapExtensionToPandocFormat(inExt) : options.customInputFormat;
    const QString toFormat = mapExtensionToPandocFormat(outExt);

    // PDF Output Verification
    if (toFormat == "pdf") {
        if (!capabilities.hasAnyPdfEngine()) {
            result.errorMessage = "PDF conversion requires an external PDF engine (e.g. Typst, XeLaTeX, or wkhtmltopdf). None were detected on this system.";
            return result;
        }
    }

    QStringList args;

    // Explicit from format
    args << "-f" << fromFormat;

    // Explicit to format (Pandoc automatically selects PDF writer if output ends with .pdf)
    if (toFormat != "pdf") {
        args << "-t" << toFormat;
    }

    // Standalone document flag (required for HTML, LaTeX, RTF headers)
    if (options.standalone && toFormat != "docx" && toFormat != "odt" && toFormat != "epub") {
        args << "--standalone";
    }

    // Table of contents
    if (options.generateToc) {
        args << "--toc";
        if (options.tocDepth > 0) {
            args << "--toc-depth" << QString::number(options.tocDepth);
        }
    }

    // Metadata title
    if (!options.documentTitle.isEmpty()) {
        args << "--metadata" << QString("title=%1").arg(options.documentTitle);
    }

    // CSS Styling for HTML/EPUB
    if (!options.customCssPath.isEmpty() && (toFormat == "html" || toFormat == "epub")) {
        args << "--css" << options.customCssPath;
    }

    // PDF engine flag
    if (toFormat == "pdf") {
        PdfEngineInfo engineInfo;
        if (options.pdfEngine != PdfEngineType::None) {
            for (const auto& eng : capabilities.availablePdfEngines()) {
                if (eng.type == options.pdfEngine && eng.isAvailable) {
                    engineInfo = eng;
                    break;
                }
            }
        }
        if (!engineInfo.isAvailable) {
            engineInfo = capabilities.preferredPdfEngine();
        }

        if (engineInfo.isAvailable) {
            args << QString("--pdf-engine=%1").arg(engineInfo.executablePath);
        }
    }

    // Input path
    args << inputPath;

    // Output path flag
    args << "-o" << outputPath;

    result.isValid = true;
    result.arguments = args;
    return result;
}

} // namespace uc::engines
