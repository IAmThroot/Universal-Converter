#include "FfmpegArgBuilder.h"
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>
#include "../core/ProcessRunner.h"

namespace uc::engines {

namespace {

bool isStrictUtf8(const QByteArray& data) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    const int len = data.size();
    int i = 0;
    while (i < len) {
        const unsigned char c = bytes[i];
        if (c <= 0x7F) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len) return false;
            if ((bytes[i + 1] & 0xC0) != 0x80) return false;
            if (c < 0xC2) return false; // Overlong
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len) return false;
            if ((bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80) return false;
            if (c == 0xE0 && bytes[i + 1] < 0xA0) return false; // Overlong
            if (c == 0xED && bytes[i + 1] >= 0xA0) return false; // Surrogate
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len) return false;
            if ((bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80 || (bytes[i + 3] & 0xC0) != 0x80) return false;
            if (c == 0xF0 && bytes[i + 1] < 0x90) return false; // Overlong
            if (c == 0xF4 && bytes[i + 1] > 0x8F) return false; // Out of range (> U+10FFFF)
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

QString FfmpegArgBuilder::selectVideoEncoder(const QString& targetContainer,
                                             HardwareEncoderMode hwMode,
                                             const QStringList& availableHwEncoders,
                                             bool& outIsHw)
{
    outIsHw = false;
    const QString cnt = targetContainer.toLower();

    // Check hardware availability if requested
    const bool allowNvenc = (hwMode == HardwareEncoderMode::Auto || hwMode == HardwareEncoderMode::Nvenc);
    const bool allowVaapi = (hwMode == HardwareEncoderMode::Auto || hwMode == HardwareEncoderMode::Vaapi);

    if (cnt == "mp4" || cnt == "mkv" || cnt == "mov" || cnt == "ts" || cnt == "flv" || cnt == "avi") {
        if (allowNvenc && availableHwEncoders.contains("h264_nvenc")) {
            outIsHw = true;
            return "h264_nvenc";
        }
        if (allowVaapi && availableHwEncoders.contains("h264_vaapi")) {
            outIsHw = true;
            return "h264_vaapi";
        }
        return "libx264";
    }

    if (cnt == "webm") {
        if (allowNvenc && availableHwEncoders.contains("av1_nvenc")) {
            outIsHw = true;
            return "av1_nvenc";
        }
        return "libvpx-vp9";
    }

    if (cnt == "ogv") {
        return "libtheora";
    }

    if (cnt == "wmv" || cnt == "asf") {
        return "wmv2";
    }

    if (cnt == "mpeg" || cnt == "mpg" || cnt == "vob") {
        return "mpeg2video";
    }

    if (cnt == "3gp" || cnt == "3g2") {
        return "h263";
    }

    // Default universal video fallback
    return "libx264";
}

bool FfmpegArgBuilder::isAudioContainer(const QString& container) {
    static const QSet<QString> s_audioContainers = {
        "mp3", "wav", "flac", "aac", "ogg", "oga", "opus", "m4a", "wma",
        "ac3", "eac3", "dts", "mp2", "aiff", "au", "voc", "wv", "qoa", "m4b"
    };
    return s_audioContainers.contains(container.toLower());
}

bool FfmpegArgBuilder::isSubtitleContainer(const QString& container) {
    static const QSet<QString> s_subContainers = {
        "srt", "vtt", "ass", "ssa", "sub"
    };
    return s_subContainers.contains(container.toLower());
}

bool FfmpegArgBuilder::isAnimatedImageContainer(const QString& container) {
    const QString c = container.toLower();
    return (c == "gif" || c == "webp" || c == "avif");
}

QString FfmpegArgBuilder::detectSubtitleEncoding(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return "UTF-8";
    }

    const QByteArray sample = file.read(512 * 1024);
    file.close();

    if (sample.isEmpty()) {
        return "UTF-8";
    }

    // 1. Check for standard Byte Order Marks (BOM)
    if (sample.startsWith("\xEF\xBB\xBF")) {
        return "UTF-8";
    }
    if (sample.startsWith("\xFF\xFE")) {
        return "UTF-16LE";
    }
    if (sample.startsWith("\xFE\xFF")) {
        return "UTF-16BE";
    }

    // 2. Strict UTF-8 validation
    if (isStrictUtf8(sample)) {
        return "UTF-8";
    }

    // 3. Not valid UTF-8: try uchardet if available
    const QString uchardetPath = QStandardPaths::findExecutable("uchardet");
    if (!uchardetPath.isEmpty()) {
        const auto res = core::ProcessRunner::executeSync(uchardetPath, {filePath}, 1500);
        if (res.isSuccess()) {
            const QString detected = QString::fromUtf8(res.standardOutput).trimmed();
            if (!detected.isEmpty() && !detected.contains("unknown", Qt::CaseInsensitive)) {
                return detected;
            }
        }
    }

    // 4. Standard fallback for non-UTF8 text subtitles (overwhelmingly Windows-1252 / Latin-1)
    return "CP1252";
}

ArgumentBuildResult FfmpegArgBuilder::buildSubtitleArguments(const QString& inputPath,
                                                             const QString& outputPath,
                                                             const QString& targetFormatId)
{
    ArgumentBuildResult result;

    if (inputPath.isEmpty() || outputPath.isEmpty()) {
        result.errorMessage = "Input or output path is empty.";
        return result;
    }

    if (QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath()) {
        result.errorMessage = "Input and output path resolve to the exact same file.";
        return result;
    }

    const QString target = targetFormatId.toLower();
    QString codec;
    if (target == "srt") {
        codec = "subrip";
    } else if (target == "vtt") {
        codec = "webvtt";
    } else if (target == "ass") {
        codec = "ass";
    } else if (target == "ssa") {
        codec = "ssa";
    } else {
        codec = "copy";
    }

    const QString charenc = detectSubtitleEncoding(inputPath);

    QStringList args;
    args << "-hide_banner" << "-y";
    if (!charenc.isEmpty()) {
        args << "-sub_charenc" << charenc;
    }
    args << "-i" << inputPath;
    args << "-c:s" << codec;
    args << outputPath;

    result.isValid = true;
    result.arguments = args;
    result.chosenAudioCodec = codec;
    return result;
}

QString FfmpegArgBuilder::selectAudioEncoder(const QString& targetContainer) {
    const QString cnt = targetContainer.toLower();

    if (cnt == "mp3") return "libmp3lame";
    if (cnt == "wav") return "pcm_s16le";
    if (cnt == "flac") return "flac";
    if (cnt == "aac" || cnt == "m4a" || cnt == "m4b" || cnt == "mp4" || cnt == "mov") return "aac";
    if (cnt == "ogg" || cnt == "oga" || cnt == "ogv") return "libvorbis";
    if (cnt == "opus" || cnt == "webm") return "libopus";
    if (cnt == "wma") return "wmav2";
    if (cnt == "ac3") return "ac3";
    if (cnt == "eac3") return "eac3";
    if (cnt == "dts") return "dca";
    if (cnt == "mp2") return "mp2";
    if (cnt == "aiff") return "pcm_s16be";
    if (cnt == "au") return "pcm_s16be";
    if (cnt == "voc") return "pcm_u8";
    if (cnt == "wv") return "wavpack";
    if (cnt == "3gp" || cnt == "3g2") return "amr_nb";

    return "aac";
}

QStringList FfmpegArgBuilder::buildSpeedAndBitrateArgs(const QString& encoder,
                                                       ConversionPreset preset,
                                                       int videoBitrateKbps)
{
    QStringList args;
    const int level = static_cast<int>(preset);

    if (encoder == "libx264" || encoder == "libx265") {
        const QStringList pNames = {"ultrafast", "fast", "medium", "slow", "slower", "veryslow"};
        args << "-preset" << pNames.value(qBound(0, level, 5));
        if (videoBitrateKbps > 0) {
            args << "-b:v" << QString("%1k").arg(videoBitrateKbps);
        } else {
            args << "-crf" << (encoder == "libx264" ? "23" : "28");
        }
    } else if (encoder == "h264_nvenc" || encoder == "hevc_nvenc" || encoder == "av1_nvenc") {
        const QStringList pNames = {"p1", "p2", "p4", "p5", "p6", "p7"};
        args << "-preset" << pNames.value(qBound(0, level, 5));
        if (videoBitrateKbps > 0) {
            args << "-b:v" << QString("%1k").arg(videoBitrateKbps);
        } else {
            args << "-cq" << "24";
        }
    } else if (encoder == "libvpx" || encoder == "libvpx-vp9") {
        const QStringList cpuUsed = {"8", "6", "4", "2", "1", "0"};
        args << "-cpu-used" << cpuUsed.value(qBound(0, level, 5));
        if (videoBitrateKbps > 0) {
            args << "-b:v" << QString("%1k").arg(videoBitrateKbps);
        } else {
            args << "-crf" << "32" << "-b:v" << "0";
        }
    } else if (encoder == "libsvtav1") {
        const QStringList pNames = {"12", "10", "8", "6", "4", "2"};
        args << "-preset" << pNames.value(qBound(0, level, 5));
        if (videoBitrateKbps > 0) {
            args << "-b:v" << QString("%1k").arg(videoBitrateKbps);
        } else {
            args << "-crf" << "30";
        }
    } else {
        if (videoBitrateKbps > 0) {
            args << "-b:v" << QString("%1k").arg(videoBitrateKbps);
        }
    }

    return args;
}

ArgumentBuildResult FfmpegArgBuilder::buildArguments(const QString& inputPath,
                                                     const QString& outputPath,
                                                     const MediaProbeResult& probe,
                                                     const FfmpegConversionOptions& options,
                                                     const QStringList& availableHwEncoders)
{
    ArgumentBuildResult result;

    if (inputPath.isEmpty() || outputPath.isEmpty()) {
        result.errorMessage = "Input or output path is empty.";
        return result;
    }

    if (QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath()) {
        result.errorMessage = "Input and output path resolve to the exact same file.";
        return result;
    }

    const QString targetContainer = options.targetFormatId.toLower();

    // Determine workflow: Is output an audio format or a video format?
    const bool isTargetAudioOnly = isAudioContainer(targetContainer);

    QStringList args;
    args << "-hide_banner" << "-y";

    // Progress reporting over stdout pipe
    args << "-progress" << "pipe:1";

    // Input path
    args << "-i" << inputPath;

    if (isTargetAudioOnly) {
        // --- Workflow: Video-to-Audio extraction or Audio-to-Audio transcoding ---
        if (!probe.hasAudio() && probe.isValid) {
            result.errorMessage = "Input media has no audio streams to convert or extract.";
            return result;
        }

        // Disable video stream output
        args << "-vn";

        const QString audioEncoder = options.customAudioCodec.isEmpty() ?
                                         selectAudioEncoder(targetContainer) : options.customAudioCodec;
        result.chosenAudioCodec = audioEncoder;
        args << "-c:a" << audioEncoder;

        if (options.customAudioBitrateKbps > 0) {
            args << "-b:a" << QString("%1k").arg(options.customAudioBitrateKbps);
        }
        if (options.customAudioSampleRate > 0) {
            args << "-ar" << QString::number(options.customAudioSampleRate);
        }
        if (options.customAudioChannels > 0) {
            args << "-ac" << QString::number(options.customAudioChannels);
        }
    } else {
        // --- Workflow: Video-to-Video transcoding or Video-to-Animated Image or Audio-to-Video bridging ---
        if (probe.hasVideo()) {
            if (targetContainer == "gif") {
                // High-quality palettegen/paletteuse pipeline for animated GIF
                result.chosenVideoCodec = "gif";
                args << "-c:v" << "gif";

                QString filter;
                if (options.customWidth > 0 && options.customHeight > 0) {
                    filter = QString("scale=%1:%2:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse")
                                 .arg(options.customWidth).arg(options.customHeight);
                } else if (options.customWidth > 0) {
                    filter = QString("scale=%1:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse")
                                 .arg(options.customWidth);
                } else {
                    filter = "split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse";
                }
                args << "-vf" << filter;
                if (options.customFps > 0) {
                    args << "-r" << QString::number(options.customFps);
                }
                // GIFs strictly have no audio
                args << "-an";
            } else if (targetContainer == "webp") {
                result.chosenVideoCodec = "libwebp";
                args << "-c:v" << "libwebp";
                args << "-loop" << "0";
                if (options.customFps > 0) {
                    args << "-r" << QString::number(options.customFps);
                }
                if (options.customWidth > 0 && options.customHeight > 0) {
                    args << "-vf" << QString("scale=%1:%2").arg(options.customWidth).arg(options.customHeight);
                }
                // Strip audio for animated webp
                args << "-an";
            } else if (targetContainer == "avif") {
                result.chosenVideoCodec = "libsvtav1";
                args << "-c:v" << "libsvtav1";
                args << "-crf" << "30";
                if (options.customFps > 0) {
                    args << "-r" << QString::number(options.customFps);
                }
                if (options.customWidth > 0 && options.customHeight > 0) {
                    args << "-vf" << QString("scale=%1:%2").arg(options.customWidth).arg(options.customHeight);
                }
                // Strip audio for animated avif
                args << "-an";
            } else {
                bool isHw = false;
                const QString videoEncoder = options.customVideoCodec.isEmpty() ?
                                                 selectVideoEncoder(targetContainer, options.hwMode, availableHwEncoders, isHw) :
                                                 options.customVideoCodec;
                result.chosenVideoCodec = videoEncoder;
                result.isHardwareAccelerated = isHw;

                args << "-c:v" << videoEncoder;

                // Pixel format standardization (yuv420p is universally supported by NVENC, VAAPI, VP9, and x264)
                args << "-pix_fmt" << "yuv420p";

                // Speed and bitrate
                args.append(buildSpeedAndBitrateArgs(videoEncoder, options.preset, options.customVideoBitrateKbps));

                // Resolution / scaling
                if (options.customWidth > 0 && options.customHeight > 0) {
                    args << "-vf" << QString("scale=%1:%2").arg(options.customWidth).arg(options.customHeight);
                }
                if (options.customFps > 0) {
                    args << "-r" << QString::number(options.customFps);
                }

                // Audio handling in video container
                if (probe.hasAudio()) {
                    const QString audioEncoder = options.customAudioCodec.isEmpty() ?
                                                     selectAudioEncoder(targetContainer) : options.customAudioCodec;
                    result.chosenAudioCodec = audioEncoder;
                    args << "-c:a" << audioEncoder;
                } else {
                    args << "-an";
                }
            }
        } else if (probe.hasAudio()) {
            // Audio-to-Video bridging (Audio stream with black video)
            result.chosenVideoCodec = "libx264";
            result.chosenAudioCodec = selectAudioEncoder(targetContainer);

            // Generate black video canvas matching audio length
            args << "-f" << "lavfi" << "-i" << "color=c=black:s=1280x720:r=25";
            args << "-c:v" << "libx264" << "-pix_fmt" << "yuv420p" << "-shortest";
            args << "-c:a" << result.chosenAudioCodec;
        } else {
            result.errorMessage = "Input file contains neither video nor audio streams.";
            return result;
        }

        // Faststart flag for web-friendly streaming in MP4/MOV
        if (targetContainer == "mp4" || targetContainer == "mov" || targetContainer == "m4v") {
            args << "-movflags" << "+faststart";
        }
    }

    // Output path
    args << outputPath;

    result.isValid = true;
    result.arguments = args;
    return result;
}

} // namespace uc::engines
