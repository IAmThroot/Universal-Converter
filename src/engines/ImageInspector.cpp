#include "ImageInspector.h"
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include "../core/ProcessRunner.h"

namespace uc::engines {

ImageInspectResult ImageInspector::inspect(const QString& magickPath, const QString& filePath) {
    ImageInspectResult result;

    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        result.errorMessage = QString("File does not exist: %1").arg(filePath);
        return result;
    }

    result.fileSizeBytes = fi.size();
    if (result.fileSizeBytes <= 16) {
        result.errorMessage = QString("File is empty or corrupt (%1 bytes).").arg(result.fileSizeBytes);
        return result;
    }

    if (magickPath.isEmpty() || !QFile::exists(magickPath)) {
        // Fallback: File exists and is non-empty, but cannot inspect deeper
        result.isValid = true;
        return result;
    }

    // ImageMagick 7 uses `magick identify ...`, whereas ImageMagick 6 has standalone `identify`
    QString identifyProg = magickPath;
    QStringList args;
    const QString exeBase = QFileInfo(magickPath).fileName().toLower();
    if (exeBase.startsWith("convert")) {
        const QString companionIdentify = QDir(QFileInfo(magickPath).absolutePath()).filePath("identify");
        if (QFile::exists(companionIdentify)) {
            identifyProg = companionIdentify;
        } else {
            identifyProg = "identify";
        }
        args = {"-format", "%m %w %h\n", filePath};
    } else {
        args = {"identify", "-format", "%m %w %h\n", filePath};
    }

    const auto res = core::ProcessRunner::executeSync(identifyProg, args, 4000);
    if (!res.isSuccess()) {
        result.errorMessage = QString("Failed to identify image: %1")
                                  .arg(res.errorMessage.isEmpty() ? "Identify execution failed" : res.errorMessage);
        return result;
    }

    const QString out = QString::fromUtf8(res.standardOutput).trimmed();
    const auto lines = out.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        result.errorMessage = "ImageMagick identify returned no output.";
        return result;
    }

    result.frameCount = static_cast<int>(lines.size());

    // Parse frames
    for (const auto& line : lines) {
        const auto parts = line.trimmed().split(QRegularExpression("\\s+"));
        if (parts.size() >= 3) {
            if (result.format.isEmpty()) {
                result.format = parts.value(0);
            }
            const int w = parts.value(1).toInt();
            const int h = parts.value(2).toInt();
            if (result.width == 0 && result.height == 0) {
                result.width = w;
                result.height = h;
            }
            result.frameSizes.append(QSize(w, h));
        }
    }

    result.isValid = true;
    return result;
}

} // namespace uc::engines
