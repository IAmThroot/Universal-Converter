#pragma once

#include <QIcon>

namespace uc::ui {

// Multi-resolution application icon.
//
// The icon set is generated from resources/app_icon.svg (master) and
// resources/app_icon_small.svg (optical correction for <= 32px) by
// scripts/generate_icons.sh. Each PNG is embedded in the binary via
// resources/resources.qrc, so the window, taskbar and dock entry stay
// crisp on HiDPI displays instead of being upscaled from a single bitmap.
QIcon appIcon();

} // namespace uc::ui
