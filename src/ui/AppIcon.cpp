#include "AppIcon.h"

#include <QSize>

namespace uc::ui {

QIcon appIcon() {
    QIcon icon;
    icon.addFile(":/app_icon_16.png", QSize(16, 16));
    icon.addFile(":/app_icon_22.png", QSize(22, 22));
    icon.addFile(":/app_icon_24.png", QSize(24, 24));
    icon.addFile(":/app_icon_32.png", QSize(32, 32));
    icon.addFile(":/app_icon_48.png", QSize(48, 48));
    icon.addFile(":/app_icon_64.png", QSize(64, 64));
    icon.addFile(":/app_icon_128.png", QSize(128, 128));
    icon.addFile(":/app_icon_256.png", QSize(256, 256));
    icon.addFile(":/app_icon_512.png", QSize(512, 512));
    return icon;
}

} // namespace uc::ui
