# Universal Converter

<p align="center">
  <img src="resources/icons/app_icon_256.png" alt="Universal Converter" width="128">
</p>

[![Build & Package Universal Converter](https://github.com/IAmThroot/Universal-Converter/actions/workflows/package.yml/badge.svg)](https://github.com/IAmThroot/Universal-Converter/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-brightgreen.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt 6](https://img.shields.io/badge/Qt-6.4%2B-green.svg)](https://www.qt.io/)
[![Version: 1.1.6](https://img.shields.io/badge/Release-v1.1.6-emerald.svg)](https://github.com/IAmThroot/Universal-Converter/releases/tag/v1.1.6)

A high-performance, completely offline, native **C++20 / Qt 6** desktop application for converting audio, video, images, subtitles, and documents across formats without web runtimes, Electron, localhost daemons, or remote API dependencies.

---

## Features & Highlights

- **Native Architecture**: Built directly with modern C++20 and Qt 6 Widgets. Low memory footprint, fast startup, and native FreeDesktop/Windows integration.
- **Conversion Presets & Custom Preset Builder**: Ready-to-use profiles including *"Compress for Discord"*, *"YouTube 1080p"*, *"High-Quality Animated GIF"*, *"Animated WebP"*, *"Lossless Audio (FLAC)"*, and *"Markdown to PDF"*, plus an interactive GUI to build, save, export, and import custom JSON recipes.
- **Watch Folder Automation**: Background filesystem watcher with size-stable debouncing, custom output routing, and configurable post-processing triggers (*Move to Processed*, *Delete Original*, *Keep Original*).
- **Subtitles & Text Stream Engine**: Conversion between `SRT`, `VTT`, `ASS`, and `SSA`, with `.sub` supported as input only with automatic multi-tier character encoding detection (BOM, strict UTF-8, `uchardet`, `CP1252`/Latin-1), preventing decoding errors across international subtitle sets.
- **Video ↔ Animated Image Pipelines**: High-fidelity bidirectional conversion between video and animated images (`GIF`, `WebP`, `AVIF`) using dual-pass `palettegen`/`paletteuse` filters, looping controls, and audio stripping.
- **Modern Vector & Image Pipelines**: First-class SVG vector rasterization via `rsvg-convert` with a zero-dependency `Qt6::Svg` fallback, plus detection and execution hooks for standalone image optimization toolchains (`oxipng`, `pngquant`, `cwebp`, `avifenc`).
- **Interactive Before & After Image Preview**: Dual-panel comparison using reduced-resolution ImageMagick samples, a debounced quality slider, zoom, and a transparency checkerboard. Preview failure does not block conversion.
- **Dynamic Format Capability Resolver**: Route availability checks with blocking reasons based on installed tools, output containers, required encoders, and image/document capabilities. Actual input streams are inspected during conversion.
- **Conversion History & Retry**: Privacy-preserving persistent local history logging timestamps, file sizes, conversion duration, and job status with one-click retry.
- **Unified Engine Orchestration**:
  - **Video & Audio**: Multi-threaded encoding via FFmpeg, hardware acceleration where supported by the installed engine and available device (NVENC, VA-API, QSV), and accurate stream probing via FFprobe.
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

The registry describes known formats and permitted routes. The application checks installed tools and reported capabilities before enabling a route, and revalidates before dispatch. Input codec/delegate restrictions can still cause a conversion to fail. Registry test tiers are separate from runtime readiness and do not mean every file or host configuration has been tested.

### Registered formats (availability depends on installed tools)

| Engine | Category | Listed Count | Formats |
| :--- | :--- | :---: | :--- |
| **FFmpeg** | Video & Animated | 23 | `mp4`, `mkv`, `webm`, `avi`, `mov`, `flv`, `wmv`, `m4v`, `3gp`, `ts`, `vob`, `ogv`, `mpeg`, `mpg`, `m2ts`, `asf`, `f4v`, `y4m`, `dv`, `wtv`, animated `gif`, `webp`, `avif` |
| **FFmpeg** | Audio | 23 | `mp3`, `aac`, `ogg`, `opus`, `flac`, `wav`, `m4a`, `wma`, `aiff`, `alac`, `ac3`, `mka`, `amr`, `ape`, `ra`, `voc`, `au`, `caf`, `spx`, `mid`, `eac3`, `dts`, `wv` |
| **FFmpeg** | Subtitles | 5 | `srt`, `vtt`, `ass`, `ssa`, `sub` (input only; with automatic character encoding detection) |
| **ImageMagick & Vector** | Images & RAW | 50 | `png`, `jpg`, `jpeg`, `webp`, `gif`, `bmp`, `tiff`, `ico`, `svg`, `avif`, `tga`, `heic`, `dcm`, `cr2`, `nef`, `arw`, `dng`, `orf`, `rw2`, `pef`, `jxl`, `exr`, `pcx`, `pbm`, `pgm`, `ppm`, `pnm`, `xpm`, `xbm`, `wbmp`, `dds`, `dpx`, `cin`, `sgi`, `ras`, `qoi`, `mos`, `raw`, `3fr`, `erf`, `mrw`, `mef`, `srw`, `sr2`, `srf`, etc. |
| **Pandoc + Typst** | Documents & PDF | 16 | `md`, `markdown`, `docx`, `odt`, `epub`, `tex`, `rst`, `adoc`, `org`, `textile`, `wiki`, `opml`, `rtf`, `txt`, `pdf`, `html` |

> [!TIP]
> **Container vs. Codec Discovery**: An MP4 container requires supported video/audio codecs in your installed FFmpeg build; Camera RAW formats require ImageMagick delegate libraries (`libraw`, `librsvg`). Universal Converter dynamically probes your installed engine capabilities at launch and disables unavailable routes. Registry test tiers describe coverage, not guarantees for every host or file.


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
  - 💾 **Download `UniversalConverter-1.1.6-win64.exe`** (NSIS setup wizard with desktop shortcuts and uninstaller)
  - 🗜️ **Download `UniversalConverter-1.1.6-win64.zip`** (Portable package: extract and run `UniversalConverter.exe`)
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
- Qt 6.4+ (MSVC 2022 64-bit).
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

# Run automated tests (core/integration and Qt Widgets regression suites)
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

1. **Check tools**: Click **Tools: X/4 ready** for FFmpeg, ImageMagick, Pandoc, and PDF typesetter status. Configure custom paths in Settings.
2. **Add files**: Drop files onto the landing area or use File → Open Files.
3. **Review the batch**: Select rows and choose a format/preset. Review destinations and readiness. Resolve or remove blocked rows. Cancel starts no jobs; **Convert N Files** reserves unique output names and starts the batch.
4. **Inspect progress**: The queue shows progress and elapsed time, with speed/ETA when the engine reports enough information. Right-click for Open Output, Open Folder, Retry, Cancel, or Details.
5. **Inspect outcomes**: Summaries distinguish success, failure, cancellation, active work, and waiting jobs. History stores local diagnostics and retry options.
6. **Automate**: Open **Watch Folders** to configure folders, destinations, presets, recursion, and post-actions. New folders keep originals by default. Pause clears pending candidates and cancels queued jobs; running jobs finish using their captured settings. Use Retry Failures after resolving a problem. Watching runs while the application is open.
7. **Close safely**: During active conversion, choose Keep Open or Cancel Jobs and Exit. The application waits for workers to stop before destruction.

The Discord preset uses a fixed video/audio bitrate; it does not guarantee an upload-size limit. Image previews are reduced-resolution engine samples; final size and appearance can vary with the full-resolution conversion and optimizer.


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
