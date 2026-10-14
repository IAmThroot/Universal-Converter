#pragma once

#include <QString>
#include <QStringList>
#include <QProcessEnvironment>

namespace uc::app {

enum class ToolSource {
    UserOverride,
    Bundled,
    SystemPath,
    None
};

struct ResolvedTool {
    QString name;
    QString executablePath;
    ToolSource source{ToolSource::None};
    bool isAvailable{false};
    QString version;
    QString failureReason;

    QString sourceDescription() const {
        switch (source) {
            case ToolSource::UserOverride: return "User Override";
            case ToolSource::Bundled: return "Bundled";
            case ToolSource::SystemPath: return "System PATH";
            case ToolSource::None: default: return "Not Found";
        }
    }
};

class EngineResolver {
public:
    static ResolvedTool resolve(const QString& toolName,
                                const QString& overridePath,
                                const QString& versionArg,
                                const QString& baseDirPath = QString(),
                                const QString& companionDir = QString());

    static QString defaultAppDirPath();
    static QProcessEnvironment cleanProcessEnvironment();
};

} // namespace uc::app
