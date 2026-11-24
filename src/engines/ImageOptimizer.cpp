#include "ImageOptimizer.h"
#include "../app/ToolchainManager.h"
#include "../core/ProcessRunner.h"
#include <QFileInfo>
#include <QFile>

namespace uc::engines {

OptimizationResult ImageOptimizer::optimizeImage(const QString& filePath,
                                                const ImageOptimizerOptions& options)
{
    OptimizationResult result;
    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        result.errorMessage = QString("File does not exist: %1").arg(filePath);
        return result;
    }

    result.originalSizeBytes = fi.size();
    result.optimizedSizeBytes = result.originalSizeBytes;
    result.usedTool = "none";

    const QString ext = fi.suffix().toLower();
    const auto& tc = app::ToolchainManager::instance();

    // 1. PNG Optimization: Try oxipng, pngquant, or ImageMagick
    if (ext == "png") {
        if (tc.oxipng().isAvailable) {
            QStringList args;
            args << "-o" << QString::number(qBound(1, options.compressionLevel, 6));
            if (options.stripMetadata) {
                args << "--strip" << "safe";
            }
            args << filePath;

            const auto runRes = core::ProcessRunner::executeSync(tc.oxipng().executablePath, args, 30000);
            if (runRes.isSuccess()) {
                const qint64 newSize = QFileInfo(filePath).size();
                result.optimizedSizeBytes = newSize;
                result.usedTool = "oxipng";
                result.isOptimized = (newSize < result.originalSizeBytes);
                if (result.originalSizeBytes > 0) {
                    result.savingsPercent = 100.0 * (1.0 - static_cast<double>(newSize) / result.originalSizeBytes);
                }
                return result;
            }
        }

        if (!options.lossless && tc.pngquant().isAvailable) {
            QStringList args;
            args << "--ext" << ".png" << "--force"
                 << "--quality" << QString("%1-%2").arg(qMax(10, options.quality - 20)).arg(options.quality);
            if (options.stripMetadata) {
                args << "--strip";
            }
            args << filePath;

            const auto runRes = core::ProcessRunner::executeSync(tc.pngquant().executablePath, args, 30000);
            if (runRes.isSuccess()) {
                const qint64 newSize = QFileInfo(filePath).size();
                result.optimizedSizeBytes = newSize;
                result.usedTool = "pngquant";
                result.isOptimized = (newSize < result.originalSizeBytes);
                if (result.originalSizeBytes > 0) {
                    result.savingsPercent = 100.0 * (1.0 - static_cast<double>(newSize) / result.originalSizeBytes);
                }
                return result;
            }
        }
    }

    // 2. Generic fallback optimizer via ImageMagick (lossless or strip)
    if (tc.magick().isAvailable && options.stripMetadata) {
        QStringList args;
        args << filePath << "-strip" << filePath;
        const auto runRes = core::ProcessRunner::executeSync(tc.magick().executablePath, args, 15000);
        if (runRes.isSuccess()) {
            const qint64 newSize = QFileInfo(filePath).size();
            result.optimizedSizeBytes = newSize;
            result.usedTool = "ImageMagick";
            result.isOptimized = (newSize < result.originalSizeBytes);
            if (result.originalSizeBytes > 0) {
                result.savingsPercent = 100.0 * (1.0 - static_cast<double>(newSize) / result.originalSizeBytes);
            }
            return result;
        }
    }

    result.isOptimized = false;
    return result;
}

} // namespace uc::engines
