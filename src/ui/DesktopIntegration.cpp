#include "DesktopIntegration.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QPointer>
#include <QTimer>
namespace uc::ui {
QProcessEnvironment DesktopIntegration::hostEnvironment(QProcessEnvironment env) {
    const bool bundled = env.contains("APPIMAGE") || env.contains("APPDIR") || env.contains("UC_APPDIR") ||
                         env.contains("ORIGINAL_LD_LIBRARY_PATH");
    if (!bundled)
        return env;
    auto restore = [&env](const QString &variable, const QString &backup) {
        if (env.contains(backup) && !env.value(backup).isEmpty())
            env.insert(variable, env.value(backup));
        else
            env.remove(variable);
    };
    restore("LD_LIBRARY_PATH", "ORIGINAL_LD_LIBRARY_PATH");
    restore("QT_PLUGIN_PATH", "UC_ORIGINAL_QT_PLUGIN_PATH");
    restore("QML2_IMPORT_PATH", "UC_ORIGINAL_QML2_IMPORT_PATH");
    // These may also point into an AppImage runtime or its Qt installation.
    env.remove("QT_QPA_PLATFORM_PLUGIN_PATH");
    env.remove("QT_QPA_FONTDIR");
    env.remove("QML_IMPORT_PATH");
    if (env.contains("UC_ORIGINAL_PATH"))
        env.insert("PATH", env.value("UC_ORIGINAL_PATH"));
    else {
        QStringList paths;
        for (const auto &path : env.value("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts)) {
            const QString clean = QDir::cleanPath(path);
            const QString root = env.value("UC_APPDIR", env.value("APPDIR"));
            if (clean == QCoreApplication::applicationDirPath())
                continue;
            if (!root.isEmpty() && (clean == root || clean.startsWith(root + '/')))
                continue;
            paths << path;
        }
        env.insert("PATH", paths.join(QDir::listSeparator()));
    }
    // Host applications must not mistake themselves for this AppImage.
    for (const auto &name :
         {"APPIMAGE", "APPDIR", "UC_APPDIR", "UC_ORIGINAL_PATH", "UC_ORIGINAL_QT_PLUGIN_PATH",
          "UC_ORIGINAL_QML2_IMPORT_PATH", "ORIGINAL_LD_LIBRARY_PATH"})
        env.remove(name);
    return env;
}

void DesktopIntegration::openLocalPath(QWidget *parent, const QString &path) {
    const QFileInfo target(path);
    if (path.trimmed().isEmpty() || !target.exists()) {
        QMessageBox::warning(parent, "Cannot open output", "The file or folder no longer exists:\n" + path);
        return;
    }
    const QUrl url = QUrl::fromLocalFile(target.absoluteFilePath());
#ifdef Q_OS_LINUX
    const auto env = hostEnvironment(QProcessEnvironment::systemEnvironment());
    const auto paths = env.value("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
    // GIO launches the associated application independently and reports dispatch
    // errors, so closing this app cannot terminate the viewer or file manager.
    const QString gio = paths.isEmpty() ? QString() : QStandardPaths::findExecutable("gio", paths);
    if (!gio.isEmpty()) {
        auto *process = new QProcess(QCoreApplication::instance());
        process->setProcessEnvironment(env);
        process->setProgram(gio);
        process->setArguments({"open", url.toString(QUrl::FullyEncoded)});
        const QPointer<QWidget> owner(parent);
        auto report = [owner, path](const QString &reason) {
            if (!owner)
                return;
            QMessageBox::warning(owner, "Cannot open output",
                                 "The desktop could not open:\n" + path + "\n\n" + reason);
        };
        auto *timer = new QTimer(process);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, process, [process]() { process->kill(); });
        QObject::connect(process, &QProcess::errorOccurred, process,
                         [process, report](QProcess::ProcessError error) {
                             if (error == QProcess::FailedToStart) {
                                 process->deleteLater();
                                 report(process->errorString());
                             }
                         });
        QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
                         [process, timer, report](int code, QProcess::ExitStatus status) {
                             timer->stop();
                             process->deleteLater();
                             if (code != 0 || status != QProcess::NormalExit) {
                                 const QString error =
                                     QString::fromUtf8(process->readAllStandardError()).trimmed();
                                 report(error.isEmpty() ? "The desktop launcher failed or timed out. Check "
                                                          "the default application for this file type."
                                                        : error.left(2000));
                             }
                         });
        timer->start(15000);
        process->start();
        return;
    }
    // xdg-open remains available on desktops without GIO.
    const QString xdg = paths.isEmpty() ? QString() : QStandardPaths::findExecutable("xdg-open", paths);
    QProcess process;
    process.setProcessEnvironment(env);
    process.setProgram(xdg);
    process.setArguments({url.toString(QUrl::FullyEncoded)});
    if (!xdg.isEmpty() && process.startDetached())
        return;
    QMessageBox::warning(parent, "Cannot open output",
                         "Could not start a desktop launcher. Install xdg-utils or GIO and configure a "
                         "default application.\n\n" +
                             path);
#else
    if (!QDesktopServices::openUrl(url))
        QMessageBox::warning(parent, "Cannot open output",
                             "The system could not open this path. Check its default application:\n" + path);
#endif
}
} // namespace uc::ui
