#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>
#include "FormatRegistry.h"

namespace uc::core {

enum class PresetCategory {
    WebAndSocial,
    VideoProduction,
    AudioLossless,
    DocumentPublishing,
    Custom
};

struct ConversionPreset {
    QString id;                 // e.g., "discord_video", "youtube_1080p"
    QString name;               // e.g., "Compress for Discord"
    QString description;        // e.g., "Compress video to under 25MB with 720p H.264 & AAC"
    PresetCategory category{PresetCategory::WebAndSocial};
    MediaCategory sourceCategory{MediaCategory::Video};
    QString targetFormatId;     // e.g., "mp4", "webm", "flac", "pdf"
    QVariantMap options;        // Key-value pairs matching engine options
    bool isBuiltIn{true};

    // Helper checks
    bool appliesTo(MediaCategory cat) const { return sourceCategory == cat; }
};

class PresetRegistry {
public:
    static PresetRegistry& instance();

    QList<ConversionPreset> allPresets() const;
    QList<ConversionPreset> presetsForCategory(MediaCategory cat) const;
    const ConversionPreset* findById(const QString& id) const;

    void registerCustomPreset(const ConversionPreset& preset, bool autoSave = true);
    bool removeCustomPreset(const QString& id);

    // Profile Export & Import (JSON)
    QString exportProfilesToJson() const;
    bool importProfilesFromJson(const QString& jsonString, QString* errorMessage = nullptr);

    void loadCustomPresets();
    void saveCustomPresets();

private:
    PresetRegistry();
    ~PresetRegistry() = default;

    void registerBuiltInPresets();
    QString customPresetsFilePath() const;

    QList<ConversionPreset> m_presets;
};

} // namespace uc::core
