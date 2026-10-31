#include "ToolchainManager.h"
#include "AppSettings.h"
#include "../core/SystemInfo.h"
#include "../engines/ImageMagickCapabilities.h"
#include "../engines/PandocCapabilities.h"
#include <QFileInfo>
#include <QThreadPool>
#include <QThread>

namespace uc::app {

ToolchainManager &ToolchainManager::instance() {
    static ToolchainManager s_instance;
    return s_instance;
}

ToolchainManager::ToolchainManager(QObject *parent) : QObject(parent) { m_scanPool.setMaxThreadCount(1); }

void ToolchainManager::scanAllAsync() {
    {
        QMutexLocker locker(&m_mutex);
        m_isScanning = true;
        ++m_pendingScans;
    }
    emit toolchainStatusChanged();

    const auto &settings = AppSettings::instance();
    QString ffmpeg = settings.ffmpegPathOverride();
    QString magick = settings.magickPathOverride();
    QString pandoc = settings.pandocPathOverride();

    m_scanPool.start([this, ffmpeg, magick, pandoc]() { scanAll(ffmpeg, magick, pandoc); });
}

void ToolchainManager::scanAll() {
    const auto &settings = AppSettings::instance();
    scanAll(settings.ffmpegPathOverride(), settings.magickPathOverride(), settings.pandocPathOverride());
}

void ToolchainManager::scanAll(const QString &ffmpegOverride, const QString &magickOverride,
                               const QString &pandocOverride) {
    QMutexLocker scanLock(&m_scanMutex);
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

    engines::ImageMagickCapabilities imageCaps;
    engines::PandocCapabilities documentCaps;
    if (magick.isAvailable)
        imageCaps.discover(magick.executablePath);
    if (pandoc.isAvailable)
        documentCaps.discover(pandoc.executablePath);
    auto publish = [this, ffmpeg, ffprobe, magick, pandoc, rsvgConvert, avifenc, cwebp, oxipng, pngquant,
                    imageCaps, documentCaps]() {
        {
            QMutexLocker locker(&m_mutex);
            // Queued scans run in order; publish only the last requested snapshot.
            if (m_pendingScans > 1) {
                --m_pendingScans;
                return;
            }
            m_ffmpeg = ffmpeg;
            m_ffprobe = ffprobe;
            m_magick = magick;
            m_pandoc = pandoc;
            m_rsvgConvert = rsvgConvert;
            m_avifenc = avifenc;
            m_cwebp = cwebp;
            m_oxipng = oxipng;
            m_pngquant = pngquant;
            if (m_pendingScans > 0)
                --m_pendingScans;
            m_isScanning = false;
        }
        engines::ImageMagickCapabilities::instance() = imageCaps;
        engines::PandocCapabilities::instance() = documentCaps;
        emit toolchainStatusChanged();
    };
    if (QThread::currentThread() == thread())
        publish();
    else
        QMetaObject::invokeMethod(this, publish, Qt::QueuedConnection);
}

} // namespace uc::app
