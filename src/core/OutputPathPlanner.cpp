#include "OutputPathPlanner.h"
#include "FormatRegistry.h"
#include <QRegularExpression>
#include <QUuid>
#include <QMutex>
#include <QSet>

namespace uc::core {

static QMutex s_reservedMutex;
static QSet<QString> s_reservedPaths;

void OutputPathPlanner::reservePath(const QString& path) {
    if (path.isEmpty()) return;
    QMutexLocker locker(&s_reservedMutex);
    s_reservedPaths.insert(path);
}

void OutputPathPlanner::releasePath(const QString& path) {
    if (path.isEmpty()) return;
    QMutexLocker locker(&s_reservedMutex);
    s_reservedPaths.remove(path);
}

void OutputPathPlanner::clearReservedPaths() {
    QMutexLocker locker(&s_reservedMutex);
    s_reservedPaths.clear();
}

QString OutputPathPlanner::resolveUniquePath(const QString& targetFilePath) {
    {
        QMutexLocker locker(&s_reservedMutex);
        if (!QFile::exists(targetFilePath) && !s_reservedPaths.contains(targetFilePath)) {
            return targetFilePath;
        }
    }

    const QFileInfo fi(targetFilePath);
    const QString dir = fi.absolutePath();
    const QString baseName = fi.completeBaseName();
    const QString ext = fi.suffix();

    constexpr int maxDisambiguationAttempts = 10000;
    for (int counter = 1; counter <= maxDisambiguationAttempts; ++counter) {
        const QString candidate = QString("%1/%2_%3%4")
                                      .arg(dir, baseName, QString::number(counter), ext.isEmpty() ? "" : "." + ext);
        QMutexLocker locker(&s_reservedMutex);
        if (!QFile::exists(candidate) && !s_reservedPaths.contains(candidate)) {
            return candidate;
        }
    }

    // Fallback using random UUID suffix if filesystem collision persists
    return QString("%1/%2_%3%4")
        .arg(dir, baseName, QUuid::createUuid().toString(QUuid::WithoutBraces).left(8), ext.isEmpty() ? "" : "." + ext);
}

QString OutputPathPlanner::generateTemporaryOutputPath(const QString& finalOutputPath) {
    if (finalOutputPath.trimmed().isEmpty()) {
        return QString();
    }
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

    // Determine target extension (sanitized against path traversal)
    const auto* fmt = FormatRegistry::instance().findById(outputFormatId);
    const QString rawExt = fmt ? fmt->defaultExtension : outputFormatId;
    const QString ext = sanitizeExtension(rawExt);

    // Build base target filename
    const QString targetFileName = QString("%1.%2").arg(inputInfo.completeBaseName(), ext);
    const QString proposedPath = QDir(res.targetDirectory).filePath(targetFileName);

    // Guard against source and destination being the exact same file.
    // Uses canonical paths (symlink-aware) + case-insensitive compare on
    // Windows/macOS where the filesystem is case-insensitive.
    const QFileInfo proposedInfo(proposedPath);
    const QString proposedCanon = proposedInfo.canonicalFilePath().isEmpty()
                                      ? proposedInfo.absoluteFilePath()
                                      : proposedInfo.canonicalFilePath();
    const QString sourceCanon = inputInfo.canonicalFilePath().isEmpty()
                                    ? inputInfo.absoluteFilePath()
                                    : inputInfo.canonicalFilePath();
#ifdef Q_OS_WIN
    const bool isSameAsSource = (proposedCanon.compare(sourceCanon, Qt::CaseInsensitive) == 0);
#else
    // macOS default APFS is case-insensitive; treat darwin like Windows.
#ifdef Q_OS_MACOS
    const bool isSameAsSource = (proposedCanon.compare(sourceCanon, Qt::CaseInsensitive) == 0);
#else
    const bool isSameAsSource = (proposedCanon == sourceCanon);
#endif
#endif

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
        res.finalFilePath = resolveUniquePath(proposedPath);
        reservePath(res.finalFilePath);
    }

    res.isValid = true;
    return res;
}

bool OutputPathPlanner::publishTempToFinal(const QString& tempPath, const QString& finalPath,
                                           QString* errorMessage) {
    if (tempPath.isEmpty() || finalPath.isEmpty()) {
        releasePath(finalPath);
        if (errorMessage) *errorMessage = "Temporary or final output path is empty.";
        return false;
    }
    if (!QFile::exists(tempPath)) {
        releasePath(finalPath);
        if (errorMessage) *errorMessage = QString("Temporary file missing: %1").arg(tempPath);
        return false;
    }

    // Target exists or was created concurrently: move it aside first.
    const QString backupPath = finalPath + QString(".bak_%1")
                                   .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    bool hadExisting = false;
    if (QFile::exists(finalPath)) {
        hadExisting = true;
        if (!QFile::rename(finalPath, backupPath)) {
            releasePath(finalPath);
            if (errorMessage) {
                *errorMessage = QString("Failed to back up existing file prior to publish: %1").arg(finalPath);
            }
            return false;
        }
    }

    if (QFile::rename(tempPath, finalPath)) {
        if (hadExisting && QFile::exists(backupPath)) {
            QFile::remove(backupPath);
        }
        releasePath(finalPath);
        return true;
    }

    // Restore original on failure.
    if (hadExisting && QFile::exists(backupPath)) {
        QFile::rename(backupPath, finalPath);
    }
    QFile::remove(tempPath);
    releasePath(finalPath);
    if (errorMessage) {
        *errorMessage = QString("Failed to move temporary file to final path: %1").arg(finalPath);
    }
    return false;
}

QString OutputPathPlanner::sanitizeExtension(const QString& rawExtension) {
    QString ext = rawExtension.trimmed().toLower();
    // Strip leading dots and path components; keep only safe chars.
    while (ext.startsWith('.')) ext.remove(0, 1);
    const int slash = qMax(ext.lastIndexOf('/'), ext.lastIndexOf('\\'));
    if (slash >= 0) ext = ext.mid(slash + 1);
    QString clean;
    clean.reserve(ext.size());
    for (const QChar c : ext) {
        if (c.isLetterOrNumber()) clean.append(c);
    }
    if (clean.isEmpty()) clean = QStringLiteral("bin");
    return clean.left(16);
}

bool OutputPathPlanner::validateProducedFile(const QString& path, qint64 minBytes,
                                            QString* errorMessage) {
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        if (errorMessage) *errorMessage = QString("Output file does not exist: %1").arg(path);
        return false;
    }
    if (fi.size() < minBytes) {
        if (errorMessage) {
            *errorMessage = QString("Conversion failed to produce a valid file (%1 bytes, minimum %2).")
                                .arg(fi.size()).arg(minBytes);
        }
        return false;
    }
    return true;
}

} // namespace uc::core
