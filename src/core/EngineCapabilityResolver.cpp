#include "EngineCapabilityResolver.h"
#include "../app/ToolchainManager.h"
#include "../engines/ImageMagickCapabilities.h"
#include "../engines/PandocCapabilities.h"

namespace uc::core {

EngineCapabilityResolver& EngineCapabilityResolver::instance() {
    static EngineCapabilityResolver s_instance;
    return s_instance;
}

EngineCapabilityResolver::EngineCapabilityResolver() {
    refresh();
}

void EngineCapabilityResolver::refresh() {
    m_cache.clear();

    const auto& formats = FormatRegistry::instance().allFormats();
    const auto& toolchain = app::ToolchainManager::instance();
    const bool hasFfmpeg = toolchain.ffmpeg().isAvailable;
    const bool hasMagick = toolchain.magick().isAvailable;
    const bool hasPandoc = toolchain.pandoc().isAvailable;

    auto& magickCaps = engines::ImageMagickCapabilities::instance();
    auto& pandocCaps = engines::PandocCapabilities::instance();

    if (hasMagick && !magickCaps.isDiscovered()) {
        magickCaps.discover(toolchain.magick().executablePath);
    }
    if (hasPandoc && !pandocCaps.isDiscovered()) {
        pandocCaps.discover(toolchain.pandoc().executablePath);
    }

    for (const auto& fmt : formats) {
        DynamicFormatCapability cap;
        cap.definition = fmt;
        cap.readiness = FormatReadiness::Recognized;
        cap.isReadableOnHost = false;
        cap.isWritableOnHost = false;

        if (fmt.category == MediaCategory::Video || fmt.category == MediaCategory::Audio || fmt.category == MediaCategory::Subtitle) {
            cap.activeEngineName = "FFmpeg";
            if (hasFfmpeg) {
                cap.isReadableOnHost = fmt.canInput;
                cap.isWritableOnHost = fmt.canOutput;
                cap.readiness = (fmt.tier == CapabilityTier::TierA_VerifiedVertAndNative) ?
                                FormatReadiness::Verified : FormatReadiness::Available;
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
                    cap.readiness = (fmt.tier == CapabilityTier::TierA_VerifiedVertAndNative) ?
                                    FormatReadiness::Verified : FormatReadiness::Available;
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
                        cap.engineStatusDetail = QString("Engine: %1").arg(pandocCaps.preferredPdfEngine().name);
                    }
                } else {
                    cap.engineStatusDetail = QString("Pandoc %1").arg(pandocCaps.versionString().left(20));
                }

                if (cap.isReadableOnHost || cap.isWritableOnHost) {
                    cap.readiness = (fmt.tier == CapabilityTier::TierA_VerifiedVertAndNative) ?
                                    FormatReadiness::Verified : FormatReadiness::Available;
                }
            } else {
                cap.engineStatusDetail = "Requires Pandoc installation";
            }
        }

        m_cache.insert(fmt.id, cap);
    }
}

QList<DynamicFormatCapability> EngineCapabilityResolver::allResolvedCapabilities() const {
    return m_cache.values();
}

DynamicFormatCapability EngineCapabilityResolver::resolveFormat(const QString& formatId) const {
    const QString norm = FormatRegistry::instance().normalizeFormatId(formatId);
    return m_cache.value(norm);
}

int EngineCapabilityResolver::totalRecognizedCount() const {
    return m_cache.size();
}

int EngineCapabilityResolver::totalAvailableCount() const {
    int count = 0;
    for (const auto& cap : m_cache) {
        if (cap.readiness == FormatReadiness::Available || cap.readiness == FormatReadiness::Verified) {
            count++;
        }
    }
    return count;
}

int EngineCapabilityResolver::totalVerifiedCount() const {
    int count = 0;
    for (const auto& cap : m_cache) {
        if (cap.readiness == FormatReadiness::Verified) {
            count++;
        }
    }
    return count;
}

} // namespace uc::core
