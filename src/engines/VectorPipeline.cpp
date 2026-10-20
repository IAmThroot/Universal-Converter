#include "VectorPipeline.h"
#include "../app/ToolchainManager.h"
#include "../core/ProcessRunner.h"
#include <QFileInfo>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QColor>

namespace uc::engines {

bool VectorPipeline::canConvert(const QString& inputFormatId, const QString& outputFormatId) {
    const QString in = inputFormatId.toLower().remove('.');
    const QString out = outputFormatId.toLower().remove('.');

    if (in != "svg") {
        return false;
    }

    const QStringList supportedOutputs = {
        "png", "jpg", "jpeg", "webp", "pdf", "bmp", "tiff", "tif", "ico"
    };

    return supportedOutputs.contains(out);
}

bool VectorPipeline::renderSvgToImageWithQt(const QString& inputPath,
                                            const QString& outputPath,
                                            const VectorRasterOptions& options,
                                            QString* errorMessage)
{
    QFile file(inputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QString("Cannot open SVG input file: %1").arg(inputPath);
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QSvgRenderer renderer(data);
    if (!renderer.isValid()) {
        if (errorMessage) *errorMessage = "Invalid or corrupted SVG file format.";
        return false;
    }

    QSize defaultSize = renderer.defaultSize();
    if (defaultSize.isEmpty()) {
        defaultSize = QSize(800, 600);
    }

    int targetW = options.width;
    int targetH = options.height;

    // Clamp to a safe upper bound to prevent multi-GB QImage allocations / OOM.
    constexpr int kMaxRasterDimension = 8192;
    constexpr qint64 kMaxRasterBytes = 128LL * 1024LL * 1024LL; // 128 MiB buffer limit
    if (targetW > kMaxRasterDimension || targetH > kMaxRasterDimension) {
        if (errorMessage) {
            *errorMessage = QString("Requested dimensions %1x%2 exceed maximum %3px.")
                                .arg(targetW).arg(targetH).arg(kMaxRasterDimension);
        }
        return false;
    }
    if (targetW < 0 || targetH < 0) {
        if (errorMessage) *errorMessage = "Requested dimensions must be non-negative.";
        return false;
    }

    if (targetW <= 0 && targetH <= 0) {
        targetW = defaultSize.width();
        targetH = defaultSize.height();
    } else if (targetW > 0 && targetH <= 0) {
        targetH = qRound(static_cast<double>(targetW) * defaultSize.height() / defaultSize.width());
    } else if (targetW <= 0 && targetH > 0) {
        targetW = qRound(static_cast<double>(targetH) * defaultSize.width() / defaultSize.height());
    } else if (options.keepAspectRatio) {
        const QSize scaled = defaultSize.scaled(targetW, targetH, Qt::KeepAspectRatio);
        targetW = scaled.width();
        targetH = scaled.height();
    }

    if (static_cast<qint64>(targetW) * targetH * 4 > kMaxRasterBytes) {
        if (errorMessage) {
            *errorMessage = QString("Requested dimensions %1x%2 exceed memory limit (%3 MiB).")
                                .arg(targetW).arg(targetH).arg(kMaxRasterBytes / (1024 * 1024));
        }
        return false;
    }

    QImage image(targetW, targetH, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        if (errorMessage) *errorMessage = "Failed to allocate render surface (dimensions too large).";
        return false;
    }

    if (options.backgroundColor.isEmpty() || options.backgroundColor.toLower() == "transparent") {
        image.fill(Qt::transparent);
    } else {
        const QColor bg(options.backgroundColor);
        image.fill(bg.isValid() ? bg : Qt::transparent);
    }

    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();

    const QString ext = QFileInfo(outputPath).suffix().toLower();
    const char* format = nullptr;
    if (ext == "png") format = "PNG";
    else if (ext == "jpg" || ext == "jpeg") format = "JPEG";
    else if (ext == "bmp") format = "BMP";
    else if (ext == "webp") format = "WEBP";
    else if (ext == "ico") format = "ICO";

    if (!image.save(outputPath, format)) {
        if (errorMessage) *errorMessage = QString("Failed to save rendered image to: %1").arg(outputPath);
        return false;
    }

    return true;
}

VectorConversionResult VectorPipeline::convertSvg(const QString& inputPath,
                                                 const QString& outputPath,
                                                 const VectorRasterOptions& options)
{
    VectorConversionResult result;
    result.outputFilePath = outputPath;

    const QFileInfo inInfo(inputPath);
    if (!inInfo.exists() || !inInfo.isFile()) {
        result.errorMessage = QString("Input vector file not found: %1").arg(inputPath);
        return result;
    }

    const QString outExt = QFileInfo(outputPath).suffix().toLower();
    const auto& tc = app::ToolchainManager::instance();

    // 1. Try rsvg-convert (fastest and highest quality for SVG -> PNG, PDF, PS, EPS)
    if (tc.rsvgConvert().isAvailable) {
        QStringList args;

        if (outExt == "png") {
            args << "-f" << "png";
        } else if (outExt == "pdf") {
            args << "-f" << "pdf";
        } else if (outExt == "ps" || outExt == "eps") {
            args << "-f" << outExt;
        }

        if (!args.isEmpty()) {
            if (options.width > 0) {
                args << "-w" << QString::number(options.width);
            }
            if (options.height > 0) {
                args << "-h" << QString::number(options.height);
            }
            if (options.keepAspectRatio) {
                args << "-a";
            }
            if (options.dpi > 0) {
                args << "-d" << QString::number(options.dpi) << "-p" << QString::number(options.dpi);
            }
            if (!options.backgroundColor.isEmpty() && options.backgroundColor.toLower() != "transparent") {
                args << "-b" << options.backgroundColor;
            }

            args << "-o" << outputPath << inputPath;

            const auto runResult = core::ProcessRunner::executeSync(tc.rsvgConvert().executablePath, args, 30000);
            if (runResult.isSuccess() && QFile::exists(outputPath) && QFileInfo(outputPath).size() > 0) {
                result.isSuccess = true;
                result.usedEngine = "rsvg-convert";
                return result;
            }
        }
    }

    // 2. Fallback for raster formats (PNG, JPG, BMP, WEBP, ICO) using Qt6::Svg
    if (outExt != "pdf" && outExt != "ps" && outExt != "eps") {
        QString qtError;
        if (renderSvgToImageWithQt(inputPath, outputPath, options, &qtError)) {
            result.isSuccess = true;
            result.usedEngine = "QtSvg";
            return result;
        }
    }

    // 3. Fallback for PDF or any other format using ImageMagick
    if (tc.magick().isAvailable) {
        QStringList args;
        if (options.dpi > 0) {
            args << "-density" << QString::number(options.dpi);
        }
        args << inputPath;
        if (options.width > 0 || options.height > 0) {
            args << "-resize" << QString("%1x%2%3")
                     .arg(options.width > 0 ? QString::number(options.width) : "")
                     .arg(options.height > 0 ? QString::number(options.height) : "")
                     .arg(options.keepAspectRatio ? "" : "!");
        }
        args << outputPath;

        const auto runResult = core::ProcessRunner::executeSync(tc.magick().executablePath, args, 30000);
        if (runResult.isSuccess() && QFile::exists(outputPath) && QFileInfo(outputPath).size() > 0) {
            result.isSuccess = true;
            result.usedEngine = "ImageMagick";
            return result;
        }
    }

    result.isSuccess = false;
    result.errorMessage = QString("All vector conversion backends failed to convert SVG to %1").arg(outExt.toUpper());
    return result;
}

} // namespace uc::engines
