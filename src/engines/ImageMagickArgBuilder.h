#pragma once

#include <QString>
#include <QStringList>
#include <QList>

namespace uc::engines {

struct ImageConversionOptions {
    QString targetFormatId;
    int quality{85};              // 1 - 100 for JPEG/WebP/AVIF
    int width{0};                 // Resize width (0 = original)
    int height{0};                // Resize height (0 = original)
    bool preserveAspectRatio{true};
    bool stripMetadata{false};
    bool autoOrient{true};

    // Animation settings
    bool preserveAnimation{true};
    int animationDelayMs{0};      // 0 = preserve original frame delay

    // Icon settings
    bool generateMultiResolutionIcon{true}; // e.g. 16, 32, 48, 64, 128, 256 for .ico
    QList<int> iconSizes{16, 32, 48, 64, 128, 256};
};

struct MagickArgumentResult {
    bool isValid{false};
    QStringList arguments;
    QString errorMessage;
};

class ImageMagickArgBuilder {
public:
    static MagickArgumentResult buildArguments(const QString& inputPath,
                                               const QString& outputPath,
                                               const ImageConversionOptions& options);
};

} // namespace uc::engines
