#include "app/Application.h"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        uc::app::Application app(argc, argv);
        if (!app.init()) {
            std::cerr << "Failed to initialize Universal Converter application." << std::endl;
            return 1;
        }
        return app.exec();
    } catch (const std::exception& e) {
        std::cerr << "Fatal exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal exception occurred." << std::endl;
        return 1;
    }
}
