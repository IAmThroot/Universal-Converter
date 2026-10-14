#pragma once

#include <QApplication>
#include <memory>

namespace uc::ui {
    class MainWindow;
}

namespace uc::app {

class Application : public QApplication {
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    bool init();

private:
    std::unique_ptr<ui::MainWindow> m_mainWindow;
};

} // namespace uc::app
