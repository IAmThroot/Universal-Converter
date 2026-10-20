#pragma once

#include <QString>
#include <QObject>
#include <QMutex>
#include "EngineResolver.h"

namespace uc::app {

using ToolInfo = ResolvedTool;

class ToolchainManager : public QObject {
    Q_OBJECT

public:
    static ToolchainManager& instance();

    void scanAll();
    void scanAll(const QString& ffmpegOverride, const QString& magickOverride, const QString& pandocOverride);
    void scanAllAsync();

    bool isScanning() const { QMutexLocker locker(&m_mutex); return m_isScanning; }

    ToolInfo ffmpeg() const { QMutexLocker locker(&m_mutex); return m_ffmpeg; }
    ToolInfo ffprobe() const { QMutexLocker locker(&m_mutex); return m_ffprobe; }
    ToolInfo magick() const { QMutexLocker locker(&m_mutex); return m_magick; }
    ToolInfo pandoc() const { QMutexLocker locker(&m_mutex); return m_pandoc; }

    // Phase 4: Modern Image & Vector Optimization Tools
    ToolInfo rsvgConvert() const { QMutexLocker locker(&m_mutex); return m_rsvgConvert; }
    ToolInfo avifenc() const { QMutexLocker locker(&m_mutex); return m_avifenc; }
    ToolInfo cwebp() const { QMutexLocker locker(&m_mutex); return m_cwebp; }
    ToolInfo oxipng() const { QMutexLocker locker(&m_mutex); return m_oxipng; }
    ToolInfo pngquant() const { QMutexLocker locker(&m_mutex); return m_pngquant; }

signals:
    void toolchainStatusChanged();

private:
    ToolchainManager(QObject* parent = nullptr);
    ~ToolchainManager() = default;

    mutable QMutex m_mutex;
    ToolInfo m_ffmpeg;
    ToolInfo m_ffprobe;
    ToolInfo m_magick;
    ToolInfo m_pandoc;
    ToolInfo m_rsvgConvert;
    ToolInfo m_avifenc;
    ToolInfo m_cwebp;
    ToolInfo m_oxipng;
    ToolInfo m_pngquant;
    bool m_isScanning{false};
};

} // namespace uc::app
