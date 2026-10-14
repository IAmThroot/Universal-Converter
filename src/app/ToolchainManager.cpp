#include "ToolchainManager.h"
#include "AppSettings.h"
#include <QFileInfo>

namespace uc::app {

ToolchainManager& ToolchainManager::instance() {
    static ToolchainManager s_instance;
    return s_instance;
}

ToolchainManager::ToolchainManager(QObject* parent)
    : QObject(parent)
{
}

void ToolchainManager::scanAll() {
    const auto& settings = AppSettings::instance();

    // 1. Resolve FFmpeg
    m_ffmpeg = EngineResolver::resolve("ffmpeg", settings.ffmpegPathOverride(), "-version");

    // 2. Resolve FFprobe using FFmpeg directory as companion search path to prevent mismatched pairings
    QString ffmpegDir;
    if (!m_ffmpeg.executablePath.isEmpty()) {
        ffmpegDir = QFileInfo(m_ffmpeg.executablePath).absolutePath();
    }
    m_ffprobe = EngineResolver::resolve("ffprobe", QString(), "-version", QString(), ffmpegDir);

    // 3. Resolve ImageMagick (check 'magick' first, fallback to 'convert')
    m_magick = EngineResolver::resolve("magick", settings.magickPathOverride(), "-version");
    if (!m_magick.isAvailable && settings.magickPathOverride().isEmpty()) {
        auto convertTool = EngineResolver::resolve("convert", QString(), "-version");
        if (convertTool.isAvailable) {
            m_magick = convertTool;
            m_magick.name = "magick";
        }
    }

    // 4. Resolve Pandoc
    m_pandoc = EngineResolver::resolve("pandoc", settings.pandocPathOverride(), "--version");

    // 5. Phase 4: Modern Image & Vector Optimization Tools
    m_rsvgConvert = EngineResolver::resolve("rsvg-convert", QString(), "--version");
    m_avifenc = EngineResolver::resolve("avifenc", QString(), "--version");
    m_cwebp = EngineResolver::resolve("cwebp", QString(), "-version");
    m_oxipng = EngineResolver::resolve("oxipng", QString(), "--version");
    m_pngquant = EngineResolver::resolve("pngquant", QString(), "--version");

    emit toolchainStatusChanged();
}

} // namespace uc::app
