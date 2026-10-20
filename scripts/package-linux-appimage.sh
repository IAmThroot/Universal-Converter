#!/usr/bin/env bash
# ==============================================================================
# Universal Converter - Linux AppImage Packaging Script
# ==============================================================================
# Builds UniversalConverter and stages a self-contained AppDir according to
# the AppImage packaging specification.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
DIST_DIR="${PROJECT_ROOT}/dist"
APPDIR="${DIST_DIR}/UniversalConverter.AppDir"

echo "=== Universal Converter AppImage Packaging ==="
echo "Project Root: ${PROJECT_ROOT}"
echo "Build Dir:    ${BUILD_DIR}"
echo "Staging Dir:  ${APPDIR}"

# 1. Compile project
echo "--> Compiling Universal Converter..."
if [ -d "${BUILD_DIR}" ]; then
    cmake --build "${BUILD_DIR}" --config Release -j"$(nproc)"
else
    cmake -B "${BUILD_DIR}" -S "${PROJECT_ROOT}" -DCMAKE_BUILD_TYPE=Release
    cmake --build "${BUILD_DIR}" --config Release -j"$(nproc)"
fi

# 2. Prepare AppDir layout
echo "--> Staging AppDir filesystem hierarchy..."
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/plugins/platforms"
mkdir -p "${APPDIR}/usr/plugins/imageformats"
mkdir -p "${APPDIR}/usr/plugins/wayland-shell-integration"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

# 3. Copy executable and bundled engines
cp "${BUILD_DIR}/UniversalConverter" "${APPDIR}/usr/bin/"
chmod +x "${APPDIR}/usr/bin/UniversalConverter"

if [ -d "${PROJECT_ROOT}/engines" ]; then
    echo "--> Bundling local engines into AppDir..."
    mkdir -p "${APPDIR}/usr/bin/engines"
    cp -r "${PROJECT_ROOT}/engines/"* "${APPDIR}/usr/bin/engines/"
fi

# 4. Copy Qt plugins (platform, imageformats, wayland)
QT_PLUGIN_DIR=""
if command -v qtpaths6 >/dev/null 2>&1; then
    QT_PLUGIN_DIR="$(qtpaths6 --plugin-dir 2>/dev/null || true)"
elif command -v qmake6 >/dev/null 2>&1; then
    QT_PLUGIN_DIR="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
fi

if [ -z "${QT_PLUGIN_DIR}" ] || [ ! -d "${QT_PLUGIN_DIR}" ]; then
    for candidate in \
        "/usr/lib/x86_64-linux-gnu/qt6/plugins" \
        "/usr/lib/qt6/plugins" \
        "/usr/lib/qt/plugins" \
        "/usr/lib64/qt6/plugins"; do
        if [ -d "${candidate}" ]; then
            QT_PLUGIN_DIR="${candidate}"
            break
        fi
    done
fi

echo "--> Detected Qt plugin directory: ${QT_PLUGIN_DIR:-NOT FOUND}"

if [ -n "${QT_PLUGIN_DIR}" ] && [ -d "${QT_PLUGIN_DIR}" ]; then
    echo "--> Copying Qt plugins from ${QT_PLUGIN_DIR}..."
    if [ -d "${QT_PLUGIN_DIR}/platforms" ]; then
        cp -r "${QT_PLUGIN_DIR}/platforms/"* "${APPDIR}/usr/plugins/platforms/" 2>/dev/null || true
    fi
    if [ -d "${QT_PLUGIN_DIR}/imageformats" ]; then
        cp -r "${QT_PLUGIN_DIR}/imageformats/"* "${APPDIR}/usr/plugins/imageformats/" 2>/dev/null || true
    fi
    if [ -d "${QT_PLUGIN_DIR}/wayland-shell-integration" ]; then
        mkdir -p "${APPDIR}/usr/plugins/wayland-shell-integration"
        cp -r "${QT_PLUGIN_DIR}/wayland-shell-integration/"* "${APPDIR}/usr/plugins/wayland-shell-integration/" 2>/dev/null || true
    fi
    if [ -d "${QT_PLUGIN_DIR}/wayland-graphics-integration-client" ]; then
        mkdir -p "${APPDIR}/usr/plugins/wayland-graphics-integration-client"
        cp -r "${QT_PLUGIN_DIR}/wayland-graphics-integration-client/"* "${APPDIR}/usr/plugins/wayland-graphics-integration-client/" 2>/dev/null || true
    fi
fi

# Verify platform plugins exist
echo "--> Staged platform plugins in AppDir:"
ls -la "${APPDIR}/usr/plugins/platforms/" || true
if [ ! -f "${APPDIR}/usr/plugins/platforms/libqxcb.so" ] && [ ! -f "${APPDIR}/usr/plugins/platforms/libqwayland.so" ]; then
    echo "ERROR: No platform plugins (libqxcb.so or libqwayland.so) found in staging directory!"
    exit 1
fi

# 5. Desktop file and Icons
if [ -f "${PROJECT_ROOT}/resources/universal-converter.desktop" ]; then
    cp "${PROJECT_ROOT}/resources/universal-converter.desktop" "${APPDIR}/usr/share/applications/universal-converter.desktop"
    cp "${PROJECT_ROOT}/resources/universal-converter.desktop" "${APPDIR}/universal-converter.desktop"
fi

# Copy or generate icon
for s in 16 22 24 32 48 64 128 256 512; do
    if [ -f "${PROJECT_ROOT}/resources/icons/app_icon_${s}.png" ]; then
        mkdir -p "${APPDIR}/usr/share/icons/hicolor/${s}x${s}/apps"
        cp "${PROJECT_ROOT}/resources/icons/app_icon_${s}.png" "${APPDIR}/usr/share/icons/hicolor/${s}x${s}/apps/universal-converter.png"
    fi
done

if [ -f "${PROJECT_ROOT}/resources/app_icon.svg" ]; then
    mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"
    cp "${PROJECT_ROOT}/resources/app_icon.svg" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/universal-converter.svg"
fi

if [ -f "${PROJECT_ROOT}/resources/icons/app_icon_256.png" ]; then
    cp "${PROJECT_ROOT}/resources/icons/app_icon_256.png" "${APPDIR}/universal-converter.png"
    cp "${PROJECT_ROOT}/resources/icons/app_icon_256.png" "${APPDIR}/.DirIcon"
elif [ -f "${PROJECT_ROOT}/resources/app_icon.png" ]; then
    cp "${PROJECT_ROOT}/resources/app_icon.png" "${APPDIR}/universal-converter.png"
    cp "${PROJECT_ROOT}/resources/app_icon.png" "${APPDIR}/.DirIcon"
fi

# Write qt.conf so Qt runtime always locates Plugins and Prefix within AppDir
cat << 'EOF' > "${APPDIR}/usr/bin/qt.conf"
[Paths]
Prefix = ..
Plugins = plugins
Libraries = lib
EOF

# 6. AppRun entrypoint script
cat << 'EOF' > "${APPDIR}/AppRun"
#!/bin/sh
HERE="$(dirname "$(readlink -f "${0}")")"
export ORIGINAL_LD_LIBRARY_PATH="${LD_LIBRARY_PATH}"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins"
export QML2_IMPORT_PATH="${HERE}/usr/qml"
exec "${HERE}/usr/bin/UniversalConverter" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

# 7. Collect required shared libraries via ldd
echo "--> Bundling core shared libraries..."
copy_deps() {
    local target="$1"
    local deps
    deps=$(ldd "$target" 2>/dev/null | awk '{if ($3 != "") print $3; else if ($1 ~ /^\//) print $1}' | grep -E '^/' || true)
    for lib in $deps; do
        local lib_base
        lib_base=$(basename "$lib")
        # Exclude only fundamental glibc low-level symbols, hardware drivers, and raw display server connections
        case "$lib_base" in
            libc.so*|libm.so*|libdl.so*|librt.so*|libpthread.so*|ld-linux*|libresolv.so*|libutil.so*| \
            libGL.so*|libEGL.so*|libGLX.so*|libgbm.so*|libdrm.so*| \
            libX11.so*|libxcb.so.1|libXau.so*|libXdmcp.so*|libwayland-client.so*)
                continue
                ;;
            *)
                if [ ! -f "${APPDIR}/usr/lib/${lib_base}" ]; then
                    cp -L "$lib" "${APPDIR}/usr/lib/" 2>/dev/null || true
                fi
                ;;
        esac
    done
}

# Scan and copy deps for binary
copy_deps "${APPDIR}/usr/bin/UniversalConverter"

# Scan and copy deps for all staged Qt plugins (platforms, wayland, imageformats)
find "${APPDIR}/usr/plugins" -type f -name "*.so" | while read -r plugin; do
    copy_deps "$plugin"
done

# Second pass over newly copied libraries to catch transitive dependencies
find "${APPDIR}/usr/lib" -maxdepth 1 -type f -name "*.so*" | while read -r lib; do
    copy_deps "$lib"
done

echo "--> AppDir staging complete at ${APPDIR}."

# 8. Check for appimagetool to produce the final .AppImage binary
if command -v appimagetool >/dev/null 2>&1; then
    echo "--> Packaging AppImage binary..."
    appimagetool "${APPDIR}" "${DIST_DIR}/UniversalConverter-x86_64.AppImage"
    echo "--> AppImage created: ${DIST_DIR}/UniversalConverter-x86_64.AppImage"
else
    echo "--> NOTE: 'appimagetool' is not present in PATH."
    echo "--> AppDir structure is completely built, verified, and launchable at:"
    echo "    ${APPDIR}/AppRun"
    echo "--> To produce a standalone .AppImage file, download appimagetool and run:"
    echo "    appimagetool \"${APPDIR}\" \"${DIST_DIR}/UniversalConverter-x86_64.AppImage\""
fi

echo "=== Linux packaging completed successfully ==="
