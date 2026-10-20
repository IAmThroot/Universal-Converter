#include "PresetRegistry.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace uc::core {

PresetRegistry& PresetRegistry::instance() {
    static PresetRegistry s_instance;
    return s_instance;
}

PresetRegistry::PresetRegistry() {
    registerBuiltInPresets();
    loadCustomPresets();
}

void PresetRegistry::registerBuiltInPresets() {
    m_presets.clear();

    // 1. "Compress for Discord" (< 25MB friendly 720p H.264 / AAC)
    {
        ConversionPreset p;
        p.id = "compress_discord";
        p.name = "Compress for Discord";
        p.description = "Optimized for Discord upload limits: 720p H.264 at ~2.5 Mbps with AAC audio";
        p.category = PresetCategory::WebAndSocial;
        p.sourceCategory = MediaCategory::Video;
        p.targetFormatId = "mp4";
        p.options = {
            {"customWidth", 1280},
            {"customHeight", 720},
            {"customVideoBitrateKbps", 2200},
            {"customAudioBitrateKbps", 128},
            {"customVideoCodec", "libx264"},
            {"customAudioCodec", "aac"},
            {"preset", 2} // Medium
        };
        m_presets.append(p);
    }

    // 2. "YouTube 1080p" (High Quality Full HD 1080p 60fps / 320kbps Audio)
    {
        ConversionPreset p;
        p.id = "youtube_1080p";
        p.name = "YouTube 1080p";
        p.description = "Full HD 1080p high bitrate mastering profile ready for YouTube uploads";
        p.category = PresetCategory::VideoProduction;
        p.sourceCategory = MediaCategory::Video;
        p.targetFormatId = "mp4";
        p.options = {
            {"customWidth", 1920},
            {"customHeight", 1080},
            {"customFps", 60},
            {"customVideoBitrateKbps", 12000},
            {"customAudioBitrateKbps", 320},
            {"customVideoCodec", "libx264"},
            {"customAudioCodec", "aac"},
            {"preset", 3} // Slow
        };
        m_presets.append(p);
    }

    // 3. "Lossless Audio" (Studio Master Archive FLAC 24-bit 48kHz / 96kHz)
    {
        ConversionPreset p;
        p.id = "lossless_audio";
        p.name = "Lossless Audio (FLAC)";
        p.description = "Bit-perfect lossless audio preservation with max FLAC compression level";
        p.category = PresetCategory::AudioLossless;
        p.sourceCategory = MediaCategory::Audio;
        p.targetFormatId = "flac";
        p.options = {
            {"customAudioCodec", "flac"},
            {"customAudioBitrateKbps", 0}
        };
        m_presets.append(p);
    }

    // 4. "High Quality MP3" (320 kbps CBR Stereo)
    {
        ConversionPreset p;
        p.id = "podcast_mp3";
        p.name = "High Quality MP3 (320k)";
        p.description = "Universal compatibility audio stream at maximum MP3 bit rate 320 kbps";
        p.category = PresetCategory::WebAndSocial;
        p.sourceCategory = MediaCategory::Audio;
        p.targetFormatId = "mp3";
        p.options = {
            {"customAudioCodec", "libmp3lame"},
            {"customAudioBitrateKbps", 320}
        };
        m_presets.append(p);
    }

    // 5. "Web-ready Images" (WebP 82% Quality Auto-orient & Metadata stripped)
    {
        ConversionPreset p;
        p.id = "web_ready_images";
        p.name = "Web-ready Images (WebP)";
        p.description = "Modern compact WebP image format, stripped metadata, optimal compression";
        p.category = PresetCategory::WebAndSocial;
        p.sourceCategory = MediaCategory::Image;
        p.targetFormatId = "webp";
        p.options = {
            {"quality", 82},
            {"stripMetadata", true},
            {"autoOrient", true}
        };
        m_presets.append(p);
    }

    // 6. "Lossless PNG" (Image preservation with max PNG optimization)
    {
        ConversionPreset p;
        p.id = "lossless_png";
        p.name = "Lossless PNG";
        p.description = "Crisp lossless RGB/RGBA format suitable for diagrams and UI elements";
        p.category = PresetCategory::WebAndSocial;
        p.sourceCategory = MediaCategory::Image;
        p.targetFormatId = "png";
        p.options = {
            {"quality", 100},
            {"stripMetadata", false}
        };
        m_presets.append(p);
    }

    // 7. "Markdown to PDF" (Clean typesetting via Typst or Pandoc PDF engine)
    {
        ConversionPreset p;
        p.id = "markdown_to_pdf";
        p.name = "Markdown to PDF";
        p.description = "Typeset Markdown documents into PDF using standalone document engines";
        p.category = PresetCategory::DocumentPublishing;
        p.sourceCategory = MediaCategory::Document;
        p.targetFormatId = "pdf";
        p.options = {
            {"toc", false},
            {"standalone", true}
        };
        m_presets.append(p);
    }

    // 8. "Video to Animated GIF" (High quality palettegen 15 fps)
    {
        ConversionPreset p;
        p.id = "video_to_gif";
        p.name = "High-Quality Animated GIF";
        p.description = "Convert video to animated GIF with dual-pass palette generation and 15 fps rate limit";
        p.category = PresetCategory::WebAndSocial;
        p.sourceCategory = MediaCategory::Video;
        p.targetFormatId = "gif";
        p.options = {
            {"customFps", 15},
            {"customWidth", 480}
        };
        m_presets.append(p);
    }

    // 9. "Video to Animated WebP" (Modern high-compression animation)
    {
        ConversionPreset p;
        p.id = "video_to_webp";
        p.name = "Animated WebP (Compact)";
        p.description = "Convert video to lightweight looping animated WebP for web delivery";
        p.category = PresetCategory::WebAndSocial;
        p.sourceCategory = MediaCategory::Video;
        p.targetFormatId = "webp";
        p.options = {
            {"customFps", 20}
        };
        m_presets.append(p);
    }

    // 10. "Video to Animated AVIF" (Next-gen AV1 HDR-capable animation)
    {
        ConversionPreset p;
        p.id = "video_to_avif";
        p.name = "Animated AVIF (Next-Gen)";
        p.description = "Convert video to modern AV1-encoded animated AVIF";
        p.category = PresetCategory::WebAndSocial;
        p.sourceCategory = MediaCategory::Video;
        p.targetFormatId = "avif";
        p.options = {
            {"customFps", 24}
        };
        m_presets.append(p);
    }
}

QList<ConversionPreset> PresetRegistry::allPresets() const {
    return m_presets;
}

QList<ConversionPreset> PresetRegistry::presetsForCategory(MediaCategory cat) const {
    QList<ConversionPreset> filtered;
    for (const auto& p : m_presets) {
        if (p.appliesTo(cat)) {
            filtered.append(p);
        }
    }
    return filtered;
}

const ConversionPreset* PresetRegistry::findById(const QString& id) const {
    for (const auto& p : m_presets) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

void PresetRegistry::registerCustomPreset(const ConversionPreset& preset, bool autoSave) {
    for (int i = 0; i < m_presets.size(); ++i) {
        if (m_presets[i].id == preset.id) {
            m_presets[i] = preset;
            if (autoSave) saveCustomPresets();
            return;
        }
    }
    m_presets.append(preset);
    if (autoSave) saveCustomPresets();
}

bool PresetRegistry::removeCustomPreset(const QString& id) {
    for (int i = 0; i < m_presets.size(); ++i) {
        if (m_presets[i].id == id && !m_presets[i].isBuiltIn) {
            m_presets.removeAt(i);
            saveCustomPresets();
            return true;
        }
    }
    return false;
}

QString PresetRegistry::customPresetsFilePath() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath("custom_presets.json");
}

void PresetRegistry::loadCustomPresets() {
    const QString path = customPresetsFilePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    const QByteArray data = file.readAll();
    file.close();

    importProfilesFromJson(QString::fromUtf8(data));
}

void PresetRegistry::saveCustomPresets() {
    const QString json = exportProfilesToJson();
    const QString path = customPresetsFilePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    file.write(json.toUtf8());
    file.close();
}

QString PresetRegistry::exportProfilesToJson() const {
    QJsonArray arr;
    for (const auto& p : m_presets) {
        if (p.isBuiltIn) continue; // Only export user-authored profiles

        QJsonObject obj;
        obj["id"] = p.id;
        obj["name"] = p.name;
        obj["description"] = p.description;
        obj["category"] = static_cast<int>(p.category);
        obj["sourceCategory"] = static_cast<int>(p.sourceCategory);
        obj["targetFormatId"] = p.targetFormatId;
        obj["options"] = QJsonObject::fromVariantMap(p.options);
        arr.append(obj);
    }

    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

bool PresetRegistry::importProfilesFromJson(const QString& jsonString, QString* errorMessage) {
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(jsonString.toUtf8(), &err);
    if (doc.isNull() || !doc.isArray()) {
        if (errorMessage) *errorMessage = QString("Invalid JSON format: %1").arg(err.errorString());
        return false;
    }

    const auto arr = doc.array();
    bool anyAdded = false;
    for (const auto& item : arr) {
        if (!item.isObject()) continue;
        const auto obj = item.toObject();

        ConversionPreset p;
        p.id = obj["id"].toString();
        p.name = obj["name"].toString();
        p.description = obj["description"].toString();
        p.category = static_cast<PresetCategory>(obj["category"].toInt(static_cast<int>(PresetCategory::Custom)));
        p.sourceCategory = static_cast<MediaCategory>(obj["sourceCategory"].toInt());
        p.targetFormatId = obj["targetFormatId"].toString();
        p.options = obj["options"].toObject().toVariantMap();
        p.isBuiltIn = false;

        if (!p.id.isEmpty() && !p.targetFormatId.isEmpty()) {
            registerCustomPreset(p, false);
            anyAdded = true;
        }
    }

    if (anyAdded) {
        saveCustomPresets();
    }

    return true;
}

} // namespace uc::core
