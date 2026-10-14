#include "EngineResolver.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace uc::app {

QString EngineResolver::defaultAppDirPath() {
    return QCoreApplication::applicationDirPath();
}

QProcessEnvironment EngineResolver::cleanProcessEnvironment() {
    auto env = QProcessEnvironment::systemEnvironment();
    // In an AppImage, LD_LIBRARY_PATH contains the bundled libs which crash host binaries
    // like ffmpeg and imagemagick due to mismatched system font/image library ABIs.
    // If ORIGINAL_LD_LIBRARY_PATH was recorded by AppRun, restore it; otherwise remove LD_LIBRARY_PATH.
    if (env.contains("ORIGINAL_LD_LIBRARY_PATH")) {
        const QString orig = env.value("ORIGINAL_LD_LIBRARY_PATH");
        if (orig.isEmpty()) {
            env.remove("LD_LIBRARY_PATH");
        } else {
            env.insert("LD_LIBRARY_PATH", orig);
        }
    } else if (env.contains("APPIMAGE") || env.contains("APPDIR")) {
        env.remove("LD_LIBRARY_PATH");
    }
    return env;
}

ResolvedTool EngineResolver::resolve(const QString& toolName,
                                     const QString& overridePath,
                                     const QString& versionArg,
                                     const QString& baseDirPath,
                                     const QString& companionDir)
{
    ResolvedTool tool;
    tool.name = toolName;

    // 1. Explicit User Override (Highest Priority)
    if (!overridePath.isEmpty()) {
        const QFileInfo fi(overridePath);
        if (!fi.exists()) {
            tool.source = ToolSource::UserOverride;
            tool.isAvailable = false;
            tool.executablePath = overridePath;
            tool.failureReason = QString("Configured override path does not exist: %1").arg(overridePath);
            return tool;
        }
        if (!fi.isExecutable()) {
            tool.source = ToolSource::UserOverride;
            tool.isAvailable = false;
            tool.executablePath = overridePath;
            tool.failureReason = QString("Configured override file is not executable: %1").arg(overridePath);
            return tool;
        }

        // Test probe execution
        QProcess proc;
        proc.setProcessEnvironment(cleanProcessEnvironment());
        proc.start(overridePath, QStringList{versionArg});
        if (proc.waitForFinished(3000)) {
            if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
                tool.source = ToolSource::UserOverride;
                tool.isAvailable = true;
                tool.executablePath = overridePath;
                tool.version = QString::fromUtf8(proc.readAllStandardOutput()).split('\n').value(0).trimmed();
                return tool;
            } else {
                tool.source = ToolSource::UserOverride;
                tool.isAvailable = false;
                tool.executablePath = overridePath;
                tool.failureReason = QString("Execution failed with exit code %1").arg(proc.exitCode());
                return tool;
            }
        } else {
            tool.source = ToolSource::UserOverride;
            tool.isAvailable = false;
            tool.executablePath = overridePath;
            tool.failureReason = "Probe execution timed out (3s)";
            return tool;
        }
    }

    const QString appDir = baseDirPath.isEmpty() ? defaultAppDirPath() : baseDirPath;

    // 2. Bundled Engine (Relative to runtime app dir)
    QStringList bundledCandidates;

    // 2a. Matched companion directory (e.g. if FFmpeg was resolved from bundled, check its directory first for FFprobe)
    if (!companionDir.isEmpty()) {
        bundledCandidates << QDir(companionDir).filePath(toolName);
#ifdef Q_OS_WIN
        bundledCandidates << QDir(companionDir).filePath(toolName + ".exe");
        bundledCandidates << QDir(companionDir).filePath(toolName + ".bat");
        bundledCandidates << QDir(companionDir).filePath(toolName + ".cmd");
#endif
    }

    // 2b. Standard bundled layouts
    // Subdirectory layout: engines/<toolName>/<toolName>
    bundledCandidates << QDir(appDir).filePath("engines/" + toolName + "/" + toolName);
    // Dev build layout (from build/ to project root): ../engines/<toolName>/<toolName>
    bundledCandidates << QDir(appDir).filePath("../engines/" + toolName + "/" + toolName);
    // Flat bundled layout: tools/<toolName>
    bundledCandidates << QDir(appDir).filePath("tools/" + toolName);
    // AppDir usr/bin layout: ../usr/bin/<toolName>
    bundledCandidates << QDir(appDir).filePath(toolName);

#ifdef Q_OS_WIN
    for (const QString& ext : {".exe", ".bat", ".cmd"}) {
        bundledCandidates << QDir(appDir).filePath("engines/" + toolName + "/" + toolName + ext);
        bundledCandidates << QDir(appDir).filePath("../engines/" + toolName + "/" + toolName + ext);
        bundledCandidates << QDir(appDir).filePath("tools/" + toolName + ext);
        bundledCandidates << QDir(appDir).filePath(toolName + ext);
    }
#endif

    for (const auto& candidate : bundledCandidates) {
        const QFileInfo fi(candidate);
        if (fi.exists() && fi.isExecutable()) {
            QProcess proc;
            proc.start(candidate, QStringList{versionArg});
            if (proc.waitForFinished(3000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
                tool.source = ToolSource::Bundled;
                tool.isAvailable = true;
                tool.executablePath = candidate;
                tool.version = QString::fromUtf8(proc.readAllStandardOutput()).split('\n').value(0).trimmed();
                return tool;
            }
        }
    }

    // 3. System PATH fallback
    QString systemExe;
    if (!companionDir.isEmpty()) {
        const QString companionCandidate = QDir(companionDir).filePath(toolName);
        if (QFileInfo(companionCandidate).isExecutable()) {
            systemExe = companionCandidate;
        }
#ifdef Q_OS_WIN
        if (systemExe.isEmpty()) {
            for (const QString& ext : {".exe", ".bat", ".cmd"}) {
                const QString winCandidate = companionCandidate + ext;
                if (QFileInfo(winCandidate).isExecutable()) {
                    systemExe = winCandidate;
                    break;
                }
            }
        }
#endif
    }
    if (systemExe.isEmpty()) {
        systemExe = QStandardPaths::findExecutable(toolName);
    }

    if (!systemExe.isEmpty()) {
        QProcess proc;
        proc.setProcessEnvironment(cleanProcessEnvironment());
        proc.start(systemExe, QStringList{versionArg});
        if (proc.waitForFinished(3000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            tool.source = ToolSource::SystemPath;
            tool.isAvailable = true;
            tool.executablePath = systemExe;
            tool.version = QString::fromUtf8(proc.readAllStandardOutput()).split('\n').value(0).trimmed();
            return tool;
        } else {
            tool.source = ToolSource::SystemPath;
            tool.isAvailable = false;
            tool.executablePath = systemExe;
            tool.failureReason = QString("System executable failed version probe (%1)").arg(systemExe);
            return tool;
        }
    }

    // 4. Missing
    tool.source = ToolSource::None;
    tool.isAvailable = false;
    tool.failureReason = QString("Executable '%1' was not found in user overrides, bundled directories, or system PATH.").arg(toolName);
    return tool;
}

} // namespace uc::app
