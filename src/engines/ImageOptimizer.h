#pragma once

#include <QString>
#include <QStringList>

namespace uc::engines {

struct ImageOptimizerOptions {
    bool lossless{true};
    int quality{85};        // 1-100 for lossy passes
    int compressionLevel{2}; // 0-6 for oxipng/pngquant
    bool stripMetadata{false};
};

struct OptimizationResult {
    bool isOptimized{false};
    qint64 originalSizeBytes{0};
    qint64 optimizedSizeBytes{0};
    double savingsPercent{0.0};
    QString usedTool; // "oxipng", "pngquant", "ImageMagick", or "none"
    QString errorMessage;
};

class ImageOptimizer {
public:
    /// Optimize an image file in-place (or between paths)
    static OptimizationResult optimizeImage(const QString& filePath,
                                            const ImageOptimizerOptions& options = ImageOptimizerOptions());

};

} // namespace uc::engines
