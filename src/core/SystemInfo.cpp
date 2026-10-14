#include "SystemInfo.h"
#include "../app/ToolchainManager.h"
#include <QThread>
#include <QProcess>
#include <QFileInfo>
#include <QDateTime>
#include <QMutex>

namespace uc::core {

static SystemCapabilities s_cachedCaps;
static QString s_cachedPath;
static QDateTime s_cachedMtime;
static bool s_hasCached{false};
static QMutex s_cacheMutex;

SystemCapabilities SystemInfo::probe() {
    const auto ffmpeg = app::ToolchainManager::instance().ffmpeg();
    if (!ffmpeg.isAvailable) {
        SystemCapabilities caps;
        caps.cpuThreadCount = QThread::idealThreadCount();
        return caps;
    }

    QFileInfo fi(ffmpeg.executablePath);
    const QDateTime currentMtime = fi.lastModified();

    {
        QMutexLocker locker(&s_cacheMutex);
        if (s_hasCached && s_cachedPath == ffmpeg.executablePath && s_cachedMtime == currentMtime) {
            return s_cachedCaps;
        }
    }

    SystemCapabilities caps;
    caps.cpuThreadCount = QThread::idealThreadCount();

    QProcess proc;
    proc.start(ffmpeg.executablePath, QStringList{"-hide_banner", "-encoders"});
    if (proc.waitForFinished(3000)) {
        const QString out = QString::fromUtf8(proc.readAllStandardOutput());
        const QStringList lines = out.split('\n');
        QStringList candidateEncoders;
        for (const QString& line : lines) {
            if (line.contains("h264_nvenc")) candidateEncoders << "h264_nvenc";
            if (line.contains("hevc_nvenc")) candidateEncoders << "hevc_nvenc";
            if (line.contains("av1_nvenc")) candidateEncoders << "av1_nvenc";
            if (line.contains("h264_vaapi")) candidateEncoders << "h264_vaapi";
            if (line.contains("hevc_vaapi")) candidateEncoders << "hevc_vaapi";
            if (line.contains("h264_qsv")) candidateEncoders << "h264_qsv";
            if (line.contains("hevc_qsv")) candidateEncoders << "hevc_qsv";
        }

        // Test each candidate with a 1-frame probe to verify real GPU driver / library availability
        // (e.g. ffmpeg might compile nvenc support, but libcuda.so or Nvidia GPU is absent in headless CI)
        for (const auto& enc : candidateEncoders) {
            QProcess testProc;
            testProc.start(ffmpeg.executablePath, {
                "-hide_banner", "-loglevel", "error",
                "-f", "lavfi", "-i", "color=c=red:s=160x120:d=0.04",
                "-c:v", enc,
                "-f", "null", "-"
            });
            if (testProc.waitForFinished(2000) && testProc.exitStatus() == QProcess::NormalExit && testProc.exitCode() == 0) {
                caps.availableHwEncoders << enc;
                if (enc.contains("nvenc")) {
                    caps.hasNvenc = true;
                    caps.hasNvidiaGpu = true;
                }
                if (enc.contains("vaapi")) caps.hasVaapi = true;
                if (enc.contains("qsv")) caps.hasQsv = true;
            }
        }
    }

    {
        QMutexLocker locker(&s_cacheMutex);
        s_cachedCaps = caps;
        s_cachedPath = ffmpeg.executablePath;
        s_cachedMtime = currentMtime;
        s_hasCached = true;
    }

    return caps;
}

} // namespace uc::core
