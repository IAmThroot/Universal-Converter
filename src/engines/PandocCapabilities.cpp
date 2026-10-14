#include "PandocCapabilities.h"
#include "../app/EngineResolver.h"
#include "../core/ProcessRunner.h"
#include <QProcess>
#include <QFile>
#include <QStandardPaths>

namespace uc::engines {

PandocCapabilities& PandocCapabilities::instance() {
    static PandocCapabilities s_instance;
    return s_instance;
}

void PandocCapabilities::discover(const QString& pandocExecutablePath) {
    m_inputFormats.clear();
    m_outputFormats.clear();
    m_pdfEngines.clear();
    m_versionString.clear();

    probePdfEngines();

    if (pandocExecutablePath.isEmpty() || !QFile::exists(pandocExecutablePath)) {
        m_isDiscovered = false;
        return;
    }

    // 1. Query version: pandoc --version
    const auto vRes = core::ProcessRunner::executeSync(pandocExecutablePath, {"--version"}, 3000);
    if (vRes.isSuccess()) {
        const QString vOut = QString::fromUtf8(vRes.standardOutput);
        const auto lines = vOut.split('\n');
        if (!lines.isEmpty()) {
            m_versionString = lines.first().trimmed();
        }
    }

    // 2. Query input formats: pandoc --list-input-formats
    const auto inRes = core::ProcessRunner::executeSync(pandocExecutablePath, {"--list-input-formats"}, 3000);
    if (inRes.isSuccess()) {
        const QString inOut = QString::fromUtf8(inRes.standardOutput);
        const auto lines = inOut.split('\n', Qt::SkipEmptyParts);
        for (const auto& line : lines) {
            const QString fmt = line.trimmed().toLower();
            if (!fmt.isEmpty()) m_inputFormats.insert(fmt);
        }
    }

    // 3. Query output formats: pandoc --list-output-formats
    const auto outRes = core::ProcessRunner::executeSync(pandocExecutablePath, {"--list-output-formats"}, 3000);
    if (outRes.isSuccess()) {
        const QString outOut = QString::fromUtf8(outRes.standardOutput);
        const auto lines = outOut.split('\n', Qt::SkipEmptyParts);
        for (const auto& line : lines) {
            const QString fmt = line.trimmed().toLower();
            if (!fmt.isEmpty()) m_outputFormats.insert(fmt);
        }
    }

    m_isDiscovered = !m_inputFormats.isEmpty() && !m_outputFormats.isEmpty();
}

void PandocCapabilities::probePdfEngines() {
    m_pdfEngines.clear();

    auto probeOne = [this](PdfEngineType type, const QString& name, const QString& binaryName) {
        PdfEngineInfo info;
        info.type = type;
        info.name = name;

        // Use EngineResolver to resolve bundled engines (e.g. engines/typst/typst) as well as system PATH
        auto resolved = ::uc::app::EngineResolver::resolve(binaryName, QString(), "--version");
        if (resolved.isAvailable) {
            info.executablePath = resolved.executablePath;
            info.isAvailable = true;
        }
        m_pdfEngines.append(info);
    };

    probeOne(PdfEngineType::Typst, "Typst", "typst");
    probeOne(PdfEngineType::XeLaTeX, "XeLaTeX", "xelatex");
    probeOne(PdfEngineType::PdfLaTeX, "PdfLaTeX", "pdflatex");
    probeOne(PdfEngineType::LuaLaTeX, "LuaLaTeX", "lualatex");
    probeOne(PdfEngineType::WeasyPrint, "WeasyPrint", "weasyprint");
    probeOne(PdfEngineType::WkHtmlToPdf, "wkhtmltopdf", "wkhtmltopdf");
}

bool PandocCapabilities::hasAnyPdfEngine() const {
    for (const auto& eng : m_pdfEngines) {
        if (eng.isAvailable) return true;
    }
    return false;
}

PdfEngineInfo PandocCapabilities::preferredPdfEngine() const {
    for (const auto& eng : m_pdfEngines) {
        if (eng.isAvailable) return eng;
    }
    PdfEngineInfo none;
    none.type = PdfEngineType::None;
    none.name = "None";
    return none;
}

bool PandocCapabilities::canReadFormat(const QString& formatOrExt) const {
    const QString f = formatOrExt.toLower().remove('.');
    if (f == "md" || f == "markdown" || f == "txt") return m_inputFormats.contains("markdown");
    if (f == "tex") return m_inputFormats.contains("latex");
    if (f == "htm") return m_inputFormats.contains("html");
    if (f == "adoc") return m_inputFormats.contains("asciidoc");
    if (f == "wiki") return m_inputFormats.contains("mediawiki");
    return m_inputFormats.contains(f);
}

bool PandocCapabilities::canWriteFormat(const QString& formatOrExt) const {
    const QString f = formatOrExt.toLower().remove('.');
    if (f == "md" || f == "markdown") return m_outputFormats.contains("markdown");
    if (f == "tex") return m_outputFormats.contains("latex");
    if (f == "htm") return m_outputFormats.contains("html");
    if (f == "adoc") return m_outputFormats.contains("asciidoc");
    if (f == "wiki") return m_outputFormats.contains("mediawiki");
    if (f == "pdf") return m_outputFormats.contains("pdf") && hasAnyPdfEngine();
    return m_outputFormats.contains(f);
}

} // namespace uc::engines
