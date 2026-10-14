#pragma once

#include <QString>
#include <QStringList>

namespace uc::core {

struct SystemCapabilities {
    int cpuThreadCount{1};
    bool hasNvidiaGpu{false};
    bool hasNvenc{false};
    bool hasVaapi{false};
    bool hasQsv{false};
    QStringList availableHwEncoders;
};

class SystemInfo {
public:
    static SystemCapabilities probe();
};

} // namespace uc::core
