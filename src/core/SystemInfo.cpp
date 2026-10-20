#include "SystemInfo.h"
#include "../app/ToolchainManager.h"
#include "ProcessRunner.h"
#include <QThread>
#include <QProcess>
#include <QFileInfo>
#include <QDateTime>
#include <QMutex>
#include <QSet>

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
    proc.setProcessEnvironment(ProcessRunner::cleanEnvironment());
    proc.start(ffmpeg.executablePath, QStringList{"-hide_banner", "-encoders"});
    if (proc.waitForFinished(3000)) {
        const QString out = QString::fromUtf8(proc.readAllStandardOutput());
        const QStringList lines = out.split('\n');
        QSet<QString> seen;
        QStringList candidateEncoders;
        for (const QString& line : lines) {
            // Single pass over known encoder tokens.
            static const char* kTokens[] = {"h264_nvenc", "hevc_nvenc", "av1_nvenc",
                                            "h264_vaapi", "hevc_vaapi", "h264_qsv", "hevc_qsv"};
            for (const char* tok : kTokens) {
                const QString t = QString::fromLatin1(tok);
                if (line.contains(t) && !seen.contains(t)) {
                    seen.insert(t);
                    candidateEncoders << t;
                }
            }
        }

        // Test each candidate with a 1-frame probe to verify real GPU driver / library availability
        // (e.g. ffmpeg might compile nvenc support, but libcuda.so or Nvidia GPU is absent in headless CI)
        // Result is cached globally (path+mtime key) so this runs at most once per binary.
        for (const auto& enc : candidateEncoders) {
            QStringList probeArgs;
            probeArgs << "-hide_banner" << "-loglevel" << "error";

            if (enc.contains("vaapi")) {
                QString dev = "/dev/dri/renderD128";
                if (!QFile::exists(dev)) dev = "/dev/dri/card0";
                if (!QFile::exists(dev)) {
                    continue; // No DRM render node or device available
                }
                probeArgs << "-vaapi_device" << dev;
                probeArgs << "-f" << "lavfi" << "-i" << "color=c=red:s=160x120:d=0.04";
                probeArgs << "-vf" << "format=nv12,hwupload";
            } else if (enc.contains("qsv")) {
                probeArgs << "-init_hw_device" << "qsv=hw" << "-filter_hw_device" << "hw";
                probeArgs << "-f" << "lavfi" << "-i" << "color=c=red:s=160x120:d=0.04";
                probeArgs << "-vf" << "hwupload=extra_hw_frames=64,format=qsv";
            } else {
                probeArgs << "-f" << "lavfi" << "-i" << "color=c=red:s=160x120:d=0.04";
            }

            probeArgs << "-c:v" << enc << "-f" << "null" << "-";

            QProcess testProc;
            testProc.setProcessEnvironment(ProcessRunner::cleanEnvironment());
            testProc.start(ffmpeg.executablePath, probeArgs);
            const bool done = testProc.waitForFinished(2000);
            if (!done) {
                testProc.kill();
                testProc.waitForFinished(1000);
                continue;
            }
            if (testProc.exitStatus() == QProcess::NormalExit && testProc.exitCode() == 0) {
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
