# Universal Converter

[![Build & Package Universal Converter](https://github.com/IAmThroot/Universal-Converter/actions/workflows/package.yml/badge.svg)](https://github.com/IAmThroot/Universal-Converter/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-brightgreen.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt 6](https://img.shields.io/badge/Qt-6.5%2B-green.svg)](https://www.qt.io/)
[![Version: 1.0.0](https://img.shields.io/badge/Release-v1.0.0-emerald.svg)](https://github.com/IAmThroot/Universal-Converter/releases/tag/v1.0.0)

A high-performance, completely offline, native **C++20 / Qt 6** desktop application for converting audio, video, images, subtitles, and documents across formats without web runtimes, Electron, localhost daemons, or remote API dependencies.

---

## Features & Highlights

- **Native Architecture**: Built directly with modern C++20 and Qt 6 Widgets. Low memory footprint, fast startup, and native FreeDesktop/Windows integration.
- **Conversion Presets & Custom Preset Builder**: Ready-to-use profiles including *"Compress for Discord (<25MB)"*, *"YouTube 1080p"*, *"High-Quality Animated GIF"*, *"Animated WebP"*, *"Lossless Audio (FLAC)"*, and *"Markdown to PDF"*, plus an interactive GUI to build, save, export, and import custom JSON recipes.
- **Watch Folder Automation Daemon**: Background filesystem watcher with size-stable debouncing, custom output routing, and configurable post-processing triggers (*Move to Processed*, *Delete Original*, *Keep Original*).
- **Subtitles & Text Stream Engine**: Full conversion matrix for `SRT ↔ VTT ↔ ASS ↔ SSA ↔ SUB` with automatic multi-tier character encoding detection (BOM, strict UTF-8, `uchardet`, `CP1252`/Latin-1), preventing decoding errors across international subtitle sets.
- **Video ↔ Animated Image Pipelines**: High-fidelity bidirectional conversion between video and animated images (`GIF`, `WebP`, `AVIF`) using dual-pass `palettegen`/`paletteuse` filters, looping controls, and audio stripping.
- **Modern Vector & Image Pipelines**: First-class SVG vector rasterization via `rsvg-convert` with a zero-dependency `Qt6::Svg` fallback, plus detection and execution hooks for standalone image optimization toolchains (`oxipng`, `pngquant`, `cwebp`, `avifenc`).
- **Interactive Before & After Image Preview**: Dual-panel visual comparison showing original vs compressed preview with live quality slider (10% - 100%) and estimated output file size.
- **Dynamic Format Capability Resolver**: Live distinction between **Recognized**, **Available**, and **Verified** formats based on runtime inspection of installed engine binaries and delegates.
- **Conversion History & Retry**: Privacy-preserving persistent local history logging timestamps, file sizes, conversion duration, and job status with one-click retry.
- **Unified Engine Orchestration**:
  - **Video & Audio**: Multi-threaded encoding via FFmpeg, full hardware acceleration support (NVIDIA NVENC, Intel VA-API / QSV, AMD AMF), and accurate stream probing via FFprobe.
  - **Subtitles**: Direct text stream transcoding via FFmpeg with intelligent charset negotiation.
  - **Images & Vector**: Raster, vector, and RAW image conversions across 50+ formats powered by ImageMagick and `rsvg-convert`.
  - **Documents**: Markdown, DOCX, HTML, EPUB, RTF, LaTeX, ODT, AsciiDoc, Org-Mode, Textile, and MediaWiki powered by bundled Pandoc.
  - **Direct PDF Export**: High-speed, modern PDF generation powered by bundled **Typst**, avoiding heavy TeX Live / XeLaTeX distributions.
- **Fail-Safe Engine Resolver**: Flexible discovery that checks (1) explicit user overrides, (2) bundled local binaries, and (3) system `$PATH` executables.
- **Zero Cloud / True Offline**: All jobs run locally using external process isolation. No telemetry, no background network calls, and no data leaving your machine.
- **Collision & Overwrite Guard**: Safe path planner with in-place overwrite prevention and automatic numeric suffix disambiguation.
- **Modern Obsidian Dark UI**: Responsive Zinc & Emerald desktop interface with branded header, drag-and-drop batch queue, live status indicators, interactive format selector, and cancellation controls.

---

## Supported Formats & Capability Model

Universal Converter distinguishes between three tiers of format support:
1. **Recognized**: The container format or document syntax is registered in the application schema with valid MIME types and canonical normalization rules.
2. **Available**: An installed local or host engine (`ffmpeg`, `magick`, `pandoc`, or `typst`) has been dynamically probed and reports read/write support for the container and its underlying codecs.
3. **Verified**: The conversion route has passed end-to-end integration and automated test suite verification against real binary media.

### Verified Native Formats (114 Unique Formats Audited)

| Engine | Category | Verified Count | Formats |
| :--- | :--- | :---: | :--- |
| **FFmpeg** | Video & Animated | 23 | `mp4`, `mkv`, `webm`, `avi`, `mov`, `flv`, `wmv`, `m4v`, `3gp`, `ts`, `vob`, `ogv`, `mpeg`, `mpg`, `m2ts`, `asf`, `f4v`, `y4m`, `dv`, `wtv`, animated `gif`, `webp`, `avif` |
| **FFmpeg** | Audio | 23 | `mp3`, `aac`, `ogg`, `opus`, `flac`, `wav`, `m4a`, `wma`, `aiff`, `alac`, `ac3`, `mka`, `amr`, `ape`, `ra`, `voc`, `au`, `caf`, `spx`, `mid`, `eac3`, `dts`, `wv` |
| **FFmpeg** | Subtitles | 5 | `srt`, `vtt`, `ass`, `ssa`, `sub` (with automatic character encoding detection) |
| **ImageMagick & Vector** | Images & RAW | 50 | `png`, `jpg`, `jpeg`, `webp`, `gif`, `bmp`, `tiff`, `ico`, `svg`, `avif`, `tga`, `heic`, `dcm`, `cr2`, `nef`, `arw`, `dng`, `orf`, `rw2`, `pef`, `jxl`, `exr`, `pcx`, `pbm`, `pgm`, `ppm`, `pnm`, `xpm`, `xbm`, `wbmp`, `dds`, `dpx`, `cin`, `sgi`, `ras`, `qoi`, `mos`, `raw`, `3fr`, `erf`, `mrw`, `mef`, `srw`, `sr2`, `srf`, etc. |
| **Pandoc + Typst** | Documents & PDF | 16 | `md`, `markdown`, `docx`, `odt`, `epub`, `tex`, `rst`, `adoc`, `org`, `textile`, `wiki`, `opml`, `rtf`, `txt`, `pdf`, `html` |

> [!TIP]
> **Container vs. Codec Discovery**: An MP4 container requires supported video/audio codecs in your installed FFmpeg build; Camera RAW formats require ImageMagick delegate libraries (`libraw`, `librsvg`). Universal Converter dynamically probes your installed engine capabilities at launch and disables unsupported conversion routes.


---

## Installation Guide

### Engine Availability Overview

Universal Converter bundles lightweight document and typesetting engines directly into the release packages, while leveraging your system's native multimedia installations for optimal GPU performance:

| Engine | Primary Purpose | Linux (AppImage) | Windows (Installer & Portable) |
| :--- | :--- | :---: | :---: |
| **Pandoc** | Documents (Markdown, DOCX, EPUB, RTF, HTML, etc.) | **Bundled** ✅ | **Bundled** ✅ |
| **Typst** | Modern High-Speed PDF Engine | **Bundled** ✅ | **Bundled** ✅ |
| **FFmpeg** | Video & Audio encoding / Hardware acceleration | Requires Host Install ⚠️ | Requires Host Install ⚠️ |
| **ImageMagick** | Images, WebP, AVIF, RAW formats | Requires Host Install ⚠️ | Requires Host Install ⚠️ |

> [!NOTE]
> **Documents & PDFs work 100% out-of-the-box** on both Linux and Windows with zero additional downloads.
> To enable Video, Audio, or Image conversions, simply install FFmpeg and ImageMagick using the one-line commands below.

---

### Option 1: Pre-Built Release Packages (Recommended)

Download the latest pre-built binaries directly from the [GitHub Releases](https://github.com/IAmThroot/Universal-Converter/releases/latest) page:

- **Linux (x86_64)**:
  - 📦 **Download `UniversalConverter-x86_64.AppImage`**
  - Make executable and run:
    ```bash
    chmod +x UniversalConverter-x86_64.AppImage
    ./UniversalConverter-x86_64.AppImage
    ```
  - **Prerequisites for Media/Image conversions**:
    - **Arch / CachyOS**: `sudo pacman -S ffmpeg imagemagick`
    - **Ubuntu / Debian**: `sudo apt install ffmpeg imagemagick`
    - **Fedora**: `sudo dnf install ffmpeg ImageMagick`

- **Windows (x64)**:
  - 💾 **Download `UniversalConverter-1.0.0-win64.exe`** (NSIS setup wizard with desktop shortcuts and uninstaller)
  - 🗜️ **Download `UniversalConverter-1.0.0-win64.zip`** (Portable package: extract and run `UniversalConverter.exe`)
  - **Prerequisites for Media/Image conversions**:
    - Install via Windows Package Manager (`winget` in Terminal / PowerShell):
      ```powershell
      winget install Gyan.FFmpeg ImageMagick.ImageMagick
      ```
    - *Alternatively*, if you already have custom standalone binaries, set their paths in **Settings (⚙️) -> Custom Tool Paths**.


### Option 2: Building from Source

#### Prerequisites

##### System Packages (Linux)
- **Arch Linux / CachyOS**:
  ```bash
  sudo pacman -S cmake ninja qt6-base qt6-svg qt6-wayland ffmpeg imagemagick gcc
  ```
- **Ubuntu / Debian (24.04+)**:
  ```bash
  sudo apt update
  sudo apt install -y cmake ninja-build qt6-base-dev libqt6svg6-dev qt6-wayland ffmpeg imagemagick g++
  ```
- **Fedora (40+)**:
  ```bash
  sudo dnf install cmake ninja-build qt6-qtbase-devel qt6-qtsvg-devel qt6-qtwayland ffmpeg ImageMagick gcc-c++
  ```

##### System Packages (Windows)
- Visual Studio 2022 with C++ Desktop Development workload.
- Qt 6.5+ (MSVC 2022 64-bit).
- CMake 3.22+ and Ninja.
- FFmpeg and ImageMagick (optional for document-only users, recommended):
  ```cmd
  winget install Gyan.FFmpeg ImageMagick.ImageMagick
  ```

#### Build Instructions (Linux)

```bash
# Clone repository
git clone https://github.com/IAmThroot/Universal-Converter.git
cd Universal-Converter

# Configure with Ninja (Release mode)
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build executable and test suite
ninja -C build

# Run automated tests (66 unit and integration tests across core, process runner, and engines)
ctest --test-dir build --output-on-failure

# Launch application
./build/UniversalConverter
```

#### Packaging Linux AppImage / AppDir

```bash
# Creates a launchable, self-contained AppDir (and .AppImage if appimagetool is in PATH)
./scripts/package-linux-appimage.sh

# Run directly from staged AppDir
./dist/UniversalConverter.AppDir/AppRun
```

#### Packaging Windows (MSVC 2022 + windeployqt)

```cmd
:: Run packaging script from Windows command prompt
scripts\package-windows.bat
```

---

## Usage Guide

1. **Verify Engine Toolchains (Optional)**:
   - On launch, the permanent status bar at the bottom displays real-time availability for all engines (`FFmpeg`, `Magick`, `Pandoc`).
   - Open **Settings (⚙️)** in the header bar if you need to supply custom executable path overrides, configure output directory behavior, or set up automated Watch Folders.
2. **Add Files**: Drag and drop any media, image, subtitle, or document files directly onto the drop zone, or click **Add Files**.
3. **Select Target Format or Preset**:
   - Choose an output format from the format selection dialog, pick from ready-made presets (*Discord*, *YouTube 1080p*, *High-Quality Animated GIF*, *Animated WebP*, *Lossless Audio*, *Web-ready Images*, *Markdown to PDF*), or click **+ New Preset** to build a custom recipe.
   - For lossy image formats, an interactive before & after preview dialog lets you inspect quality and estimated size before confirming.
4. **Automatic Conversion**:
   - Conversion starts immediately upon format selection—no extra clicks required.
   - Jobs execute concurrently in the batch queue with per-engine resource throttling (protecting GPU and memory bandwidth), real-time progress indicators, and cancellation support.
5. **Inspect Output & History**:
   - Double-click any completed job in the queue to reveal its output folder in your file manager.
   - Open **History (📜)** to inspect execution metrics, duration, copy detailed command diagnostics to clipboard, click **Open Output Folder**, or retry failed jobs with 1-click.


---

## Limitations

- **ImageMagick Delegates**: Complex image conversions (such as PostScript or Camera RAW) depend on external delegate libraries (`librsvg`, `libwebp`, `libraw`) present in your system ImageMagick installation.
- **Complex LaTeX Macros**: Document-to-PDF conversion uses bundled **Typst**, which is fast and lightweight (~54 MB) but does not execute arbitrary LaTeX packages (`\usepackage{...}`). For legacy LaTeX files with extensive TeX macro packages, install `xelatex` or `pdflatex` on your system.
- **Patented Codec Licensing**: FFmpeg binaries installed via your system package manager are subject to regional patent licensing laws for proprietary video codecs (H.264/HEVC/AAC).

---

## Licensing & Third-Party Credits

Universal Converter is licensed under the **[MIT License](LICENSE)**.

Third-party runtime components:
- **Qt 6 Framework**: GNU Lesser General Public License (LGPLv3).
- **Pandoc**: GNU General Public License (GPLv2 or later).
- **Typst**: Apache License 2.0.
- **ImageMagick**: ImageMagick License (Apache 2.0-compatible).
- **FFmpeg**: GNU General Public License (GPLv3 / LGPL depending on host build).

For license terms and third-party notices, see the **[LICENSE](LICENSE)** file.
