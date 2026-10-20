# Changelog

All notable changes to Universal Converter will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.5] - 2026-10-20

### Added
- **Hardware Acceleration Pipelines**: Full, verified implementation of Intel/AMD VA-API (`h264_vaapi`, `hevc_vaapi`, `vp9_vaapi`) and Intel QSV (`h264_qsv`, `hevc_qsv`, `vp9_qsv`) pipelines with explicit hardware device (`-vaapi_device /dev/dri/renderD128`) and `format=nv12,hwupload` filter insertion.
- **End-to-End Orchestrator Integration Tests**: Added automated integration test suite validating queue serialization, completion transitions, signal firing, and history persistence through `ConversionOrchestrator`.
- **Live Progress & State Tracking in UI**: Wired `jobProgressChanged` and `jobStateChanged` across `ConversionOrchestrator` and `QueueViewWidget` for real-time percentages, progress bars, and status updates.
- **Custom Preset Builder Speed & Aspect Controls**: Added encoder speed presets (`ultrafast`, `fast`, `medium`, `slow`, `slower`, `veryslow`) and `preserveAspectRatio` toggle to `PresetEditorDialog`, preserving user parameters on save.
- **Recursive Watch Folder Scanning**: Added recursive directory descent (`isRecursive`) using `QDirIterator` and full reconciliation for runtime addition, update, removal, and enable/disable of watch folders.

### Fixed
- **Job Lifecycle & Completion Reliability**: Resolved completion handling issue where finished terminal jobs were prematurely intercepted by guards, ensuring history recording, `jobFinished` emissions, and queue pumping execute reliably.
- **Engine Cancellation Signal Fidelity**: Fixed `jobFinished` emitting `Failed` on cancellation; cancellations now accurately carry `JobState::Cancelled`.
- **GUI Thread Responsiveness**: Completely eliminated main-thread blocking by offloading media stream probing (`MediaProbe`), SVG vector rendering (`VectorPipeline`), image inspection (`ImageInspector`), and image optimization passes (`ImageOptimizer`) to background worker thread pools.
- **Startup Toolchain Scan Gate**: Fixed startup race where immediate drag-and-drop before background toolchain resolution failed with missing executable errors; queue now cleanly defers launching until discovery completes.
- **Watch Folder Re-Conversion Under KeepOriginal**: Transitioned processed file tracking to a path-to-mtime map; memory bounds pruning now only purges files that no longer exist on disk.
- **Thread Safety in Toolchain Discovery**: Fixed cross-thread access on `AppSettings` during async scans by snapshotting path overrides on the calling thread.
- **TOCTOU Output Path Preservation**: Fixed race window in `OutputPathPlanner` by maintaining thread-safe path reservations across planning, backup sidecar creation, and publishing.
- **Atomic Serialized History Persistence**: Isolated history persistence to a dedicated single-threaded writer pool (`m_writerPool`), preventing out-of-order writes or concurrency between async and sync writes.
- **Format Selector Option Bleed**: Fixed preset options persisting across target format switches; changing target format now resets stale options.
- **Container/Codec Alignments**: Defaulted AVI audio codec to `libmp3lame`, 3GP to `mpeg4`, and restricted CRF parameters to supported codecs.
- **Thread Concurrency Defaults**: Bounded default parallel job concurrency to a safe maximum of 4 (range 1–8).

## [1.1.0] - 2026-10-16

### Added
- Gated state transitions in `ConversionJob` (`Failed -> Queued` on transient retries and `Preparing -> Queued` for engine re-dispatch) to prevent state-machine deadlocks.
- Atomic file publishing in `OutputPathPlanner::publishTempToFinal` across FFmpeg, ImageMagick, and Pandoc engines, preventing data loss by using sidecar backups prior to final replace.
- Extension sanitization in `OutputPathPlanner` to guard against path traversal attempts (`..`, `/`, `\`).
- Case-insensitive canonical path comparison for Windows and macOS file collision detection.
- Unified minimum file size validation helper `OutputPathPlanner::validateProducedFile`.
- Dimension upper-bound clamping (16384px) in `VectorPipeline` to prevent memory exhaustion on SVG rasterization.
- Default `Release` build type configuration and `-O2` compiler optimization flags in `CMakeLists.txt`.

### Fixed
- Fixed process launch classification in `ProcessRunner::executeSync`: missing or inaccessible binaries are now correctly reported as `FailedToStart` instead of `TimedOut`.
- Fixed process leakage on timeout in `ProcessRunner::executeSync` by forcefully terminating timed-out child processes.
- Aligned timeout semantics between synchronous and asynchronous process execution (`timeoutMs <= 0` represents no timeout).
- Fixed integer multiplication overflow during dimension scaling calculations in `MediaEstimator`.
- Fixed silent validation pass in `DocumentValidator` when seeking to PDF trailer markers fails.
- Fixed `ImageInspector` dimension parser accepting zero or negative dimensions on invalid image headers.
- Fixed `MediaProbe` marking empty probe results as valid when no streams or formats are detected.
- Fixed status bar and queue summary text remaining stuck on stale counts after conversions finish or after clicking "Clear Completed".
- Fixed UI stutter and tree recreation in `QueueViewWidget` by implementing incremental row upserts keyed by job ID.

### Changed
- Standard output and error buffers in `ProcessRunner` are now bounded to 2 MiB per stream to prevent unbounded RAM growth during long conversions.
- Progress parser buffer in `FfmpegProgressParser` capped at 1 MiB to prevent memory growth on malformed streams.
- Hardware encoder probes in `SystemInfo` optimized to a single-pass token scan with timeout process kills.
- Conversion history writes serialized using compact JSON representation, reducing disk I/O on job completion.
- Updated application and About dialog version strings to 1.1.0.

## [1.0.0] - 2026-10-14

### Added
- Initial public release of Universal Converter.
- Offline desktop conversion engine built with C++20 and Qt 6 Widgets.
- Audio, video, and container conversion powered by FFmpeg.
- Raster and vector image processing powered by ImageMagick and Qt SVG.
- Document and markup conversion powered by Pandoc.
- Watch folder automation service with size-stable debouncing.
- Custom preset creator and JSON configuration manager.
- Multi-tier subtitle encoding detector and converter (SRT, VTT, ASS, SSA, SUB).
- Interactive before-and-after image preview with live quality slider.
- Cross-platform packaging support (NSIS installer for Windows, portable archives, AppImage/tarball for Linux).
