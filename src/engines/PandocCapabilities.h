#pragma once

#include <QString>
#include <QStringList>
#include <QSet>

namespace uc::engines {

enum class PdfEngineType {
    None,
    Typst,
    XeLaTeX,
    PdfLaTeX,
    LuaLaTeX,
    WeasyPrint,
    WkHtmlToPdf
};

struct PdfEngineInfo {
    PdfEngineType type{PdfEngineType::None};
    QString name;
    QString executablePath;
    bool isAvailable{false};
};

class PandocCapabilities {
public:
    static PandocCapabilities& instance();

    bool isDiscovered() const { return m_isDiscovered; }
    void discover(const QString& pandocExecutablePath);

    bool canReadFormat(const QString& formatOrExt) const;
    bool canWriteFormat(const QString& formatOrExt) const;

    int inputFormatCount() const { return m_inputFormats.size(); }
    int outputFormatCount() const { return m_outputFormats.size(); }

    QString versionString() const { return m_versionString; }

    // PDF engine detection
    QList<PdfEngineInfo> availablePdfEngines() const { return m_pdfEngines; }
    bool hasAnyPdfEngine() const;
    PdfEngineInfo preferredPdfEngine() const;

    PandocCapabilities() = default;

private:
    void probePdfEngines();

    bool m_isDiscovered{false};
    QString m_versionString;

    QSet<QString> m_inputFormats;
    QSet<QString> m_outputFormats;
    QList<PdfEngineInfo> m_pdfEngines;
};

} // namespace uc::engines
