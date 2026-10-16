#include "Application.h"
#include "ToolchainManager.h"
#include "../ui/MainWindow.h"
#include "../ui/themes/ThemeManager.h"

namespace uc::app {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setApplicationName("UniversalConverter");
    setApplicationDisplayName("Universal Converter");
    setOrganizationName("UniversalConverter");
    setOrganizationDomain("universal-converter.local");
    setApplicationVersion("1.1.0");
    setWindowIcon(QIcon(":/app_icon.png"));
    setDesktopFileName("universal-converter");
}

Application::~Application() = default;

bool Application::init() {
    // 1. Set global icon
    setWindowIcon(QIcon(":/app_icon.png"));

    // 2. Apply dark stylesheet
    ui::themes::ThemeManager::applyVertDarkTheme(this);

    // 2. Discover external conversion tools
    ToolchainManager::instance().scanAll();

    // 3. Create and display main window
    m_mainWindow = std::make_unique<ui::MainWindow>();
    m_mainWindow->show();

    return true;
}

} // namespace uc::app
