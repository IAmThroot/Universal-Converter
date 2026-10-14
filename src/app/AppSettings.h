#pragma once

#include <QString>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>

namespace uc::app {

enum class DestinationMode {
    SameAsSource,
    SourceConvertedSubfolder,
    CustomFolder
};

class AppSettings {
public:
    static AppSettings& instance();

    // General
    int maxConcurrentJobs() const;
    void setMaxConcurrentJobs(int jobs);

    bool hardwareAccelerationEnabled() const;
    void setHardwareAccelerationEnabled(bool enabled);

    DestinationMode destinationMode() const;
    void setDestinationMode(DestinationMode mode);

    QString customDestinationPath() const;
    void setCustomDestinationPath(const QString& path);

    // Toolchain paths
    QString ffmpegPathOverride() const;
    void setFfmpegPathOverride(const QString& path);

    QString magickPathOverride() const;
    void setMagickPathOverride(const QString& path);

    QString pandocPathOverride() const;
    void setPandocPathOverride(const QString& path);

    // Watch Folders
    QString watchFoldersJson() const;
    void setWatchFoldersJson(const QString& json);

    void sync();

private:
    AppSettings();
    ~AppSettings() = default;

    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    QSettings m_settings;
};

} // namespace uc::app
