#include "ThemeManager.h"
#include <QFile>

namespace uc::ui::themes {

void ThemeManager::applyVertDarkTheme(QApplication* app) {
    if (!app) return;

    QFile file(":/vert_dark.qss");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        const QString stylesheet = QString::fromUtf8(file.readAll());
        app->setStyleSheet(stylesheet);
    }
}

} // namespace uc::ui::themes
