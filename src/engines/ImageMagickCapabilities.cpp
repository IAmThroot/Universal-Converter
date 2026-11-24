#include "ImageMagickCapabilities.h"
#include <QProcess>
#include <QFile>
#include <QRegularExpression>
#include "../core/ProcessRunner.h"

namespace uc::engines {

ImageMagickCapabilities& ImageMagickCapabilities::instance() {
    static ImageMagickCapabilities s_instance;
    return s_instance;
}

void ImageMagickCapabilities::discover(const QString& magickExecutablePath) {
    if (magickExecutablePath.isEmpty() || !QFile::exists(magickExecutablePath)) {
        m_isDiscovered = false;
        return;
    }

    m_formats.clear();
    m_readableFormats.clear();
    m_writableFormats.clear();
    m_multiFrameFormats.clear();

    // 1. Query version
    const auto vRes = core::ProcessRunner::executeSync(magickExecutablePath, {"-version"}, 3000);
    if (vRes.isSuccess()) {
        const QString vOut = QString::fromUtf8(vRes.standardOutput);
        const auto lines = vOut.split('\n');
        if (!lines.isEmpty()) {
            m_versionString = lines.first().trimmed();
        }
        for (const auto& line : lines) {
            if (line.contains("Delegates")) {
                m_delegateInfo = line.trimmed();
                break;
            }
        }
    }

    // 2. Query formats table: magick -list format
    const auto fRes = core::ProcessRunner::executeSync(magickExecutablePath, {"-list", "format"}, 15000);
    if (fRes.isSuccess()) {
        const QString fOut = QString::fromUtf8(fRes.standardOutput);
        const auto lines = fOut.split('\n');

        // Static builds omit the Module column. Match the mode itself in either layout.
        static const QRegularExpression formatRow(
            QStringLiteral(R"(^\s*(\S+?)\*?\s+(?:\S+\s+)?([r-][w-][+-])(?:\s|$))"));
        for (const auto& line : lines) {
            const auto match = formatRow.match(line);
            if (!match.hasMatch()) continue;

            const QString fmtTag = match.captured(1).toUpper();
            const QString flags = match.captured(2);
            ImageFormatMode mode;
            mode.format = fmtTag;
            mode.canRead = flags.at(0) == 'r';
            mode.canWrite = flags.at(1) == 'w';
            mode.supportsMultiFrame = flags.at(2) == '+';

            m_formats[fmtTag] = mode;
            if (mode.canRead) m_readableFormats.insert(fmtTag);
            if (mode.canWrite) m_writableFormats.insert(fmtTag);
            if (mode.supportsMultiFrame) m_multiFrameFormats.insert(fmtTag);
        }
    }

    m_isDiscovered = !m_formats.isEmpty();
}

bool ImageMagickCapabilities::canReadFormat(const QString& formatOrExt) const {
    const QString tag = formatOrExt.toUpper().remove('.');
    if (tag == "JPG" || tag == "JPEG") {
        return m_readableFormats.contains("JPEG") || m_readableFormats.contains("JPG");
    }
    if (tag == "TIF" || tag == "TIFF") {
        return m_readableFormats.contains("TIFF") || m_readableFormats.contains("TIF");
    }
    return m_readableFormats.contains(tag);
}

bool ImageMagickCapabilities::canWriteFormat(const QString& formatOrExt) const {
    const QString tag = formatOrExt.toUpper().remove('.');
    if (tag == "JPG" || tag == "JPEG") {
        return m_writableFormats.contains("JPEG") || m_writableFormats.contains("JPG");
    }
    if (tag == "TIF" || tag == "TIFF") {
        return m_writableFormats.contains("TIFF") || m_writableFormats.contains("TIF");
    }
    return m_writableFormats.contains(tag);
}

bool ImageMagickCapabilities::supportsAnimation(const QString& formatOrExt) const {
    const QString tag = formatOrExt.toUpper().remove('.');
    return m_multiFrameFormats.contains(tag);
}

} // namespace uc::engines
