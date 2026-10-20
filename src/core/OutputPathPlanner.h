#pragma once

#include <QString>
#include <QFileInfo>
#include <QDir>
#include "../app/AppSettings.h"

namespace uc::core {

enum class CollisionPolicy {
    GenerateUniqueName, // Append _1, _2, etc.
    Overwrite,          // Overwrite existing file
    Skip                // Do not convert, report error
};

struct PathPlanResult {
    bool isValid{false};
    QString targetDirectory;
    QString finalFilePath;
    QString errorMessage;
};

class OutputPathPlanner {
public:
    static PathPlanResult planOutputPath(const QString& inputFilePath,
                                         const QString& outputFormatId,
                                         app::DestinationMode mode,
                                         const QString& customDirectoryPath,
                                         CollisionPolicy policy = CollisionPolicy::GenerateUniqueName);

    static QString resolveUniquePath(const QString& targetFilePath);
    static QString generateTemporaryOutputPath(const QString& finalOutputPath);

    static void reservePath(const QString& path);
    static void releasePath(const QString& path);
    static void clearReservedPaths();

    // Atomically publish tempPath -> finalPath without data loss.
    // Never deletes an existing finalPath unless the new file is in place:
    // existing target is moved to a .bak sidecar first, then restored on failure.
    // Returns true on success; errorMessage describes the failure otherwise.
    static bool publishTempToFinal(const QString& tempPath, const QString& finalPath,
                                   QString* errorMessage = nullptr);
    static QString sanitizeExtension(const QString& rawExtension);
    // Unified non-empty-file validation. Centralizes the previously scattered
    // size floors (media 128B / docs 4B / images 16B) into one helper with an
    // explicit minimum so thresholds stay consistent.
    static bool validateProducedFile(const QString& path, qint64 minBytes,
                                     QString* errorMessage = nullptr);
};

} // namespace uc::core
