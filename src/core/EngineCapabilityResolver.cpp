#include "EngineCapabilityResolver.h"
#include "../app/ToolchainManager.h"
#include "../engines/ImageMagickCapabilities.h"
#include "../engines/PandocCapabilities.h"
#include "ProcessRunner.h"
#include "SystemInfo.h"
#include "../app/AppSettings.h"
#include "../engines/VectorPipeline.h"
#include "../engines/FfmpegArgBuilder.h"

namespace uc::core {

EngineCapabilityResolver &EngineCapabilityResolver::instance() {
    static EngineCapabilityResolver s_instance;
    return s_instance;
}

EngineCapabilityResolver::EngineCapabilityResolver() {
    connect(&app::ToolchainManager::instance(), &app::ToolchainManager::toolchainStatusChanged, this,
            [this]() {
                if (!app::ToolchainManager::instance().isScanning())
                    refresh();
            });
    if (!app::ToolchainManager::instance().isScanning())
        refresh();
}

static QSet<QString> listedNames(const QByteArray &listing, bool muxers) {
    QSet<QString> names;
    for (const auto &line : listing.split('\n')) {
        const auto parts = line.simplified().split(' ');
        if (parts.size() < 2 || parts[1] == "=")
            continue;
        const auto flags = parts.first();
        if (muxers ? !flags.contains('E') : flags.size() != 6 || !QByteArray("VAS").contains(flags[0]))
            continue;
        for (const auto &name : parts[1].split(','))
            names.insert(QString::fromLatin1(name));
    }
    return names;
}

void EngineCapabilityResolver::refresh() {
    m_cache.clear();
    const auto ffmpeg = app::ToolchainManager::instance().ffmpeg();
    m_hardwareEncoders = SystemInfo::probe().availableHwEncoders;
    m_muxers.clear();
    m_encoders.clear();
    if (ffmpeg.isAvailable) {
        m_muxers =
            listedNames(ProcessRunner::executeSync(ffmpeg.executablePath, {"-hide_banner", "-muxers"}, 3000)
                            .standardOutput,
                        true);
        m_encoders =
            listedNames(ProcessRunner::executeSync(ffmpeg.executablePath, {"-hide_banner", "-encoders"}, 3000)
                            .standardOutput,
                        false);
    }

    const auto &formats = FormatRegistry::instance().allFormats();
    const auto &toolchain = app::ToolchainManager::instance();
    const bool hasFfmpeg = toolchain.ffmpeg().isAvailable;
    const bool hasMagick = toolchain.magick().isAvailable;
    const bool hasPandoc = toolchain.pandoc().isAvailable;

    auto &magickCaps = engines::ImageMagickCapabilities::instance();
    auto &pandocCaps = engines::PandocCapabilities::instance();

    if (hasMagick && !magickCaps.isDiscovered()) {
        magickCaps.discover(toolchain.magick().executablePath);
    }
    if (hasPandoc && !pandocCaps.isDiscovered()) {
        pandocCaps.discover(toolchain.pandoc().executablePath);
    }

    for (const auto &fmt : formats) {
        DynamicFormatCapability cap;
        cap.definition = fmt;
        cap.readiness = FormatReadiness::Recognized;
        cap.isReadableOnHost = false;
        cap.isWritableOnHost = false;

        if (fmt.category == MediaCategory::Video || fmt.category == MediaCategory::Audio ||
            fmt.category == MediaCategory::Subtitle) {
            cap.activeEngineName = "FFmpeg";
            if (hasFfmpeg) {
                cap.isReadableOnHost = fmt.canInput;
                cap.isWritableOnHost = fmt.canOutput;
                cap.readiness = (fmt.tier == CapabilityTier::TierA_VerifiedVertAndNative)
                                    ? FormatReadiness::Verified
                                    : FormatReadiness::Available;
                cap.engineStatusDetail = "Native FFmpeg container/codec engine active";
            } else {
                cap.engineStatusDetail = "Requires FFmpeg installation";
            }
        } else if (fmt.category == MediaCategory::Image) {
            cap.activeEngineName = "ImageMagick";
            if (hasMagick) {
                const QString ext = fmt.defaultExtension.toUpper();
                const bool canRead = magickCaps.canReadFormat(ext);
                const bool canWrite = magickCaps.canWriteFormat(ext);
                cap.isReadableOnHost = canRead;
                cap.isWritableOnHost = canWrite;

                if (canRead || canWrite) {
                    cap.readiness = (fmt.tier == CapabilityTier::TierA_VerifiedVertAndNative)
                                        ? FormatReadiness::Verified
                                        : FormatReadiness::Available;
                    cap.engineStatusDetail = QString("ImageMagick %1 (R: %2, W: %3)")
                                                 .arg(magickCaps.versionString().left(25))
                                                 .arg(canRead ? "Yes" : "No")
                                                 .arg(canWrite ? "Yes" : "No");
                } else {
                    cap.engineStatusDetail = "Delegates not installed or format restricted";
                }
            } else {
                cap.engineStatusDetail = "Requires ImageMagick installation";
            }
        } else if (fmt.category == MediaCategory::Document) {
            cap.activeEngineName = "Pandoc";
            if (hasPandoc) {
                const bool canRead = pandocCaps.canReadFormat(fmt.defaultExtension);
                const bool canWrite = pandocCaps.canWriteFormat(fmt.defaultExtension);
                cap.isReadableOnHost = canRead;
                cap.isWritableOnHost = canWrite;

                if (fmt.id == "pdf") {
                    cap.isWritableOnHost = pandocCaps.hasAnyPdfEngine();
                    if (!pandocCaps.hasAnyPdfEngine()) {
                        cap.engineStatusDetail = "PDF output requires Typst, XeLaTeX, or wkhtmltopdf";
                    } else {
                        cap.engineStatusDetail =
                            QString("Engine: %1").arg(pandocCaps.preferredPdfEngine().name);
                    }
                } else {
                    cap.engineStatusDetail = QString("Pandoc %1").arg(pandocCaps.versionString().left(20));
                }

                if (cap.isReadableOnHost || cap.isWritableOnHost) {
                    cap.readiness = (fmt.tier == CapabilityTier::TierA_VerifiedVertAndNative)
                                        ? FormatReadiness::Verified
                                        : FormatReadiness::Available;
                }
            } else {
                cap.engineStatusDetail = "Requires Pandoc installation";
            }
        }

        m_cache.insert(fmt.id, cap);
    }
    emit capabilitiesChanged();
}

static bool hasEncodingOptions(MediaCategory category, const QVariantMap &options) {
    auto hasAny = [&](std::initializer_list<const char *> keys) {
        for (const auto *key : keys)
            if (options.contains(key))
                return true;
        return false;
    };
    if (category == MediaCategory::Image)
        return hasAny({"width", "height", "quality", "stripMetadata", "autoOrient", "preserveAnimation"});
    if (category == MediaCategory::Audio || category == MediaCategory::Video) {
        if (hasAny({"customAudioCodec", "customAudioBitrateKbps", "customAudioSampleRate", "customAudioChannels"}))
            return true;
        return category == MediaCategory::Video &&
               hasAny({"customWidth", "customHeight", "customVideoCodec", "customVideoBitrateKbps", "customFps", "preset", "crf"});
    }
    return false;
}

RouteCapability EngineCapabilityResolver::resolveRoute(const QString &input, const QString &output,
                                                       const QVariantMap &options) const {
    const auto *source = FormatRegistry::instance().findById(input);
    const bool reencode = source && input == output && options.value("presetTarget").toString() == output &&
                          hasEncodingOptions(source->category, options);
    const auto comp = FormatRegistry::instance().checkCompatibility(input, output, reencode);
    RouteCapability result;
    result.engine = comp.responsibleEngine;
    auto block = [&](const QString &why) {
        result.reason = why;
        return result;
    };
    if (app::ToolchainManager::instance().isScanning())
        return block("Scanning tools; please wait");
    if (!comp.isSupported)
        return block("These formats cannot be converted together");
    const auto in = resolveFormat(input), out = resolveFormat(output);
    if (comp.responsibleEngine == "ImageMagick" && input == "svg" &&
        engines::VectorPipeline::canConvert(input, output) && output == "png") {
        result.available = true;
        return result;
    }
    if (comp.responsibleEngine == "ImageMagick" && output == "pdf") {
        if (input == "svg" && app::ToolchainManager::instance().rsvgConvert().isAvailable) {
            result.available = true;
            return result;
        }
        if (!in.isReadableOnHost || !engines::ImageMagickCapabilities::instance().canWriteFormat("PDF"))
            return block("Image-to-PDF requires a writable ImageMagick PDF format or an SVG PDF renderer");
        result.available = true;
        return result;
    }
    if (comp.responsibleEngine != "FFmpeg" && !in.isReadableOnHost)
        return block("Input decoder or required tool is unavailable");
    if (comp.responsibleEngine != "FFmpeg" && !out.isWritableOnHost)
        return block(out.engineStatusDetail.isEmpty() ? "Output encoder is unavailable"
                                                      : out.engineStatusDetail);
    if (comp.responsibleEngine == "FFmpeg") {
        if (!app::ToolchainManager::instance().ffmpeg().isAvailable)
            return block("Install FFmpeg or configure its path in Settings");
        if (!engines::FfmpegArgBuilder::isSubtitleContainer(output) &&
            !app::ToolchainManager::instance().ffprobe().isAvailable)
            return block("FFprobe is required to inspect and validate media");
        static const QMap<QString, QString> mux = {
            {"aac", "adts"},    {"m4b", "ipod"},   {"mkv", "matroska"},     {"mka", "matroska"},
            {"m4a", "ipod"},    {"m4v", "ipod"},   {"srt", "srt"},          {"ssa", "ass"},
            {"mpeg", "mpeg"},   {"mpg", "mpeg"},   {"y4m", "yuv4mpegpipe"}, {"dv", "dv"},
            {"alac", "ipod"},   {"vtt", "webvtt"}, {"jpg", "image2"},       {"ts", "mpegts"},
            {"m2ts", "mpegts"}, {"wmv", "asf"},    {"wma", "asf"},          {"ogv", "ogg"},
            {"opus", "opus"},   {"aiff", "aiff"}};
        const QString container = mux.value(output, output);
        if (!m_muxers.contains(container))
            return block("Installed FFmpeg does not report this output container");
        if (engines::FfmpegArgBuilder::isSubtitleContainer(output)) {
            const QString codec = output == "srt"   ? "subrip"
                                  : output == "vtt" ? "webvtt"
                                  : output == "ssa" ? "ssa"
                                                    : "ass";
            if (!m_encoders.contains(codec))
                return block("Missing subtitle encoder: " + codec);
        } else {
            engines::MediaProbeResult probe;
            probe.isValid = true;
            probe.durationSeconds = 1;
            engines::MediaStreamInfo audio;
            audio.codecType = "audio";
            audio.sampleRate = 48000;
            audio.channels = 2;
            probe.streams << audio;
            if (in.definition.category != MediaCategory::Audio) {
                engines::MediaStreamInfo video;
                video.codecType = "video";
                video.width = 1280;
                video.height = 720;
                video.frameRate = 30;
                probe.streams << video;
            }
            const auto settings = engines::FfmpegConversionOptions::fromJob(
                output, options, app::AppSettings::instance().hardwareAccelerationEnabled());
            const auto built = engines::FfmpegArgBuilder::buildArguments("input." + input, "output." + output,
                                                                         probe, settings, m_hardwareEncoders);
            if (!built.isValid)
                return block(built.errorMessage);
            for (const auto &codec : {built.chosenVideoCodec, built.chosenAudioCodec})
                if (!codec.isEmpty() && codec != "copy" && !m_encoders.contains(codec))
                    return block("Required FFmpeg encoder missing: " + codec);
        }
    }
    result.available = true;
    return result;
}

QList<DynamicFormatCapability> EngineCapabilityResolver::allResolvedCapabilities() const {
    return m_cache.values();
}

DynamicFormatCapability EngineCapabilityResolver::resolveFormat(const QString &formatId) const {
    const QString norm = FormatRegistry::instance().normalizeFormatId(formatId);
    return m_cache.value(norm);
}

int EngineCapabilityResolver::totalRecognizedCount() const { return m_cache.size(); }

int EngineCapabilityResolver::totalAvailableCount() const {
    int count = 0;
    for (const auto &cap : m_cache) {
        if (cap.readiness == FormatReadiness::Available || cap.readiness == FormatReadiness::Verified) {
            count++;
        }
    }
    return count;
}

int EngineCapabilityResolver::totalVerifiedCount() const {
    int count = 0;
    for (const auto &cap : m_cache) {
        if (cap.readiness == FormatReadiness::Verified) {
            count++;
        }
    }
    return count;
}

} // namespace uc::core
