#include "OutputPathPlanner.h"
#include "FormatRegistry.h"
#include <QRegularExpression>

#include <QUuid>

namespace uc::core {

QString OutputPathPlanner::resolveUniquePath(const QString& targetFilePath) {
    if (!QFile::exists(targetFilePath)) {
        return targetFilePath;
    }

    const QFileInfo fi(targetFilePath);
    const QString dir = fi.absolutePath();
    const QString baseName = fi.completeBaseName();
    const QString ext = fi.suffix();

    constexpr int maxDisambiguationAttempts = 10000;
    for (int counter = 1; counter <= maxDisambiguationAttempts; ++counter) {
        const QString candidate = QString("%1/%2_%3%4")
                                      .arg(dir, baseName, QString::number(counter), ext.isEmpty() ? "" : "." + ext);
        if (!QFile::exists(candidate)) {
            return candidate;
        }
    }

    // Fallback using random UUID suffix if filesystem collision persists
    return QString("%1/%2_%3%4")
        .arg(dir, baseName, QUuid::createUuid().toString(QUuid::WithoutBraces).left(8), ext.isEmpty() ? "" : "." + ext);
}

QString OutputPathPlanner::generateTemporaryOutputPath(const QString& finalOutputPath) {
    const QFileInfo fi(finalOutputPath);
    const QString tempFileName = QString(".tmp_%1_%2.%3")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8),
                                          fi.completeBaseName(),
                                          fi.suffix());
    return QDir(fi.absolutePath()).filePath(tempFileName);
}

PathPlanResult OutputPathPlanner::planOutputPath(const QString& inputFilePath,
                                                 const QString& outputFormatId,
                                                 app::DestinationMode mode,
                                                 const QString& customDirectoryPath,
                                                 CollisionPolicy policy)
{
    PathPlanResult res;

    if (inputFilePath.trimmed().isEmpty()) {
        res.errorMessage = "Input file path is empty.";
        return res;
    }

    const QFileInfo inputInfo(inputFilePath);
    if (!inputInfo.exists() || !inputInfo.isFile()) {
        res.errorMessage = QString("Source file does not exist: %1").arg(inputFilePath);
        return res;
    }

    // Determine target directory
    QString targetDir;
    switch (mode) {
        case app::DestinationMode::SameAsSource:
            targetDir = inputInfo.absolutePath();
            break;

        case app::DestinationMode::SourceConvertedSubfolder:
            targetDir = QDir(inputInfo.absolutePath()).filePath("Converted");
            break;

        case app::DestinationMode::CustomFolder:
            if (customDirectoryPath.trimmed().isEmpty()) {
                res.errorMessage = "Custom destination directory is not specified.";
                return res;
            }
            targetDir = customDirectoryPath;
            break;
    }

    // Ensure target directory exists or can be created
    QDir dir(targetDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            res.errorMessage = QString("Failed to create destination directory: %1").arg(targetDir);
            return res;
        }
    }

    res.targetDirectory = dir.absolutePath();

    // Determine target extension
    const auto* fmt = FormatRegistry::instance().findById(outputFormatId);
    const QString ext = fmt ? fmt->defaultExtension : outputFormatId;

    // Build base target filename
    const QString targetFileName = QString("%1.%2").arg(inputInfo.completeBaseName(), ext);
    const QString proposedPath = QDir(res.targetDirectory).filePath(targetFileName);

    // Guard against source and destination being the exact same file
    const bool isSameAsSource = (QFileInfo(proposedPath).absoluteFilePath() == inputInfo.absoluteFilePath());

    if (policy == CollisionPolicy::Skip && QFile::exists(proposedPath)) {
        res.errorMessage = QString("Destination file already exists and policy is Skip: %1").arg(proposedPath);
        return res;
    }

    if (policy == CollisionPolicy::Overwrite) {
        if (isSameAsSource) {
            res.errorMessage = "Source and target path are identical; in-place overwriting is prohibited.";
            return res;
        }
        res.finalFilePath = proposedPath;
    } else { // GenerateUniqueName
        if (isSameAsSource || QFile::exists(proposedPath)) {
            res.finalFilePath = resolveUniquePath(proposedPath);
        } else {
            res.finalFilePath = proposedPath;
        }
    }

    res.isValid = true;
    return res;
}

} // namespace uc::core
