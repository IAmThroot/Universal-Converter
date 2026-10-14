#pragma once

#include <QString>
#include <QSize>

namespace uc::engines {

struct VectorRasterOptions {
    int width{0};           // Target width (0 = natural/aspect-ratio-preserved)
    int height{0};          // Target height (0 = natural/aspect-ratio-preserved)
    int dpi{96};            // DPI (standard 96)
    bool keepAspectRatio{true};
    QString backgroundColor; // e.g. "transparent", "white", "#ffffff"
};

struct VectorConversionResult {
    bool isSuccess{false};
    QString outputFilePath;
    QString errorMessage;
    QString usedEngine; // "rsvg-convert", "QtSvg", or "ImageMagick"
};

class VectorPipeline {
public:
    /// Checks if a vector format (e.g. "svg") can be converted to targetFormatId
    static bool canConvert(const QString& inputFormatId, const QString& outputFormatId);

    /// Converts SVG to raster format (PNG, JPEG, WebP, etc.) or vector PDF
    static VectorConversionResult convertSvg(const QString& inputPath,
                                            const QString& outputPath,
                                            const VectorRasterOptions& options = VectorRasterOptions());

    /// Render SVG to QImage using Qt6::Svg (Zero-dependency fallback)
    static bool renderSvgToImageWithQt(const QString& inputPath,
                                       const QString& outputPath,
                                       const VectorRasterOptions& options,
                                       QString* errorMessage = nullptr);
};

} // namespace uc::engines
