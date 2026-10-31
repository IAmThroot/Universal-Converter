#include "ThemeManager.h"
#include <QFile>
#include <QPalette>

namespace uc::ui::themes {

void ThemeManager::applyVertDarkTheme(QApplication *app) {
    if (!app)
        return;

    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#121215"));
    palette.setColor(QPalette::WindowText, QColor("#f4f4f5"));
    palette.setColor(QPalette::Base, QColor("#18181b"));
    palette.setColor(QPalette::AlternateBase, QColor("#121215"));
    palette.setColor(QPalette::Text, QColor("#f4f4f5"));
    palette.setColor(QPalette::Button, QColor("#27272a"));
    palette.setColor(QPalette::ButtonText, QColor("#f4f4f5"));
    palette.setColor(QPalette::Highlight, QColor("#065f46"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, QColor("#34d399"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#a1a1aa"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#a1a1aa"));
    app->setPalette(palette);
    QFile file(":/vert_dark.qss");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        const QString stylesheet = QString::fromUtf8(file.readAll());
        app->setStyleSheet(stylesheet);
    }
}

} // namespace uc::ui::themes
