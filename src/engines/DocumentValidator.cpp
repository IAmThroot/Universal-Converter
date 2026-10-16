#include "DocumentValidator.h"
#include <QFileInfo>
#include <QFile>
#include <QSet>
#include <QDataStream>

namespace uc::engines {

namespace {

// Lightweight, standard cross-platform ZIP parser without private Qt headers
bool parseZipEntries(QFile& file, QSet<QString>& outEntries, QString& outError) {
    if (!file.isOpen() && !file.open(QFile::ReadOnly)) {
        outError = "Could not open archive file.";
        return false;
    }

    const qint64 fileSize = file.size();
    if (fileSize < 22) {
        outError = "Archive size is too small for a valid ZIP.";
        return false;
    }

    // Search for End of Central Directory (EOCD) signature: 0x06054b50 ("PK\x05\x06")
    // EOCD record is at least 22 bytes long and at most 22 + 65535 bytes
    const qint64 maxSearch = qMin<qint64>(fileSize, 65535 + 22);
    if (!file.seek(fileSize - maxSearch)) {
        outError = "Seek failed during EOCD search.";
        return false;
    }

    const QByteArray buffer = file.read(maxSearch);
    const QByteArray eocdSig = "PK\x05\x06";
    const int eocdOffsetInBuffer = buffer.lastIndexOf(eocdSig);
    if (eocdOffsetInBuffer == -1 || eocdOffsetInBuffer + 22 > buffer.size()) {
        outError = "Corrupt or truncated archive (missing End of Central Directory record).";
        return false;
    }

    // Read EOCD record values (Little Endian)
    const char* eocdPtr = buffer.constData() + eocdOffsetInBuffer;
    quint16 totalEntries = 0;
    quint32 cdSize = 0, cdOffset = 0;
    std::memcpy(&totalEntries, eocdPtr + 10, sizeof(quint16));
    std::memcpy(&cdSize, eocdPtr + 12, sizeof(quint32));
    std::memcpy(&cdOffset, eocdPtr + 16, sizeof(quint32));

    if (totalEntries == 0) {
        outError = "Archive contains 0 entries.";
        return false;
    }

    if (static_cast<qint64>(cdOffset) + cdSize > fileSize) {
        outError = "Corrupt archive (central directory offset out of bounds).";
        return false;
    }

    if (!file.seek(cdOffset)) {
        outError = "Failed to seek to central directory.";
        return false;
    }

    // Iterate through Central Directory Headers
    for (quint16 i = 0; i < totalEntries; ++i) {
        char cdHeader[46];
        if (file.read(cdHeader, 46) != 46 || std::memcmp(cdHeader, "PK\x01\x02", 4) != 0) {
            outError = "Corrupt central directory file header.";
            return false;
        }

        quint16 nameLen = 0, extraLen = 0, commentLen = 0;
        std::memcpy(&nameLen, cdHeader + 28, sizeof(quint16));
        std::memcpy(&extraLen, cdHeader + 30, sizeof(quint16));
        std::memcpy(&commentLen, cdHeader + 32, sizeof(quint16));

        const QByteArray nameBytes = file.read(nameLen);
        if (nameBytes.size() != nameLen) {
            outError = "Unexpected end of file reading entry name.";
            return false;
        }

        outEntries.insert(QString::fromUtf8(nameBytes));

        // Skip extra field + comment
        if (extraLen + commentLen > 0) {
            if (!file.seek(file.pos() + extraLen + commentLen)) {
                outError = "Seek error during directory parsing.";
                return false;
            }
        }
    }

    return true;
}

} // namespace

DocumentValidationResult DocumentValidator::validate(const QString& filePath, const QString& expectedFormatId) {
    DocumentValidationResult result;

    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        result.errorMessage = QString("Output file does not exist: %1").arg(filePath);
        return result;
    }

    result.fileSizeBytes = fi.size();
    if (result.fileSizeBytes <= 4) {
        result.errorMessage = QString("Output document is empty or corrupt (%1 bytes).").arg(result.fileSizeBytes);
        return result;
    }

    QFile file(filePath);
    if (!file.open(QFile::ReadOnly)) {
        result.errorMessage = QString("Could not open output document for validation: %1").arg(filePath);
        return result;
    }

    const QByteArray header = file.read(16);

    const QString fmt = expectedFormatId.toLower();

    // 1. ZIP-based container validation (DOCX, ODT, EPUB)
    if (fmt == "docx" || fmt == "odt" || fmt == "epub") {
        // Must start with PK\x03\x04
        if (header.size() < 4 || header[0] != 'P' || header[1] != 'K' || header[2] != 0x03 || header[3] != 0x04) {
            file.close();
            result.errorMessage = QString("Output %1 is not a valid ZIP container (magic bytes missing).").arg(fmt.toUpper());
            return result;
        }

        // Realistic non-empty document packages have headers and internal entries
        if (result.fileSizeBytes < 100) {
            file.close();
            result.errorMessage = QString("Output %1 container is abnormally small (%2 bytes) for a valid archive.")
                                  .arg(fmt.toUpper()).arg(result.fileSizeBytes);
            return result;
        }

        QSet<QString> entryNames;
        QString parseError;
        if (!parseZipEntries(file, entryNames, parseError)) {
            file.close();
            result.errorMessage = QString("Output %1 is a corrupt, truncated, or empty ZIP archive (%2).")
                                      .arg(fmt.toUpper(), parseError);
            return result;
        }
        file.close();

        // Strict format-specific entry requirements:
        if (fmt == "docx") {
            // DOCX requires [Content_Types].xml and word/document.xml
            if (!entryNames.contains("[Content_Types].xml") && !entryNames.contains("%5BContent_Types%5D.xml")) {
                result.errorMessage = "DOCX archive is missing required [Content_Types].xml entry.";
                return result;
            }
            if (!entryNames.contains("word/document.xml")) {
                result.errorMessage = "DOCX archive is missing required word/document.xml entry.";
                return result;
            }
        } else if (fmt == "odt") {
            // ODT requires mimetype and content.xml
            if (!entryNames.contains("mimetype")) {
                result.errorMessage = "ODT archive is missing required 'mimetype' entry.";
                return result;
            }
            if (!entryNames.contains("content.xml")) {
                result.errorMessage = "ODT archive is missing required 'content.xml' entry.";
                return result;
            }
        } else if (fmt == "epub") {
            // EPUB requires mimetype and META-INF/container.xml
            if (!entryNames.contains("mimetype")) {
                result.errorMessage = "EPUB archive is missing required 'mimetype' entry.";
                return result;
            }
            if (!entryNames.contains("META-INF/container.xml")) {
                result.errorMessage = "EPUB archive is missing required 'META-INF/container.xml' entry.";
                return result;
            }
        }

        result.detectedType = "zip-package/" + fmt;
    }

    // 2. PDF validation
    else if (fmt == "pdf") {
        // Must start with %PDF-
        if (!header.startsWith("%PDF-")) {
            result.errorMessage = "Output file does not contain a valid %PDF header signature.";
            return result;
        }
        if (result.fileSizeBytes < 50) {
            result.errorMessage = "PDF output file size is abnormally small.";
            return result;
        }

        // Read tail of PDF to check for end-of-file trailer markers (%%EOF)
        QFile pdfFile(filePath);
        if (pdfFile.open(QFile::ReadOnly)) {
            const qint64 tailSize = qMin(static_cast<qint64>(1024), result.fileSizeBytes);
            if (!pdfFile.seek(result.fileSizeBytes - tailSize)) {
                pdfFile.close();
                file.close();
                result.errorMessage = "PDF validation failed: could not seek to trailer (file truncated or modified during read).";
                return result;
            }
            const QByteArray tail = pdfFile.read(tailSize);
            if (!tail.contains("%%EOF")) {
                pdfFile.close();
                file.close();
                result.errorMessage = "PDF file is truncated or damaged (missing %%EOF trailer marker).";
                return result;
            }
            pdfFile.close();
        }
        file.close();

        result.detectedType = "application/pdf";
    }

    // 3. HTML validation
    else if (fmt == "html" || fmt == "htm") {
        if (result.fileSizeBytes < 10) {
            result.errorMessage = "HTML output file contains insufficient content.";
            return result;
        }
        result.detectedType = "text/html";
    } else {
        result.detectedType = "text/plain";
    }

    file.close();
    result.isValid = true;
    return result;
}

} // namespace uc::engines
