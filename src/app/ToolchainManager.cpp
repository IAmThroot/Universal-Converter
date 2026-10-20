#include "ToolchainManager.h"
#include "AppSettings.h"
#include "../core/SystemInfo.h"
#include "../engines/ImageMagickCapabilities.h"
#include "../engines/PandocCapabilities.h"
#include <QFileInfo>
#include <QThreadPool>

namespace uc::app {

ToolchainManager& ToolchainManager::instance() {
    static ToolchainManager s_instance;
    return s_instance;
}

ToolchainManager::ToolchainManager(QObject* parent)
    : QObject(parent)
{
}

void ToolchainManager::scanAllAsync() {
    {
        QMutexLocker locker(&m_mutex);
        m_isScanning = true;
    }
    emit toolchainStatusChanged();

    const auto& settings = AppSettings::instance();
    QString ffmpeg = settings.ffmpegPathOverride();
    QString magick = settings.magickPathOverride();
    QString pandoc = settings.pandocPathOverride();

    QThreadPool::globalInstance()->start([this, ffmpeg, magick, pandoc]() {
        scanAll(ffmpeg, magick, pandoc);
    });
}

void ToolchainManager::scanAll() {
    const auto& settings = AppSettings::instance();
    scanAll(settings.ffmpegPathOverride(), settings.magickPathOverride(), settings.pandocPathOverride());
}

void ToolchainManager::scanAll(const QString& ffmpegOverride, const QString& magickOverride, const QString& pandocOverride) {
    // 1. Resolve FFmpeg
    auto ffmpeg = EngineResolver::resolve("ffmpeg", ffmpegOverride, "-version");

    // 2. Resolve FFprobe using FFmpeg directory as companion search path to prevent mismatched pairings
    QString ffmpegDir;
    if (!ffmpeg.executablePath.isEmpty()) {
        ffmpegDir = QFileInfo(ffmpeg.executablePath).absolutePath();
    }
    auto ffprobe = EngineResolver::resolve("ffprobe", QString(), "-version", QString(), ffmpegDir);

    // 3. Resolve ImageMagick (check 'magick' first, fallback to 'convert')
    auto magick = EngineResolver::resolve("magick", magickOverride, "-version");
    if (!magick.isAvailable && magickOverride.isEmpty()) {
        auto convertTool = EngineResolver::resolve("convert", QString(), "-version");
        if (convertTool.isAvailable) {
            magick = convertTool;
            magick.name = "magick";
        }
    }

    // 4. Resolve Pandoc
    auto pandoc = EngineResolver::resolve("pandoc", pandocOverride, "--version");

    // 5. Phase 4: Modern Image & Vector Optimization Tools
    auto rsvgConvert = EngineResolver::resolve("rsvg-convert", QString(), "--version");
    auto avifenc = EngineResolver::resolve("avifenc", QString(), "--version");
    auto cwebp = EngineResolver::resolve("cwebp", QString(), "-version");
    auto oxipng = EngineResolver::resolve("oxipng", QString(), "--version");
    auto pngquant = EngineResolver::resolve("pngquant", QString(), "--version");

    {
        QMutexLocker locker(&m_mutex);
        m_ffmpeg = ffmpeg;
        m_ffprobe = ffprobe;
        m_magick = magick;
        m_pandoc = pandoc;
        m_rsvgConvert = rsvgConvert;
        m_avifenc = avifenc;
        m_cwebp = cwebp;
        m_oxipng = oxipng;
        m_pngquant = pngquant;
        m_isScanning = false;
    }

    // Warm capabilities probes while on worker thread
    core::SystemInfo::probe();
    if (magick.isAvailable) {
        engines::ImageMagickCapabilities::instance().discover(magick.executablePath);
    }
    if (pandoc.isAvailable) {
        engines::PandocCapabilities::instance().discover(pandoc.executablePath);
    }

    QMetaObject::invokeMethod(this, &ToolchainManager::toolchainStatusChanged, Qt::QueuedConnection);
}

} // namespace uc::app
