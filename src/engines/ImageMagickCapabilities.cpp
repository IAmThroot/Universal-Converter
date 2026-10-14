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

        for (const auto& line : lines) {
            if (line.startsWith("---") || line.contains("Format  Module") || line.trimmed().isEmpty()) {
                continue;
            }

            // Columns typically: Format (col 0-10), Module (10-20), Mode (20-26), Description
            const QString trimmed = line.trimmed();
            const auto tokens = trimmed.split(QRegularExpression("\\s+"));
            if (tokens.size() < 3) continue;

            QString fmtTag = tokens.value(0).remove('*').trimmed().toUpper();
            QString modeStr = tokens.value(2).trimmed();

            // Mode usually has 3 characters: e.g. "rw+", "r--", "rw-"
            if (modeStr.size() < 2 && tokens.size() >= 4) {
                // Shift if format description or module had odd spacing
                modeStr = tokens.value(1).trimmed();
            }

            if (modeStr.contains('r') || modeStr.contains('w')) {
                ImageFormatMode mode;
                mode.format = fmtTag;
                mode.canRead = modeStr.contains('r');
                mode.canWrite = modeStr.contains('w');
                mode.supportsMultiFrame = modeStr.contains('+');

                m_formats[fmtTag] = mode;
                if (mode.canRead) m_readableFormats.insert(fmtTag);
                if (mode.canWrite) m_writableFormats.insert(fmtTag);
                if (mode.supportsMultiFrame) m_multiFrameFormats.insert(fmtTag);
            }
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
