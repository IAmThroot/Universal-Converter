#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>

namespace uc::engines {

struct ImageFormatMode {
    QString format;       // Upper-case tag, e.g. "PNG", "JPEG", "CR2"
    bool canRead{false};
    bool canWrite{false};
    bool supportsMultiFrame{false};
    QString description;
};

class ImageMagickCapabilities {
public:
    static ImageMagickCapabilities& instance();

    bool isDiscovered() const { return m_isDiscovered; }
    void discover(const QString& magickExecutablePath);

    bool canReadFormat(const QString& formatOrExt) const;
    bool canWriteFormat(const QString& formatOrExt) const;
    bool supportsAnimation(const QString& formatOrExt) const;

    int totalRecognizedFormats() const { return m_formats.size(); }
    int readableFormatCount() const { return m_readableFormats.size(); }
    int writableFormatCount() const { return m_writableFormats.size(); }

    QString versionString() const { return m_versionString; }
    QString delegateInfo() const { return m_delegateInfo; }

private:
    ImageMagickCapabilities() = default;

    bool m_isDiscovered{false};
    QString m_versionString;
    QString m_delegateInfo;

    QMap<QString, ImageFormatMode> m_formats; // keyed by uppercase format tag
    QSet<QString> m_readableFormats;
    QSet<QString> m_writableFormats;
    QSet<QString> m_multiFrameFormats;
};

} // namespace uc::engines
