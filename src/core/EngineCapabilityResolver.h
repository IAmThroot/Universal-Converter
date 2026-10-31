#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QObject>
#include <QVariantMap>
#include "FormatRegistry.h"

namespace uc::core {

enum class FormatReadiness {
    Recognized, // Application knows format specifications and aliases
    Available,  // Engine installed on current OS and supports reading/writing this format
    Verified    // Proven end-to-end with high-fidelity output tests
};

struct DynamicFormatCapability {
    FormatDefinition definition;
    FormatReadiness readiness{FormatReadiness::Recognized};
    bool isReadableOnHost{false};
    bool isWritableOnHost{false};
    QString activeEngineName;
    QString engineStatusDetail;
    QString delegateDetail;
};

struct RouteCapability {
    bool available{false};
    QString reason;
    QString engine;
};

class EngineCapabilityResolver : public QObject {
    Q_OBJECT
  public:
    static EngineCapabilityResolver &instance();

    void refresh();
    RouteCapability resolveRoute(const QString &input, const QString &output,
                                 const QVariantMap &options = {}) const;
  signals:
    void capabilitiesChanged();

  public:
    QList<DynamicFormatCapability> allResolvedCapabilities() const;
    DynamicFormatCapability resolveFormat(const QString &formatId) const;

    int totalRecognizedCount() const;
    int totalAvailableCount() const;
    int totalVerifiedCount() const;

  private:
    EngineCapabilityResolver();
    ~EngineCapabilityResolver() = default;

    QMap<QString, DynamicFormatCapability> m_cache;
    QString m_muxers;
    QString m_encoders;
};

} // namespace uc::core
