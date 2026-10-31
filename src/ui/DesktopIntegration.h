#pragma once
#include <QString>
#include <QProcessEnvironment>
class QWidget;
namespace uc::ui {
class DesktopIntegration {
  public:
    // Launch with host libraries/plugins rather than the AppImage's Qt runtime.
    static QProcessEnvironment hostEnvironment(QProcessEnvironment environment);
    static void openLocalPath(QWidget *parent, const QString &path);
};
} // namespace uc::ui
