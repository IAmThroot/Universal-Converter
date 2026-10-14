#pragma once

#include <QString>
#include <QStringList>
#include <QList>
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

class EngineCapabilityResolver {
public:
    static EngineCapabilityResolver& instance();

    void refresh();

    QList<DynamicFormatCapability> allResolvedCapabilities() const;
    DynamicFormatCapability resolveFormat(const QString& formatId) const;

    int totalRecognizedCount() const;
    int totalAvailableCount() const;
    int totalVerifiedCount() const;

private:
    EngineCapabilityResolver();
    ~EngineCapabilityResolver() = default;

    QMap<QString, DynamicFormatCapability> m_cache;
};

} // namespace uc::core
