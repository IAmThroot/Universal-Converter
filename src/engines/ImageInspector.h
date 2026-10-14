#pragma once

#include <QString>
#include <QList>
#include <QSize>

namespace uc::engines {

struct ImageInspectResult {
    bool isValid{false};
    QString format;           // e.g. "PNG", "JPEG", "GIF"
    int width{0};
    int height{0};
    int frameCount{1};
    QList<QSize> frameSizes;  // especially for multi-size ICO
    qint64 fileSizeBytes{0};
    QString errorMessage;
};

class ImageInspector {
public:
    static ImageInspectResult inspect(const QString& magickPath, const QString& filePath);
};

} // namespace uc::engines
