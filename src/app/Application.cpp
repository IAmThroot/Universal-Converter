#include "Application.h"
#include <QTimer>
#include "ToolchainManager.h"
#include "../ui/AppIcon.h"
#include "../ui/MainWindow.h"
#include "../ui/themes/ThemeManager.h"

namespace uc::app {

Application::Application(int &argc, char **argv) : QApplication(argc, argv) {
    setApplicationName("UniversalConverter");
    setApplicationDisplayName("Universal Converter");
    setOrganizationName("UniversalConverter");
    setOrganizationDomain("universal-converter.local");
    setApplicationVersion(UC_VERSION);
    setWindowIcon(ui::appIcon());
    setDesktopFileName("universal-converter");
}

Application::~Application() = default;

bool Application::init() {
    // 1. Set global icon
    setWindowIcon(ui::appIcon());

    // 2. Apply dark stylesheet
    ui::themes::ThemeManager::applyVertDarkTheme(this);

    // 2. Discover external conversion tools asynchronously
    ToolchainManager::instance().scanAllAsync();

    // 3. Create and display main window
    m_mainWindow = std::make_unique<ui::MainWindow>();
    m_mainWindow->show();
    const auto paths = arguments().mid(1);
    if (!paths.isEmpty())
        QTimer::singleShot(0, m_mainWindow.get(), [this, paths]() { m_mainWindow->onFilesDropped(paths); });

    return true;
}

} // namespace uc::app
