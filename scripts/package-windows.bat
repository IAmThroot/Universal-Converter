@echo off
rem ==============================================================================
rem Universal Converter - Windows Packaging Script
rem ==============================================================================
rem Requires MSVC or MinGW, CMake, Ninja, Qt 6, and NSIS.
rem ==============================================================================

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set BUILD_DIR=%PROJECT_ROOT%\build-win
set DIST_DIR=%PROJECT_ROOT%\dist-win

echo === Universal Converter Windows Packaging ===
echo Project Root: %PROJECT_ROOT%
echo Build Dir:    %BUILD_DIR%

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

echo --> Configuring CMake project...
cmake -B "%BUILD_DIR%" -S "%PROJECT_ROOT%" -G "Ninja" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto error

echo --> Building Universal Converter...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 goto error

echo --> Packaging with CPack (NSIS Installer + Portable ZIP with Qt deployment)...
cd /d "%BUILD_DIR%"
cpack -C Release --verbose
if errorlevel 1 goto error

echo === Windows Packaging completed successfully ===
exit /b 0

:error
echo [!] An error occurred during packaging.
exit /b 1
