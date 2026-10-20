cmake_minimum_required(VERSION 3.22)
project(UniversalConverter
    VERSION 1.1.5
    DESCRIPTION "High-performance, standalone, offline cross-platform media and document converter"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimized Release build when no build type is given
# (single-config generators like Ninja/Make). Multi-config generators ignore this.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

if(NOT MSVC)
    # Baseline optimization for non-MSVC when user did not specify flags.
    add_compile_options($<$<CONFIG:Release>:-O2>)
endif()

# Enable Qt standard properties
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Svg
)

# Compiler Sanitizer Option (ASan + UBSan)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# ==========================================
# --- converter_core Reusable Library ---
# ==========================================
set(CORE_SOURCES
    src/app/AppSettings.cpp
    src/app/AppSettings.h
    src/app/EngineResolver.cpp
    src/app/EngineResolver.h
    src/app/ToolchainManager.cpp
    src/app/ToolchainManager.h
    src/core/ConversionJob.cpp
    src/core/ConversionJob.h
    src/core/FormatRegistry.cpp
    src/core/FormatRegistry.h
    src/core/ProcessRunner.cpp
    src/core/ProcessRunner.h
    src/core/OutputPathPlanner.cpp
    src/core/OutputPathPlanner.h
    src/core/ConversionOrchestrator.cpp
    src/core/ConversionOrchestrator.h
    src/core/WatchFolderConfig.h
    src/core/WatchFolderService.cpp
    src/core/WatchFolderService.h
    src/core/SystemInfo.cpp
    src/core/SystemInfo.h
    src/engines/MediaProbe.cpp
    src/engines/MediaProbe.h
    src/engines/FfmpegProgressParser.cpp
    src/engines/FfmpegProgressParser.h
    src/engines/FfmpegArgBuilder.cpp
    src/engines/FfmpegArgBuilder.h
    src/engines/OutputValidator.cpp
    src/engines/OutputValidator.h
    src/engines/FfmpegEngine.cpp
    src/engines/FfmpegEngine.h
    src/engines/ImageMagickCapabilities.cpp
    src/engines/ImageMagickCapabilities.h
    src/engines/ImageMagickArgBuilder.cpp
    src/engines/ImageMagickArgBuilder.h
    src/engines/ImageInspector.cpp
    src/engines/ImageInspector.h
    src/engines/ImageMagickEngine.cpp
    src/engines/ImageMagickEngine.h
    src/engines/VectorPipeline.cpp
    src/engines/VectorPipeline.h
    src/engines/ImageOptimizer.cpp
    src/engines/ImageOptimizer.h
    src/engines/PandocCapabilities.cpp
    src/engines/PandocCapabilities.h
    src/engines/PandocArgBuilder.cpp
    src/engines/PandocArgBuilder.h
    src/core/PresetRegistry.cpp
    src/core/PresetRegistry.h
    src/core/ConversionHistoryManager.cpp
    src/core/ConversionHistoryManager.h
    src/core/MediaEstimator.cpp
    src/core/MediaEstimator.h
    src/core/EngineCapabilityResolver.cpp
    src/core/EngineCapabilityResolver.h
    src/engines/DocumentValidator.cpp
    src/engines/DocumentValidator.h
    src/engines/PandocEngine.cpp
    src/engines/PandocEngine.h
)

add_library(converter_core STATIC
    ${CORE_SOURCES}
)

target_include_directories(converter_core PUBLIC
    src
)

target_link_libraries(converter_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Svg
    Qt6::Network
)

if(ENABLE_SANITIZERS)
    if(MSVC)
        target_compile_options(converter_core PUBLIC /fsanitize=address)
    else()
        target_compile_options(converter_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(converter_core PUBLIC -fsanitize=address,undefined)
    endif()
endif()

if(MSVC)
    target_compile_options(converter_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(converter_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ==========================================
# --- Main Application Target ---
# ==========================================
set(APP_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/Application.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/DropZoneWidget.cpp
    src/ui/DropZoneWidget.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/FormatSelectorDialog.cpp
    src/ui/FormatSelectorDialog.h
    src/ui/HistoryDialog.cpp
    src/ui/HistoryDialog.h
    src/ui/PresetEditorDialog.cpp
    src/ui/PresetEditorDialog.h
    src/ui/ImagePreviewDialog.cpp
    src/ui/ImagePreviewDialog.h
    src/ui/QueueViewWidget.cpp
    src/ui/QueueViewWidget.h
    src/ui/themes/ThemeManager.cpp
    src/ui/themes/ThemeManager.h
)

set(APP_RESOURCES
    resources/resources.qrc
)

if(WIN32)
    list(APPEND APP_SOURCES resources/windows/app.rc)
endif()

add_executable(UniversalConverter WIN32
    ${APP_SOURCES}
    ${APP_RESOURCES}
)

set_target_properties(UniversalConverter PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_include_directories(UniversalConverter PRIVATE
    src
)

target_link_libraries(UniversalConverter PRIVATE
    converter_core
    Qt6::Gui
    Qt6::Widgets
)

if(MSVC)
    target_compile_options(UniversalConverter PRIVATE /W4 /permissive-)
else()
    target_compile_options(UniversalConverter PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ==========================================
# --- Automated Unit Tests (Qt Test) ---
# ==========================================
option(BUILD_TESTING "Build automated unit test suite" ON)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()

    add_executable(tst_core
        tests/tst_core.cpp
    )

    target_include_directories(tst_core PRIVATE
        src
    )

    target_link_libraries(tst_core PRIVATE
        converter_core
        Qt6::Test
    )

    set_target_properties(tst_core PROPERTIES
        WIN32_EXECUTABLE FALSE
    )

    if(MSVC)
        target_link_options(tst_core PRIVATE "/SUBSYSTEM:CONSOLE")
    endif()

    add_test(NAME tst_core COMMAND tst_core)
endif()

# ==========================================
# --- Installation & CPack Packaging ---
# ==========================================
include(GNUInstallDirs)

# On Windows, install executable and engines directly to root (.) of package for clean portable/installed layout
if(WIN32)
    set(UC_INSTALL_BINDIR ".")
else()
    set(UC_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
endif()

install(TARGETS UniversalConverter
    RUNTIME DESTINATION ${UC_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

# Install icon and desktop file on Unix
if(UNIX AND NOT APPLE)
    install(FILES resources/universal-converter.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
        OPTIONAL
    )
    install(FILES resources/app_icon.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME universal-converter.png
        OPTIONAL
    )
endif()

# Install bundled engines if present
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/engines")
    install(DIRECTORY engines
        DESTINATION ${UC_INSTALL_BINDIR}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        OPTIONAL
    )
endif()

# Deploy Qt runtime and compiler runtime dependencies on Windows during install/CPack staging
if(WIN32)
    # Find windeployqt from Qt installation
    get_target_property(_qt_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    if(_qt_qmake_executable)
        get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    else()
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
    endif()

    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            message(STATUS \"--> Running windeployqt on \${CMAKE_INSTALL_PREFIX}/${UC_INSTALL_BINDIR}/UniversalConverter.exe...\")
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                    --release
                    --no-translations
                    --compiler-runtime
                    --svg
                    --network
                    --verbose 1
                    \"\${CMAKE_INSTALL_PREFIX}/${UC_INSTALL_BINDIR}/UniversalConverter.exe\"
                RESULT_VARIABLE _windeploy_res
            )
            if(NOT _windeploy_res EQUAL 0)
                message(FATAL_ERROR \"windeployqt failed with exit code \${_windeploy_res}\")
            endif()
        ")
    else()
        message(WARNING \"windeployqt not found; Windows installer/archive will lack Qt runtime libraries.\")
    endif()
endif()

# CPack Settings for Multi-Platform Distribution
set(CPACK_PACKAGE_NAME "UniversalConverter")
set(CPACK_PACKAGE_VENDOR "IAmThroot")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance offline cross-platform media and document converter")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT "https://github.com/IAmThroot/Universal-Converter")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(WIN32)
    # Windows: Generate NSIS Installer and Portable ZIP
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Universal Converter")
    set(CPACK_NSIS_PACKAGE_NAME "UniversalConverter")
    set(CPACK_NSIS_HELP_LINK "https://github.com/IAmThroot/Universal-Converter")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/IAmThroot/Universal-Converter")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "UniversalConverter")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_PACKAGE_EXECUTABLES "UniversalConverter" "Universal Converter")
    set(CPACK_CREATE_DESKTOP_LINKS "UniversalConverter")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "UniversalConverter.exe")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/app_icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/app_icon.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "UniversalConverter.exe")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    # Linux: TGZ archive
    set(CPACK_GENERATOR "TGZ;STGZ")
endif()

include(CPack)

