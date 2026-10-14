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
};

} // namespace uc::core
