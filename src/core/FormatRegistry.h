#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <optional>

namespace uc::core {

enum class MediaCategory {
    Video,
    Audio,
    Image,
    Document,
    Subtitle
};

enum class CapabilityTier {
    TierA_VerifiedVertAndNative, // In original VERT & verified on host
    TierB_NativeExpanded,        // Native engine expanded capability
    TierC_AwaitingEngine,        // Recognized, awaiting engine wiring
    TierD_UnsupportedOrRestricted // Explicitly disabled / read-only / write-only
};

struct FormatDefinition {
    QString id;                 // e.g. "mp4", "png", "docx"
    QString name;               // e.g. "MP4 Video", "PNG Image"
    MediaCategory category;
    QString defaultExtension;   // e.g. "mp4"
    QStringList extensions;     // e.g. ["mp4", "m4v"]
    QStringList mimeTypes;      // e.g. ["video/mp4"]
    bool canInput{true};
    bool canOutput{true};
    CapabilityTier tier{CapabilityTier::TierA_VerifiedVertAndNative};
};

struct CompatibilityResult {
    bool isSupported{false};
    QString responsibleEngine; // "FFmpeg", "ImageMagick", "Pandoc", or empty
    CapabilityTier tier{CapabilityTier::TierD_UnsupportedOrRestricted};
    QString reason;            // Human-readable explanation if unsupported or restricted
};

class FormatRegistry {
public:
    static FormatRegistry& instance();

    const QList<FormatDefinition>& allFormats() const { return m_formats; }
    QList<FormatDefinition> formatsForCategory(MediaCategory category) const;

    const FormatDefinition* findById(const QString& id) const;
    const FormatDefinition* findByExtension(const QString& ext) const;
    const FormatDefinition* findByFilePath(const QString& filePath) const;

    // Normalization utilities
    QString normalizeExtension(const QString& ext) const;
    QString normalizeFormatId(const QString& id) const;

    // Compatibility check with explanation
    CompatibilityResult checkCompatibility(const QString& inputFormatId, const QString& outputFormatId) const;
    bool isConversionSupported(const QString& inputFormatId, const QString& outputFormatId) const;

    QList<FormatDefinition> availableOutputsForInput(const QString& inputFormatId) const;

private:
    FormatRegistry();
    ~FormatRegistry() = default;

    void initializeFormats();

    QList<FormatDefinition> m_formats;
    QMap<QString, int> m_idToIndex;
    QMap<QString, int> m_extToIndex;
    QMap<QString, QString> m_aliasToNormalized;
};

} // namespace uc::core
