#pragma once

#include <QString>
#include <QStringList>
#include "MediaProbe.h"

namespace uc::engines {

enum class ConversionPreset {
    UltraFast,
    Fast,
    Medium,
    Slow,
    Slower,
    VerySlow
};

enum class HardwareEncoderMode {
    Auto,      // Prefer NVENC / VA-API if supported, fallback to software
    Disabled,  // Force CPU software encoders
    Nvenc,     // Force NVIDIA NVENC
    Vaapi,     // Force VA-API
    Qsv        // Force Intel QSV
};

struct FfmpegConversionOptions {
    QString targetFormatId;
    ConversionPreset preset{ConversionPreset::Medium};
    HardwareEncoderMode hwMode{HardwareEncoderMode::Auto};
    QString hwDevice;              // Custom/detected render device (e.g. /dev/dri/renderD128)

    // Video options
    bool hasVideo{true};
    QString customVideoCodec;
    int customVideoBitrateKbps{0}; // 0 = automatic / CRF based
    int crf{-1};                   // -1 = automatic per codec
    int customWidth{0};
    int customHeight{0};
    int customFps{0};

    // Audio options
    bool hasAudio{true};
    QString customAudioCodec;
    int customAudioBitrateKbps{0};
    int customAudioSampleRate{0};
    int customAudioChannels{0};

    // Additional flags
    bool stripMetadata{false};
};

struct ArgumentBuildResult {
    bool isValid{false};
    QStringList arguments;
    QString chosenVideoCodec;
    QString chosenAudioCodec;
    bool isHardwareAccelerated{false};
    QString errorMessage;
};

class FfmpegArgBuilder {
public:
    static ArgumentBuildResult buildArguments(const QString& inputPath,
                                              const QString& outputPath,
                                              const MediaProbeResult& probe,
                                              const FfmpegConversionOptions& options,
                                              const QStringList& availableHwEncoders);

    static bool isAudioContainer(const QString& container);
    static bool isSubtitleContainer(const QString& container);
    static bool isAnimatedImageContainer(const QString& container);
    static QString selectAudioEncoder(const QString& targetContainer);
    static QString detectSubtitleEncoding(const QString& filePath);

    static ArgumentBuildResult buildSubtitleArguments(const QString& inputPath,
                                                      const QString& outputPath,
                                                      const QString& targetFormatId);

private:
    static QString selectVideoEncoder(const QString& targetContainer,
                                      HardwareEncoderMode hwMode,
                                      const QStringList& availableHwEncoders,
                                      bool& outIsHw);

    static QStringList buildSpeedAndBitrateArgs(const QString& encoder,
                                                ConversionPreset preset,
                                                int videoBitrateKbps,
                                                int crf = -1);
};

} // namespace uc::engines
