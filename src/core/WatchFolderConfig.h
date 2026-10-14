#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

namespace uc::core {

enum class WatchPostAction {
    KeepOriginal,
    MoveToProcessed,
    DeleteOriginal
};

struct WatchFolderConfig {
    QString id;
    QString folderPath;
    QString targetFormatId;
    QString presetId;
    WatchPostAction postAction{WatchPostAction::MoveToProcessed};
    QString destinationFolder; // If empty, uses folderPath + "/converted"
    bool isRecursive{false};
    bool isEnabled{true};
    int debounceSeconds{2};

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["folderPath"] = folderPath;
        obj["targetFormatId"] = targetFormatId;
        obj["presetId"] = presetId;
        obj["postAction"] = static_cast<int>(postAction);
        obj["destinationFolder"] = destinationFolder;
        obj["isRecursive"] = isRecursive;
        obj["isEnabled"] = isEnabled;
        obj["debounceSeconds"] = debounceSeconds;
        return obj;
    }

    static WatchFolderConfig fromJson(const QJsonObject& obj) {
        WatchFolderConfig cfg;
        cfg.id = obj["id"].toString();
        cfg.folderPath = obj["folderPath"].toString();
        cfg.targetFormatId = obj["targetFormatId"].toString();
        cfg.presetId = obj["presetId"].toString();
        cfg.postAction = static_cast<WatchPostAction>(obj["postAction"].toInt(1));
        cfg.destinationFolder = obj["destinationFolder"].toString();
        cfg.isRecursive = obj["isRecursive"].toBool(false);
        cfg.isEnabled = obj["isEnabled"].toBool(true);
        cfg.debounceSeconds = obj["debounceSeconds"].toInt(2);
        return cfg;
    }
};

} // namespace uc::core
