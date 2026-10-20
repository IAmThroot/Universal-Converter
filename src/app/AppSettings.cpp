#include "AppSettings.h"
#include <QThread>

namespace uc::app {

AppSettings& AppSettings::instance() {
    static AppSettings s_instance;
    return s_instance;
}

AppSettings::AppSettings()
    : m_settings("UniversalConverter", "UniversalConverter")
{
}

int AppSettings::maxConcurrentJobs() const {
    const int threadCount = QThread::idealThreadCount();
    const int defaultJobs = qBound(1, threadCount > 0 ? (threadCount <= 4 ? threadCount : 4) : 2, 8);
    return m_settings.value("general/maxConcurrentJobs", defaultJobs).toInt();
}

void AppSettings::setMaxConcurrentJobs(int jobs) {
    m_settings.setValue("general/maxConcurrentJobs", qBound(1, jobs, 16));
}

bool AppSettings::hardwareAccelerationEnabled() const {
    return m_settings.value("general/hardwareAccelerationEnabled", true).toBool();
}

void AppSettings::setHardwareAccelerationEnabled(bool enabled) {
    m_settings.setValue("general/hardwareAccelerationEnabled", enabled);
}

DestinationMode AppSettings::destinationMode() const {
    const int val = m_settings.value("output/destinationMode", static_cast<int>(DestinationMode::SourceConvertedSubfolder)).toInt();
    return static_cast<DestinationMode>(val);
}

void AppSettings::setDestinationMode(DestinationMode mode) {
    m_settings.setValue("output/destinationMode", static_cast<int>(mode));
}

QString AppSettings::customDestinationPath() const {
    const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return m_settings.value("output/customDestinationPath", defaultPath).toString();
}

void AppSettings::setCustomDestinationPath(const QString& path) {
    m_settings.setValue("output/customDestinationPath", path);
}

QString AppSettings::ffmpegPathOverride() const {
    return m_settings.value("tools/ffmpegPath", "").toString();
}

void AppSettings::setFfmpegPathOverride(const QString& path) {
    m_settings.setValue("tools/ffmpegPath", path);
}

QString AppSettings::magickPathOverride() const {
    return m_settings.value("tools/magickPath", "").toString();
}

void AppSettings::setMagickPathOverride(const QString& path) {
    m_settings.setValue("tools/magickPath", path);
}

QString AppSettings::pandocPathOverride() const {
    return m_settings.value("tools/pandocPath", "").toString();
}

void AppSettings::setPandocPathOverride(const QString& path) {
    m_settings.setValue("tools/pandocPath", path);
}

QString AppSettings::watchFoldersJson() const {
    return m_settings.value("watchFolders/configurations", "[]").toString();
}

void AppSettings::setWatchFoldersJson(const QString& json) {
    m_settings.setValue("watchFolders/configurations", json);
}

void AppSettings::sync() {
    m_settings.sync();
}

} // namespace uc::app
