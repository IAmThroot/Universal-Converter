#include "ImageMagickArgBuilder.h"
#include <QFileInfo>

namespace uc::engines {

MagickArgumentResult ImageMagickArgBuilder::buildArguments(const QString& inputPath,
                                                           const QString& outputPath,
                                                           const ImageConversionOptions& options)
{
    MagickArgumentResult result;

    if (inputPath.isEmpty() || outputPath.isEmpty()) {
        result.errorMessage = "Input or output path is empty.";
        return result;
    }

    if (QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath()) {
        result.errorMessage = "Input and output path resolve to the exact same file.";
        return result;
    }

    const QString targetExt = QFileInfo(outputPath).suffix().toLower();
    QStringList args;

    // Multi-resolution Windows ICO generation
    if (targetExt == "ico" && options.generateMultiResolutionIcon) {
        // e.g. magick input.png -background transparent \( -clone 0 -resize 16x16 \) \( -clone 0 -resize 32x32 \) ... -delete 0 output.ico
        args << inputPath;
        args << "-background" << "transparent";

        for (int size : options.iconSizes) {
            args << "(" << "-clone" << "0" << "-resize" << QString("%1x%2").arg(size).arg(size) << ")";
        }
        args << "-delete" << "0";
        args << outputPath;

        result.isValid = true;
        result.arguments = args;
        return result;
    }

    // Standard raster / animated conversion
    // 1. Input path
    args << inputPath;

    // 2. Auto-orientation based on EXIF tag
    if (options.autoOrient) {
        args << "-auto-orient";
    }

    // 3. Animation frame preservation
    if (options.preserveAnimation && (targetExt == "gif" || targetExt == "webp" || targetExt == "apng")) {
        // Coalesce prevents frame disposal artifacts on re-encoding
        args << "-coalesce";
        if (options.animationDelayMs > 0) {
            // Delay in ticks (1/100 second)
            args << "-set" << "delay" << QString::number(options.animationDelayMs / 10);
        }
    }

    // 4. Resize if specified
    if (options.width > 0 || options.height > 0) {
        QString geom;
        if (options.width > 0 && options.height > 0) {
            geom = QString("%1x%2%3")
                       .arg(options.width)
                       .arg(options.height)
                       .arg(options.preserveAspectRatio ? "" : "!");
        } else if (options.width > 0) {
            geom = QString::number(options.width);
        } else {
            geom = QString("x%1").arg(options.height);
        }
        args << "-resize" << geom;
    }

    // 5. Quality setting where applicable (JPEG, WebP, AVIF, HEIC)
    if (targetExt == "jpg" || targetExt == "jpeg" || targetExt == "webp" || targetExt == "avif" || targetExt == "heic") {
        const int q = qBound(1, options.quality, 100);
        args << "-quality" << QString::number(q);
    }

    // 6. Strip metadata if requested
    if (options.stripMetadata) {
        args << "-strip";
    }

    // 7. Output destination
    args << outputPath;

    result.isValid = true;
    result.arguments = args;
    return result;
}

} // namespace uc::engines
