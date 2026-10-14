#pragma once

#include <QString>
#include <QObject>
#include "EngineResolver.h"

namespace uc::app {

using ToolInfo = ResolvedTool;

class ToolchainManager : public QObject {
    Q_OBJECT

public:
    static ToolchainManager& instance();

    void scanAll();

    ToolInfo ffmpeg() const { return m_ffmpeg; }
    ToolInfo ffprobe() const { return m_ffprobe; }
    ToolInfo magick() const { return m_magick; }
    ToolInfo pandoc() const { return m_pandoc; }

    // Phase 4: Modern Image & Vector Optimization Tools
    ToolInfo rsvgConvert() const { return m_rsvgConvert; }
    ToolInfo avifenc() const { return m_avifenc; }
    ToolInfo cwebp() const { return m_cwebp; }
    ToolInfo oxipng() const { return m_oxipng; }
    ToolInfo pngquant() const { return m_pngquant; }

signals:
    void toolchainStatusChanged();

private:
    ToolchainManager(QObject* parent = nullptr);
    ~ToolchainManager() = default;

    ToolInfo m_ffmpeg;
    ToolInfo m_ffprobe;
    ToolInfo m_magick;
    ToolInfo m_pandoc;
    ToolInfo m_rsvgConvert;
    ToolInfo m_avifenc;
    ToolInfo m_cwebp;
    ToolInfo m_oxipng;
    ToolInfo m_pngquant;
};

} // namespace uc::app
