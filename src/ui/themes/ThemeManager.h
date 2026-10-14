#pragma once

#include <QString>
#include <QApplication>

namespace uc::ui::themes {

class ThemeManager {
public:
    static void applyVertDarkTheme(QApplication* app);
};

} // namespace uc::ui::themes
