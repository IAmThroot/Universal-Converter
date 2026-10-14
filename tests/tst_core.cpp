#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QProcess>
#include "../src/app/ToolchainManager.h"
#include "../src/core/ConversionJob.h"
#include "../src/core/FormatRegistry.h"
#include "../src/core/ProcessRunner.h"
#include "../src/core/OutputPathPlanner.h"
#include "../src/core/ConversionOrchestrator.h"
#include "../src/core/SystemInfo.h"
#include "../src/engines/MediaProbe.h"
#include "../src/engines/FfmpegProgressParser.h"
#include "../src/engines/FfmpegArgBuilder.h"
#include "../src/engines/OutputValidator.h"
#include "../src/engines/FfmpegEngine.h"
#include "../src/engines/ImageMagickCapabilities.h"
#include "../src/engines/ImageMagickArgBuilder.h"
#include "../src/engines/ImageInspector.h"
#include "../src/engines/ImageMagickEngine.h"
#include "../src/engines/PandocCapabilities.h"
#include "../src/engines/PandocArgBuilder.h"
#include "../src/engines/DocumentValidator.h"
#include "../src/engines/PandocEngine.h"
#include "../src/core/PresetRegistry.h"
#include "../src/core/ConversionHistoryManager.h"
#include "../src/core/MediaEstimator.h"
#include "../src/core/EngineCapabilityResolver.h"
#include "../src/engines/VectorPipeline.h"
#include "../src/engines/ImageOptimizer.h"
#include "../src/core/WatchFolderService.h"

using namespace uc::core;

class CoreTests : public QObject {
    Q_OBJECT

private slots:
    // --- 1. ConversionJob Tests ---
    void testJobStateTransitions();
    void testInvalidJobStateTransitions();
    void testTerminalStateInvariance();
    void testJobCancellation();

    // --- 2. FormatRegistry Tests ---
    void testFormatNormalization();
    void testFormatLookup();
    void testFormatCompatibilityRules();
    void testExplicitlyUnsupportedFormats();

    // --- 3. OutputPathPlanner Tests ---
    void testOutputPathPlanningModes();
    void testPathCollisions();
    void testInPlaceOverwriteGuard();

    // --- 4. ProcessRunner Tests ---
    void testProcessRunnerSuccess();
    void testProcessRunnerFailureAndExitCode();
    void testProcessRunnerNonExistentExecutable();
    void testProcessRunnerCancellation();
    void testProcessRunnerExecuteSyncTimeout();
    void testProcessRunnerStructuredResult();

    // --- 5. ConversionOrchestrator Tests ---
    void testOrchestratorQueueConcurrency();
    void testQueuedJobCancellation();
    void testSlotReleaseAfterCompletion();

    // --- 6. FFmpeg Engine & Tool Tests (Phase 3) ---
    void testProgressParserIncrementalChunks();
    void testArgBuilderVideoToVideo();
    void testArgBuilderAudioExtraction();
    void testArgBuilderPathsWithSpacesAndUnicode();
    void testArgBuilderHardwareAccelerationMode();
    void testRealFfmpegConversionsEndToEnd();

    // --- 7. ImageMagick Engine & Tool Tests (Phase 4) ---
    void testImageMagickCapabilitiesDiscovery();
    void testImageMagickArgBuilderPngToJpeg();
    void testImageMagickArgBuilderResizeAndQuality();
    void testImageMagickArgBuilderMultiResolutionIco();
    void testRealImageMagickConversionsEndToEnd();

    // --- 8. Pandoc Engine & Tool Tests (Phase 5) ---
    void testPandocFormatMapping();
    void testPandocArgBuilderMarkdownToHtml();
    void testPandocArgBuilderMarkdownToDocx();
    void testPandocArgBuilderPdfEngineMissingGuard();
    void testDocumentValidatorZipContainers();
    void testRealPandocConversionsEndToEnd();

    // --- 9. EngineResolver Tests (Phase 8) ---
    void testEngineResolverUserOverrideWins();
    void testEngineResolverInvalidOverrideReportsError();
    void testEngineResolverBundledPrecedenceOverSystem();
    void testEngineResolverSystemFallback();
    void testEngineResolverMissingReportsError();
    void testEngineResolverPathsWithSpaces();
    void testEngineResolverCompanionPairing();

    // --- 10. v1.1 Feature Suite Tests ---
    void testPresetRegistryProfiles();
    void testConversionHistoryManagerPersistenceAndRetry();
    void testMediaEstimatorCalculations();
    void testEngineCapabilityResolverDistinction();

    // --- 11. Subtitle & Text Stream Engine Tests ---
    void testSubtitleFormatRegistrationAndCompatibility();
    void testSubtitleArgBuilderGeneration();
    void testRealSubtitleConversionsEndToEnd();

    // --- 12. Phase 4: Modern Image & Vector Pipelines Tests ---
    void testVectorPipelineFormatCompatibility();
    void testVectorPipelineQtSvgFallbackRasterization();
    void testVectorPipelineRsvgConvertSvgToPdfAndPng();
    void testImageOptimizerLosslessAndMetadataStripping();
    void testRealSvgToRasterConversionEndToEnd();

    // --- 13. Phase 2: Watch Folder Automation Daemon Tests ---
    void testWatchFolderConfigSerialization();
    void testWatchFolderServiceDetectionAndDebounce();
    void testWatchFolderServiceMoveToProcessed();

    // --- 14. Phase 3: Custom Preset Builder & Profile Import/Export Tests ---
    void testPresetRegistryJsonExportImport();
    void testPresetRegistryCustomProfilePersistence();

    // --- 15. Video <-> Animated Image (GIF, WebP, AVIF) Tests ---
    void testAnimatedCrossCategoryFormatCompatibility();
    void testVideoToAnimatedImageArgBuilder();
    void testRealVideoToAnimatedConversionsEndToEnd();
};

void CoreTests::testJobStateTransitions() {
    ConversionJob job("job-1", {"/tmp/test.mp4"}, "mp4", "webm", "/tmp/out");
    QCOMPARE(job.state(), JobState::Queued);
    QVERIFY(!job.isTerminal());

    QSignalSpy spy(&job, &ConversionJob::stateChanged);

    QVERIFY(job.transitionTo(JobState::Preparing));
    QCOMPARE(job.state(), JobState::Preparing);
    QCOMPARE(spy.count(), 1);

    QVERIFY(job.transitionTo(JobState::Running));
    QCOMPARE(job.state(), JobState::Running);
    QVERIFY(job.startedAt().isValid());

    job.setProgress(50, "Encoding frame 100");
    QCOMPARE(job.progressPercentage(), 50);

    QVERIFY(job.transitionTo(JobState::Completed));
    QCOMPARE(job.state(), JobState::Completed);
    QVERIFY(job.isTerminal());
    QVERIFY(job.completedAt().isValid());
}

void CoreTests::testInvalidJobStateTransitions() {
    ConversionJob job("job-2", {"/tmp/test.png"}, "png", "webp", "/tmp/out");
    // Cannot skip Preparing to go directly from Queued to Completed
    QVERIFY(!job.transitionTo(JobState::Completed));
    QCOMPARE(job.state(), JobState::Queued);
}

void CoreTests::testTerminalStateInvariance() {
    ConversionJob job("job-3", {"/tmp/test.docx"}, "docx", "pdf", "/tmp/out");
    job.transitionTo(JobState::Preparing);
    job.transitionTo(JobState::Running);
    job.transitionTo(JobState::Completed);

    // Completed job must never transition back to Running or Queued
    QVERIFY(!job.transitionTo(JobState::Running));
    QVERIFY(!job.transitionTo(JobState::Queued));
    QVERIFY(!job.transitionTo(JobState::Cancelled));
    QCOMPARE(job.state(), JobState::Completed);
}

void CoreTests::testJobCancellation() {
    ConversionJob job("job-4", {"/tmp/test.wav"}, "wav", "mp3", "/tmp/out");
    QSignalSpy cancelSpy(&job, &ConversionJob::cancellationRequested);

    job.requestCancellation();
    QVERIFY(job.isCancellationRequested());
    QCOMPARE(cancelSpy.count(), 1);

    QVERIFY(job.transitionTo(JobState::Cancelled));
    QCOMPARE(job.state(), JobState::Cancelled);
    QVERIFY(job.isTerminal());
}

void CoreTests::testFormatNormalization() {
    auto& reg = FormatRegistry::instance();

    QCOMPARE(reg.normalizeExtension(".JPG"), "jpeg");
    QCOMPARE(reg.normalizeExtension("jfif"), "jpeg");
    QCOMPARE(reg.normalizeExtension("tif"), "tiff");
    QCOMPARE(reg.normalizeExtension("asf"), "wmv");
    QCOMPARE(reg.normalizeExtension("ogx"), "ogv");
    QCOMPARE(reg.normalizeExtension("markdown"), "md");
}

void CoreTests::testFormatLookup() {
    auto& reg = FormatRegistry::instance();

    const auto* fmtMp4 = reg.findById("mp4");
    QVERIFY(fmtMp4 != nullptr);
    QCOMPARE(fmtMp4->category, MediaCategory::Video);

    const auto* fmtJpg = reg.findByExtension("jpg");
    QVERIFY(fmtJpg != nullptr);
    QCOMPARE(fmtJpg->id, "jpeg");

    const auto* fmtMd = reg.findByFilePath("/path/to/notes.markdown");
    QVERIFY(fmtMd != nullptr);
    QCOMPARE(fmtMd->id, "md");
    QCOMPARE(fmtMd->category, MediaCategory::Document);
}

void CoreTests::testFormatCompatibilityRules() {
    auto& reg = FormatRegistry::instance();

    // Video -> Video (Same category: FFmpeg)
    auto c1 = reg.checkCompatibility("mp4", "webm");
    QVERIFY(c1.isSupported);
    QCOMPARE(c1.responsibleEngine, "FFmpeg");

    // Video -> Audio (Audio extraction: FFmpeg)
    auto c2 = reg.checkCompatibility("mp4", "mp3");
    QVERIFY(c2.isSupported);
    QCOMPARE(c2.responsibleEngine, "FFmpeg");

    // Image -> Image (Same category: ImageMagick)
    auto c3 = reg.checkCompatibility("png", "webp");
    QVERIFY(c3.isSupported);
    QCOMPARE(c3.responsibleEngine, "ImageMagick");

    // Document -> Document (Same category: Pandoc)
    auto c4 = reg.checkCompatibility("md", "docx");
    QVERIFY(c4.isSupported);
    QCOMPARE(c4.responsibleEngine, "Pandoc");

    // Cross category invalid: Document -> Audio
    auto c5 = reg.checkCompatibility("docx", "mp3");
    QVERIFY(!c5.isSupported);

    // Identical format conversion rejected
    auto c6 = reg.checkCompatibility("mp4", "mp4");
    QVERIFY(!c6.isSupported);
}

void CoreTests::testExplicitlyUnsupportedFormats() {
    auto& reg = FormatRegistry::instance();

    // Legacy doc is explicitly disabled in VERT
    const auto* docFmt = reg.findById("doc");
    QVERIFY(docFmt != nullptr);
    QVERIFY(!docFmt->canInput);
    QVERIFY(!docFmt->canOutput);

    // RealMedia is input-only
    const auto* rmFmt = reg.findById("rm");
    QVERIFY(rmFmt != nullptr);
    QVERIFY(rmFmt->canInput);
    QVERIFY(!rmFmt->canOutput);

    auto rmOutputCheck = reg.checkCompatibility("mp4", "rm");
    QVERIFY(!rmOutputCheck.isSupported);
}

void CoreTests::testOutputPathPlanningModes() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString sampleFile = tempDir.filePath("sample.mp4");
    QFile f(sampleFile);
    QVERIFY(f.open(QFile::WriteOnly));
    f.write("dummy");
    f.close();

    // Mode: SameAsSource
    auto plan1 = OutputPathPlanner::planOutputPath(
        sampleFile, "webm", uc::app::DestinationMode::SameAsSource, "");
    QVERIFY(plan1.isValid);
    QCOMPARE(plan1.finalFilePath, tempDir.filePath("sample.webm"));

    // Mode: SourceConvertedSubfolder
    auto plan2 = OutputPathPlanner::planOutputPath(
        sampleFile, "webm", uc::app::DestinationMode::SourceConvertedSubfolder, "");
    QVERIFY(plan2.isValid);
    QCOMPARE(plan2.finalFilePath, tempDir.filePath("Converted/sample.webm"));
    QVERIFY(QDir(tempDir.filePath("Converted")).exists());
}

void CoreTests::testPathCollisions() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString src = tempDir.filePath("video.mp4");
    QFile f1(src);
    QVERIFY(f1.open(QFile::WriteOnly));
    f1.write("dummy");
    f1.close();

    // Pre-create video.webm to trigger collision
    const QString existing = tempDir.filePath("video.webm");
    QFile f2(existing);
    QVERIFY(f2.open(QFile::WriteOnly));
    f2.write("existing");
    f2.close();

    auto plan = OutputPathPlanner::planOutputPath(
        src, "webm", uc::app::DestinationMode::SameAsSource, "", CollisionPolicy::GenerateUniqueName);
    QVERIFY(plan.isValid);
    QCOMPARE(plan.finalFilePath, tempDir.filePath("video_1.webm"));
}

void CoreTests::testInPlaceOverwriteGuard() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString src = tempDir.filePath("document.txt");
    QFile f(src);
    QVERIFY(f.open(QFile::WriteOnly));
    f.write("text");
    f.close();

    // Attempting to overwrite source with exact same target extension
    auto plan = OutputPathPlanner::planOutputPath(
        src, "txt", uc::app::DestinationMode::SameAsSource, "", CollisionPolicy::Overwrite);
    QVERIFY(!plan.isValid);
    QVERIFY(plan.errorMessage.contains("identical"));
}

void CoreTests::testProcessRunnerSuccess() {
    ProcessRunner runner;
    QSignalSpy finishSpy(&runner, &ProcessRunner::finished);

#ifdef Q_OS_WIN
    QVERIFY(runner.start("cmd.exe", {"/c", "echo", "Hello", "VERT"}));
#else
    QVERIFY(runner.start("echo", {"Hello", "VERT"}));
#endif
    QVERIFY(finishSpy.wait(3000));

    QCOMPARE(finishSpy.count(), 1);
    const auto result = qvariant_cast<ProcessResult>(finishSpy.at(0).at(0));
    QVERIFY(result.isSuccess());
    QCOMPARE(result.exitCode, 0);
    QVERIFY(result.standardOutput.contains("Hello VERT"));
}

void CoreTests::testProcessRunnerFailureAndExitCode() {
    ProcessRunner runner;
    QSignalSpy finishSpy(&runner, &ProcessRunner::finished);

#ifdef Q_OS_WIN
    QVERIFY(runner.start("cmd.exe", {"/c", "exit", "1"}));
#else
    QVERIFY(runner.start("false", {}));
#endif
    QVERIFY(finishSpy.wait(3000));

    QCOMPARE(finishSpy.count(), 1);
    const auto result = qvariant_cast<ProcessResult>(finishSpy.at(0).at(0));
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.exitCode, 1);
}

void CoreTests::testProcessRunnerNonExistentExecutable() {
    ProcessRunner runner;
    QSignalSpy finishSpy(&runner, &ProcessRunner::finished);

    QVERIFY(runner.start("/nonexistent/binary_xyz123", {}));
    if (finishSpy.isEmpty()) {
        QVERIFY(finishSpy.wait(3000));
    }

    QCOMPARE(finishSpy.count(), 1);
    const auto result = qvariant_cast<ProcessResult>(finishSpy.at(0).at(0));
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.exitReason, ProcessExitReason::FailedToStart);
}

void CoreTests::testProcessRunnerCancellation() {
    ProcessRunner runner;
    QSignalSpy finishSpy(&runner, &ProcessRunner::finished);

#ifdef Q_OS_WIN
    QVERIFY(runner.start("ping", {"127.0.0.1", "-n", "10"}));
#else
    QVERIFY(runner.start("sleep", {"10"}));
#endif
    QVERIFY(runner.isRunning());

    runner.cancel(200);
    QVERIFY(finishSpy.wait(3000));

    QCOMPARE(finishSpy.count(), 1);
    const auto result = qvariant_cast<ProcessResult>(finishSpy.at(0).at(0));
    QCOMPARE(result.exitReason, ProcessExitReason::Cancelled);
}

void CoreTests::testProcessRunnerExecuteSyncTimeout() {
#ifdef Q_OS_WIN
    const QString prog = "ping";
    const QStringList args = {"127.0.0.1", "-n", "10"};
#else
    const QString prog = "sleep";
    const QStringList args = {"5"};
#endif

    const auto res = ProcessRunner::executeSync(prog, args, 300);
    QVERIFY(!res.isSuccess());
    QVERIFY(res.timedOut);
    QCOMPARE(res.exitReason, ProcessExitReason::TimedOut);
    QCOMPARE(res.program, prog);
    QCOMPARE(res.arguments, args);
    QVERIFY(res.durationMs >= 250);
}

void CoreTests::testProcessRunnerStructuredResult() {
#ifdef Q_OS_WIN
    const QString prog = "cmd.exe";
    const QStringList args = {"/c", "echo", "StructuredPayload"};
#else
    const QString prog = "echo";
    const QStringList args = {"StructuredPayload"};
#endif

    const auto res = ProcessRunner::executeSync(prog, args, 3000);
    QVERIFY(res.isSuccess());
    QVERIFY(!res.timedOut);
    QCOMPARE(res.exitCode, 0);
    QCOMPARE(res.program, prog);
    QCOMPARE(res.arguments, args);
    QVERIFY(res.standardOutput.contains("StructuredPayload"));
    QVERIFY(res.durationMs >= 0);
}

void CoreTests::testOrchestratorQueueConcurrency() {
    ConversionOrchestrator orchestrator(2);
    QCOMPARE(orchestrator.maxConcurrency(), 2);

    auto job1 = std::make_shared<ConversionJob>("j1", QStringList{"/tmp/1.mp4"}, "mp4", "webm", "/tmp");
    auto job2 = std::make_shared<ConversionJob>("j2", QStringList{"/tmp/2.mp4"}, "mp4", "webm", "/tmp");
    auto job3 = std::make_shared<ConversionJob>("j3", QStringList{"/tmp/3.mp4"}, "mp4", "webm", "/tmp");

    orchestrator.addJob(job1);
    orchestrator.addJob(job2);
    orchestrator.addJob(job3);

    QCOMPARE(orchestrator.totalJobCount(), 3);
}

void CoreTests::testQueuedJobCancellation() {
    ConversionOrchestrator orchestrator(1);

    auto job = std::make_shared<ConversionJob>("j-cancel", QStringList{"/tmp/in.mp4"}, "mp4", "webm", "/tmp");
    orchestrator.addJob(job);

    orchestrator.cancelJob("j-cancel");
    QCOMPARE(job->state(), JobState::Cancelled);
    QVERIFY(job->isTerminal());
}

void CoreTests::testSlotReleaseAfterCompletion() {
    ConversionOrchestrator orchestrator(2);

    auto job = std::make_shared<ConversionJob>("j-slot", QStringList{"/tmp/in.mp4"}, "mp4", "webm", "/tmp");
    orchestrator.addJob(job);

    orchestrator.cancelJob("j-slot");
    QCOMPARE(orchestrator.activeJobCount(), 0);
}

void CoreTests::testProgressParserIncrementalChunks() {
    using namespace uc::engines;
    FfmpegProgressParser parser(10.0); // 10 seconds total

    // Chunk 1: partial line
    auto s1 = parser.feedChunk("frame=12\nfps=24.0\nout_time_us=");
    QCOMPARE(s1.size(), 0);

    // Chunk 2: finish line and send progress
    auto s2 = parser.feedChunk("5000000\nspeed=2.0x\nprogress=continue\n");
    QCOMPARE(s2.size(), 1);
    QCOMPARE(s2[0].outTimeSeconds, 5.0);
    QCOMPARE(s2[0].speed, 2.0);
    QCOMPARE(parser.calculatePercentage(s2[0]), 50);

    // Chunk 3: end progress
    auto s3 = parser.feedChunk("progress=end\n");
    QCOMPARE(s3.size(), 1);
    QVERIFY(s3[0].isProgressEnd);
    QCOMPARE(parser.calculatePercentage(s3[0]), 100);
}

void CoreTests::testArgBuilderVideoToVideo() {
    using namespace uc::engines;
    MediaProbeResult probe;
    probe.isValid = true;
    probe.durationSeconds = 60.0;
    MediaStreamInfo vStream;
    vStream.codecType = "video";
    vStream.codecName = "h264";
    probe.streams.append(vStream);

    FfmpegConversionOptions opts;
    opts.targetFormatId = "webm";
    opts.preset = uc::engines::ConversionPreset::Medium;
    opts.hwMode = HardwareEncoderMode::Disabled;

    auto res = FfmpegArgBuilder::buildArguments("/in/file.mp4", "/out/file.webm", probe, opts, {});
    QVERIFY(res.isValid);
    QCOMPARE(res.chosenVideoCodec, "libvpx-vp9");
    QVERIFY(res.arguments.contains("-c:v"));
    QVERIFY(res.arguments.contains("libvpx-vp9"));
    QVERIFY(res.arguments.contains("-progress"));
    QVERIFY(res.arguments.contains("pipe:1"));
}

void CoreTests::testArgBuilderAudioExtraction() {
    using namespace uc::engines;
    MediaProbeResult probe;
    probe.isValid = true;
    probe.durationSeconds = 30.0;
    MediaStreamInfo vStream;
    vStream.codecType = "video";
    probe.streams.append(vStream);
    MediaStreamInfo aStream;
    aStream.codecType = "audio";
    probe.streams.append(aStream);

    FfmpegConversionOptions opts;
    opts.targetFormatId = "mp3";

    auto res = FfmpegArgBuilder::buildArguments("/in/movie.mkv", "/out/audio.mp3", probe, opts, {});
    QVERIFY(res.isValid);
    QCOMPARE(res.chosenAudioCodec, "libmp3lame");
    QVERIFY(res.arguments.contains("-vn"));
    QVERIFY(res.arguments.contains("libmp3lame"));
}

void CoreTests::testArgBuilderPathsWithSpacesAndUnicode() {
    using namespace uc::engines;
    MediaProbeResult probe;
    probe.isValid = true;
    MediaStreamInfo vStream;
    vStream.codecType = "video";
    probe.streams.append(vStream);

    FfmpegConversionOptions opts;
    opts.targetFormatId = "mp4";

    const QString inPath = "/home/user/My Videos/日本語 テスト/clip.mov";
    const QString outPath = "/home/user/My Videos/日本語 テスト/clip_converted.mp4";

    auto res = FfmpegArgBuilder::buildArguments(inPath, outPath, probe, opts, {});
    QVERIFY(res.isValid);
    // Arguments must contain raw paths without artificial shell quote escaping
    QVERIFY(res.arguments.contains(inPath));
    QVERIFY(res.arguments.contains(outPath));
}

void CoreTests::testArgBuilderHardwareAccelerationMode() {
    using namespace uc::engines;
    MediaProbeResult probe;
    probe.isValid = true;
    MediaStreamInfo vStream;
    vStream.codecType = "video";
    probe.streams.append(vStream);

    FfmpegConversionOptions opts;
    opts.targetFormatId = "mp4";
    opts.hwMode = HardwareEncoderMode::Auto;

    // Simulate available NVENC
    auto resNvenc = FfmpegArgBuilder::buildArguments("/in/a.mkv", "/out/b.mp4", probe, opts, {"h264_nvenc"});
    QVERIFY(resNvenc.isValid);
    QCOMPARE(resNvenc.chosenVideoCodec, "h264_nvenc");
    QVERIFY(resNvenc.isHardwareAccelerated);

    // Simulate no hardware encoders available
    auto resCpu = FfmpegArgBuilder::buildArguments("/in/a.mkv", "/out/b.mp4", probe, opts, {});
    QVERIFY(resCpu.isValid);
    QCOMPARE(resCpu.chosenVideoCodec, "libx264");
    QVERIFY(!resCpu.isHardwareAccelerated);
}

void CoreTests::testRealFfmpegConversionsEndToEnd() {
    using namespace uc::engines;
    auto& tc = uc::app::ToolchainManager::instance();
    tc.scanAll();

    if (!tc.ffmpeg().isAvailable || !tc.ffprobe().isAvailable) {
        QSKIP("FFmpeg or FFprobe is not available on host system.");
    }

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString srcVideo = tempDir.filePath("test_src.mp4");

    // 1. Generate tiny 1-second synthetic video using host FFmpeg
    QProcess genProc;
    genProc.start(tc.ffmpeg().executablePath, {
        "-y", "-f", "lavfi", "-i", "testsrc=size=320x240:rate=25",
        "-f", "lavfi", "-i", "sine=frequency=1000:sample_rate=44100",
        "-t", "1.0", "-c:v", "libx264", "-c:a", "aac", srcVideo
    });
    QVERIFY(genProc.waitForFinished(5000));
    QCOMPARE(genProc.exitCode(), 0);
    QVERIFY(QFile::exists(srcVideo));

    // 2. Probe generated file
    auto probe = MediaProbe::probeFile(tc.ffprobe().executablePath, srcVideo);
    QVERIFY(probe.isValid);
    QVERIFY(probe.hasVideo());
    QVERIFY(probe.hasAudio());
    QVERIFY(probe.durationSeconds >= 0.9);

    // 3. Test real Video-to-Video conversion (MP4 -> WebM via VP9)
    const QString outWebm = tempDir.filePath("test_out.webm");
    auto jobVideo = std::make_shared<ConversionJob>("job-v2v", QStringList{srcVideo}, "mp4", "webm", tempDir.path(), EngineType::FFmpeg);
    jobVideo->setFinalOutputPath(outWebm);

    FfmpegEngine engine;
    QSignalSpy videoFinishSpy(&engine, &FfmpegEngine::jobFinished);

    QVERIFY(engine.executeJob(jobVideo));
    const bool waited = videoFinishSpy.wait(15000);
    qDebug() << "videoFinishSpy waited:" << waited << "count:" << videoFinishSpy.count()
             << "job state:" << static_cast<int>(jobVideo->state())
             << "statusMsg:" << jobVideo->statusMessage()
             << "errorMsg:" << jobVideo->errorMessage();
    QVERIFY(waited);
    QCOMPARE(jobVideo->state(), JobState::Completed);
    QVERIFY(QFile::exists(outWebm));

    auto valWebm = OutputValidator::validate(tc.ffprobe().executablePath, outWebm, "webm");
    QVERIFY(valWebm.isValid);

    // 4. Test real Video-to-Audio extraction (MP4 -> MP3)
    const QString outMp3 = tempDir.filePath("test_audio.mp3");
    auto jobAudio = std::make_shared<ConversionJob>("job-v2a", QStringList{srcVideo}, "mp4", "mp3", tempDir.path(), EngineType::FFmpeg);
    jobAudio->setFinalOutputPath(outMp3);

    QSignalSpy audioFinishSpy(&engine, &FfmpegEngine::jobFinished);
    QVERIFY(engine.executeJob(jobAudio));
    QVERIFY(audioFinishSpy.wait(10000));
    QCOMPARE(jobAudio->state(), JobState::Completed);
    QVERIFY(QFile::exists(outMp3));

    auto valMp3 = OutputValidator::validate(tc.ffprobe().executablePath, outMp3, "mp3", true);
    QVERIFY(valMp3.isValid);

    // 5. Test real GPU NVENC conversion if supported by driver
    const auto sysCaps = uc::core::SystemInfo::probe();
    if (sysCaps.hasNvenc && sysCaps.availableHwEncoders.contains("h264_nvenc")) {
        const QString outNvenc = tempDir.filePath("test_nvenc.mkv");
        auto jobNvenc = std::make_shared<ConversionJob>("job-nvenc", QStringList{srcVideo}, "mp4", "mkv", tempDir.path(), EngineType::FFmpeg);
        jobNvenc->setFinalOutputPath(outNvenc);

        QSignalSpy nvencFinishSpy(&engine, &FfmpegEngine::jobFinished);
        QVERIFY(engine.executeJob(jobNvenc));
        QVERIFY(nvencFinishSpy.wait(10000));
        QCOMPARE(jobNvenc->state(), JobState::Completed);
        QVERIFY(QFile::exists(outNvenc));
    }
}

void CoreTests::testImageMagickCapabilitiesDiscovery() {
    using namespace uc::engines;
    auto& caps = ImageMagickCapabilities::instance();
    const auto& tc = uc::app::ToolchainManager::instance();
    if (!tc.magick().isAvailable) {
        QSKIP("ImageMagick executable is not available on host system.");
    }

    caps.discover(tc.magick().executablePath);
    QVERIFY(caps.isDiscovered());
    QVERIFY(caps.totalRecognizedFormats() > 100);
    QVERIFY(caps.canReadFormat("PNG"));
    QVERIFY(caps.canWriteFormat("JPEG"));
    QVERIFY(caps.canReadFormat("CR2")); // RAW readable
    QVERIFY(!caps.canWriteFormat("CR2")); // RAW not writable
    QVERIFY(caps.supportsAnimation("GIF"));
    QVERIFY(caps.supportsAnimation("WEBP"));
}

void CoreTests::testImageMagickArgBuilderPngToJpeg() {
    using namespace uc::engines;
    ImageConversionOptions opts;
    opts.targetFormatId = "jpeg";
    opts.quality = 90;

    auto res = ImageMagickArgBuilder::buildArguments("/in/image.png", "/out/image.jpeg", opts);
    QVERIFY(res.isValid);
    QVERIFY(res.arguments.contains("/in/image.png"));
    QVERIFY(res.arguments.contains("-quality"));
    QVERIFY(res.arguments.contains("90"));
    QVERIFY(res.arguments.contains("/out/image.jpeg"));
}

void CoreTests::testImageMagickArgBuilderResizeAndQuality() {
    using namespace uc::engines;
    ImageConversionOptions opts;
    opts.targetFormatId = "webp";
    opts.width = 800;
    opts.height = 600;
    opts.preserveAspectRatio = true;
    opts.stripMetadata = true;

    auto res = ImageMagickArgBuilder::buildArguments("/in/photo.jpg", "/out/photo.webp", opts);
    QVERIFY(res.isValid);
    QVERIFY(res.arguments.contains("-resize"));
    QVERIFY(res.arguments.contains("800x600"));
    QVERIFY(res.arguments.contains("-strip"));
}

void CoreTests::testImageMagickArgBuilderMultiResolutionIco() {
    using namespace uc::engines;
    ImageConversionOptions opts;
    opts.targetFormatId = "ico";
    opts.generateMultiResolutionIcon = true;
    opts.iconSizes = {16, 32, 64};

    auto res = ImageMagickArgBuilder::buildArguments("/in/logo.png", "/out/favicon.ico", opts);
    QVERIFY(res.isValid);
    QVERIFY(res.arguments.contains("16x16"));
    QVERIFY(res.arguments.contains("32x32"));
    QVERIFY(res.arguments.contains("64x64"));
    QVERIFY(res.arguments.contains("-delete"));
    QVERIFY(res.arguments.contains("/out/favicon.ico"));
}

void CoreTests::testRealImageMagickConversionsEndToEnd() {
    using namespace uc::engines;
    const auto& tc = uc::app::ToolchainManager::instance();
    if (!tc.magick().isAvailable) {
        QSKIP("ImageMagick executable is not available on host system.");
    }

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // 1. Generate a tiny test PNG with transparent alpha using ImageMagick
    const QString srcPng = tempDir.filePath("sample.png");
    QProcess genProc;
    genProc.start(tc.magick().executablePath, {
        "-size", "100x100", "xc:none",
        "-fill", "red", "-draw", "circle 50,50 50,20",
        srcPng
    });
    QVERIFY(genProc.waitForFinished(5000));
    QCOMPARE(genProc.exitCode(), 0);
    QVERIFY(QFile::exists(srcPng));

    ImageMagickEngine engine;

    // 2. Test PNG to JPEG (with resize)
    const QString outJpg = tempDir.filePath("converted.jpg");
    auto jobJpg = std::make_shared<ConversionJob>("job-img-1", QStringList{srcPng}, "png", "jpeg", tempDir.path(), EngineType::ImageMagick);
    jobJpg->setFinalOutputPath(outJpg);
    QVariantMap opts;
    opts["quality"] = 85;
    opts["width"] = 64;
    opts["height"] = 64;
    jobJpg->setOptions(opts);

    QSignalSpy jpgFinishSpy(&engine, &ImageMagickEngine::jobFinished);
    QVERIFY(engine.executeJob(jobJpg));
    QVERIFY(jpgFinishSpy.wait(5000));
    QCOMPARE(jobJpg->state(), JobState::Completed);
    QVERIFY(QFile::exists(outJpg));

    auto inspectJpg = ImageInspector::inspect(tc.magick().executablePath, outJpg);
    QVERIFY(inspectJpg.isValid);
    QCOMPARE(inspectJpg.width, 64);
    QCOMPARE(inspectJpg.height, 64);

    // 3. Test multi-resolution ICO generation
    const QString outIco = tempDir.filePath("icon.ico");
    auto jobIco = std::make_shared<ConversionJob>("job-ico", QStringList{srcPng}, "png", "ico", tempDir.path(), EngineType::ImageMagick);
    jobIco->setFinalOutputPath(outIco);

    QSignalSpy icoFinishSpy(&engine, &ImageMagickEngine::jobFinished);
    QVERIFY(engine.executeJob(jobIco));
    QVERIFY(icoFinishSpy.wait(5000));
    QCOMPARE(jobIco->state(), JobState::Completed);
    QVERIFY(QFile::exists(outIco));

    auto inspectIco = ImageInspector::inspect(tc.magick().executablePath, outIco);
    QVERIFY(inspectIco.isValid);
    QVERIFY(inspectIco.frameCount >= 4); // 16, 32, 48, 64, 128, 256

    // 4. Test animated GIF generation (multi-frame preservation)
    const QString srcGif = tempDir.filePath("animated.gif");
    QProcess gifGenProc;
    gifGenProc.start(tc.magick().executablePath, {
        "-delay", "10",
        "-size", "50x50", "xc:blue",
        "-size", "50x50", "xc:green",
        "-size", "50x50", "xc:red",
        srcGif
    });
    QVERIFY(gifGenProc.waitForFinished(5000));
    QCOMPARE(gifGenProc.exitCode(), 0);
    QVERIFY(QFile::exists(srcGif));

    const QString outWebpAnim = tempDir.filePath("animated.webp");
    auto jobAnim = std::make_shared<ConversionJob>("job-anim", QStringList{srcGif}, "gif", "webp", tempDir.path(), EngineType::ImageMagick);
    jobAnim->setFinalOutputPath(outWebpAnim);

    QSignalSpy animFinishSpy(&engine, &ImageMagickEngine::jobFinished);
    QVERIFY(engine.executeJob(jobAnim));
    QVERIFY(animFinishSpy.wait(5000));
    QCOMPARE(jobAnim->state(), JobState::Completed);
    QVERIFY(QFile::exists(outWebpAnim));

    auto inspectWebp = ImageInspector::inspect(tc.magick().executablePath, outWebpAnim);
    QVERIFY(inspectWebp.isValid);
    QCOMPARE(inspectWebp.frameCount, 3); // Preserved all 3 frames
}

void CoreTests::testPandocFormatMapping() {
    using namespace uc::engines;
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("md"), "markdown");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("htm"), "html");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("tex"), "latex");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("docx"), "docx");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("odt"), "odt");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("epub"), "epub");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("rst"), "rst");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("txt"), "markdown");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("adoc"), "asciidoc");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("org"), "org");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("textile"), "textile");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("wiki"), "mediawiki");
    QCOMPARE(PandocArgBuilder::mapExtensionToPandocFormat("opml"), "opml");
}

void CoreTests::testPandocArgBuilderMarkdownToHtml() {
    using namespace uc::engines;
    DocumentConversionOptions opts;
    opts.targetFormatId = "html";
    opts.standalone = true;
    opts.generateToc = true;
    opts.tocDepth = 2;
    opts.documentTitle = "User Guide";

    PandocCapabilities caps; // empty caps
    auto res = PandocArgBuilder::buildArguments("/in/guide.md", "/out/guide.html", opts, caps);
    QVERIFY(res.isValid);
    QVERIFY(res.arguments.contains("-f"));
    QVERIFY(res.arguments.contains("markdown"));
    QVERIFY(res.arguments.contains("-t"));
    QVERIFY(res.arguments.contains("html"));
    QVERIFY(res.arguments.contains("--standalone"));
    QVERIFY(res.arguments.contains("--toc"));
    QVERIFY(res.arguments.contains("--toc-depth"));
    QVERIFY(res.arguments.contains("2"));
    QVERIFY(res.arguments.contains("--metadata"));
    QVERIFY(res.arguments.contains("title=User Guide"));
    QVERIFY(res.arguments.contains("/in/guide.md"));
    QVERIFY(res.arguments.contains("/out/guide.html"));
}

void CoreTests::testPandocArgBuilderMarkdownToDocx() {
    using namespace uc::engines;
    DocumentConversionOptions opts;
    opts.targetFormatId = "docx";

    PandocCapabilities caps;
    auto res = PandocArgBuilder::buildArguments("/in/spec.md", "/out/spec.docx", opts, caps);
    QVERIFY(res.isValid);
    QVERIFY(res.arguments.contains("-f"));
    QVERIFY(res.arguments.contains("markdown"));
    QVERIFY(res.arguments.contains("-t"));
    QVERIFY(res.arguments.contains("docx"));
    // docx does not take --standalone (docx is always standalone container)
    QVERIFY(!res.arguments.contains("--standalone"));
    QVERIFY(res.arguments.contains("/out/spec.docx"));
}

void CoreTests::testPandocArgBuilderPdfEngineMissingGuard() {
    using namespace uc::engines;
    DocumentConversionOptions opts;
    opts.targetFormatId = "pdf";

    PandocCapabilities caps; // no pdf engines discovered
    auto res = PandocArgBuilder::buildArguments("/in/doc.md", "/out/doc.pdf", opts, caps);
    QVERIFY(!res.isValid);
    QVERIFY(res.errorMessage.contains("external PDF engine"));
}

namespace {
// Self-contained, portable mock ZIP writer for unit testing without private Qt headers
void writeZipMock(const QString& targetPath, const QList<QPair<QString, QByteArray>>& entries) {
    QFile f(targetPath);
    if (!f.open(QFile::WriteOnly)) return;

    struct EntryMeta {
        QByteArray name;
        qint64 size;
        qint64 offset;
    };
    QList<EntryMeta> metas;
    qint64 offset = 0;

    for (const auto& pair : entries) {
        const QByteArray nameBytes = pair.first.toUtf8();
        const QByteArray contentBytes = pair.second;

        // Local header: PK\x03\x04
        f.write("PK\x03\x04");
        const quint16 ver = 20, flags = 0, method = 0, time = 0, date = 0;
        const quint32 crc = 0, csize = contentBytes.size(), usize = contentBytes.size();
        const quint16 nlen = nameBytes.size(), xlen = 0;

        f.write(reinterpret_cast<const char*>(&ver), 2);
        f.write(reinterpret_cast<const char*>(&flags), 2);
        f.write(reinterpret_cast<const char*>(&method), 2);
        f.write(reinterpret_cast<const char*>(&time), 2);
        f.write(reinterpret_cast<const char*>(&date), 2);
        f.write(reinterpret_cast<const char*>(&crc), 4);
        f.write(reinterpret_cast<const char*>(&csize), 4);
        f.write(reinterpret_cast<const char*>(&usize), 4);
        f.write(reinterpret_cast<const char*>(&nlen), 2);
        f.write(reinterpret_cast<const char*>(&xlen), 2);
        f.write(nameBytes);
        f.write(contentBytes);

        EntryMeta meta;
        meta.name = nameBytes;
        meta.size = contentBytes.size();
        meta.offset = offset;
        metas.append(meta);

        offset += 30 + nameBytes.size() + contentBytes.size();
    }

    const qint64 cdStart = f.pos();
    for (const auto& meta : metas) {
        // Central Directory header: PK\x01\x02
        f.write("PK\x01\x02");
        const quint16 verMade = 20, verNeed = 20, flags = 0, method = 0, time = 0, date = 0;
        const quint32 crc = 0, csize = meta.size, usize = meta.size;
        const quint16 nlen = meta.name.size(), xlen = 0, clen = 0, disk = 0, intAttr = 0;
        const quint32 extAttr = 0, lclOffset = meta.offset;

        f.write(reinterpret_cast<const char*>(&verMade), 2);
        f.write(reinterpret_cast<const char*>(&verNeed), 2);
        f.write(reinterpret_cast<const char*>(&flags), 2);
        f.write(reinterpret_cast<const char*>(&method), 2);
        f.write(reinterpret_cast<const char*>(&time), 2);
        f.write(reinterpret_cast<const char*>(&date), 2);
        f.write(reinterpret_cast<const char*>(&crc), 4);
        f.write(reinterpret_cast<const char*>(&csize), 4);
        f.write(reinterpret_cast<const char*>(&usize), 4);
        f.write(reinterpret_cast<const char*>(&nlen), 2);
        f.write(reinterpret_cast<const char*>(&xlen), 2);
        f.write(reinterpret_cast<const char*>(&clen), 2);
        f.write(reinterpret_cast<const char*>(&disk), 2);
        f.write(reinterpret_cast<const char*>(&intAttr), 2);
        f.write(reinterpret_cast<const char*>(&extAttr), 4);
        f.write(reinterpret_cast<const char*>(&lclOffset), 4);
        f.write(meta.name);
    }

    const qint64 cdSize = f.pos() - cdStart;

    // End of Central Directory: PK\x05\x06
    f.write("PK\x05\x06");
    const quint16 diskNum = 0, diskStart = 0;
    const quint16 totalRec = metas.size(), totalEntries = metas.size();
    const quint32 cdSz = cdSize, cdOff = cdStart;
    const quint16 commentLen = 0;

    f.write(reinterpret_cast<const char*>(&diskNum), 2);
    f.write(reinterpret_cast<const char*>(&diskStart), 2);
    f.write(reinterpret_cast<const char*>(&totalRec), 2);
    f.write(reinterpret_cast<const char*>(&totalEntries), 2);
    f.write(reinterpret_cast<const char*>(&cdSz), 4);
    f.write(reinterpret_cast<const char*>(&cdOff), 4);
    f.write(reinterpret_cast<const char*>(&commentLen), 2);
    f.close();
}
} // namespace

void CoreTests::testDocumentValidatorZipContainers() {
    using namespace uc::engines;
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // 1. Valid DOCX archive created with mock ZIP
    const QString validDocx = tempDir.filePath("valid.docx");
    writeZipMock(validDocx, {
        {"[Content_Types].xml", "<?xml version=\"1.0\"?><Types></Types>"},
        {"word/document.xml", "<?xml version=\"1.0\"?><w:document></w:document>"}
    });
    auto valGoodDocx = DocumentValidator::validate(validDocx, "docx");
    QVERIFY(valGoodDocx.isValid);
    QCOMPARE(valGoodDocx.detectedType, QString("zip-package/docx"));

    // 2. Corrupt / Truncated DOCX archive
    const QString truncatedDocx = tempDir.filePath("truncated.docx");
    {
        QFile f(truncatedDocx);
        QVERIFY(f.open(QFile::WriteOnly));
        f.write("PK\x03\x04" "truncated broken zip archive bytes that cannot be read with extra padding to exceed one hundred bytes total length in the file");
        f.close();
    }
    auto valTrunc = DocumentValidator::validate(truncatedDocx, "docx");
    QVERIFY(!valTrunc.isValid);
    QVERIFY(valTrunc.errorMessage.contains("corrupt") || valTrunc.errorMessage.contains("record"));

    // 3. ZIP archive missing required DOCX entries (e.g. missing word/document.xml)
    const QString missingEntryDocx = tempDir.filePath("missing_entry.docx");
    writeZipMock(missingEntryDocx, {
        {"[Content_Types].xml", "<?xml version=\"1.0\"?><Types></Types>"}
    });
    auto valMissing = DocumentValidator::validate(missingEntryDocx, "docx");
    QVERIFY(!valMissing.isValid);
    QVERIFY(valMissing.errorMessage.contains("word/document.xml"));

    // 4. Valid ODT archive
    const QString validOdt = tempDir.filePath("valid.odt");
    writeZipMock(validOdt, {
        {"mimetype", "application/vnd.oasis.opendocument.text"},
        {"content.xml", "<?xml version=\"1.0\"?><office:document-content></office:document-content>"}
    });
    auto valGoodOdt = DocumentValidator::validate(validOdt, "odt");
    QVERIFY(valGoodOdt.isValid);
    QCOMPARE(valGoodOdt.detectedType, QString("zip-package/odt"));

    // 5. Valid EPUB archive
    const QString validEpub = tempDir.filePath("valid.epub");
    writeZipMock(validEpub, {
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml", "<?xml version=\"1.0\"?><container></container>"}
    });
    auto valGoodEpub = DocumentValidator::validate(validEpub, "epub");
    QVERIFY(valGoodEpub.isValid);
    QCOMPARE(valGoodEpub.detectedType, QString("zip-package/epub"));

    // 6. Valid PDF with %%EOF trailer
    const QString validPdf = tempDir.filePath("valid.pdf");
    {
        QFile f(validPdf);
        QVERIFY(f.open(QFile::WriteOnly));
        f.write("%PDF-1.7\n1 0 obj\n<< /Type /Catalog >>\nendobj\nxref\ntrailer\n<< /Root 1 0 R >>\nstartxref\n100\n%%EOF\n");
        f.close();
    }
    auto valGoodPdf = DocumentValidator::validate(validPdf, "pdf");
    QVERIFY(valGoodPdf.isValid);
    QCOMPARE(valGoodPdf.detectedType, QString("application/pdf"));

    // 7. Broken PDF without %%EOF trailer
    const QString brokenPdf = tempDir.filePath("broken.pdf");
    {
        QFile f(brokenPdf);
        QVERIFY(f.open(QFile::WriteOnly));
        f.write("%PDF-1.7\n1 0 obj\n<< /Type /Catalog >>\nendobj\ntruncated before trailer");
        f.close();
    }
    auto valBrokenPdf = DocumentValidator::validate(brokenPdf, "pdf");
    QVERIFY(!valBrokenPdf.isValid);
    QVERIFY(valBrokenPdf.errorMessage.contains("%%EOF"));

    // 8. Plain text file masquerading as docx
    const QString fakeDocx = tempDir.filePath("fake.docx");
    {
        QFile f(fakeDocx);
        QVERIFY(f.open(QFile::WriteOnly));
        f.write("Plain text masquerading as docx");
        f.close();
    }
    auto valBad = DocumentValidator::validate(fakeDocx, "docx");
    QVERIFY(!valBad.isValid);
    QVERIFY(valBad.errorMessage.contains("ZIP container"));
}

void CoreTests::testRealPandocConversionsEndToEnd() {
    using namespace uc::engines;
    const auto& tc = uc::app::ToolchainManager::instance();
    if (!tc.pandoc().isAvailable) {
        QSKIP("Pandoc executable is not available on host system.");
    }

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Sample markdown source
    const QString mdFile = tempDir.filePath("sample.md");
    QFile f(mdFile);
    QVERIFY(f.open(QFile::WriteOnly));
    f.write("# Introduction\n\nThis is a sample document for Universal Converter.\n\n## Features\n\n- Cross-platform\n- Offline\n");
    f.close();

    PandocEngine engine;

    // 1. Markdown to HTML
    const QString htmlFile = tempDir.filePath("output.html");
    auto jobHtml = std::make_shared<ConversionJob>("job-doc-html", QStringList{mdFile}, "md", "html", tempDir.path(), EngineType::Pandoc);
    jobHtml->setFinalOutputPath(htmlFile);

    QSignalSpy htmlSpy(&engine, &PandocEngine::jobFinished);
    QVERIFY(engine.executeJob(jobHtml));
    QVERIFY(htmlSpy.wait(5000));
    QCOMPARE(jobHtml->state(), JobState::Completed);
    QVERIFY(QFile::exists(htmlFile));

    auto valHtml = DocumentValidator::validate(htmlFile, "html");
    QVERIFY(valHtml.isValid);

    // 2. Markdown to DOCX
    const QString docxFile = tempDir.filePath("output.docx");
    auto jobDocx = std::make_shared<ConversionJob>("job-doc-docx", QStringList{mdFile}, "md", "docx", tempDir.path(), EngineType::Pandoc);
    jobDocx->setFinalOutputPath(docxFile);

    QSignalSpy docxSpy(&engine, &PandocEngine::jobFinished);
    QVERIFY(engine.executeJob(jobDocx));
    QVERIFY(docxSpy.wait(5000));
    QCOMPARE(jobDocx->state(), JobState::Completed);
    QVERIFY(QFile::exists(docxFile));

    auto valDocx = DocumentValidator::validate(docxFile, "docx");
    QVERIFY(valDocx.isValid);

    // 3. Markdown to PDF (via Typst)
    const QString pdfFile = tempDir.filePath("output.pdf");
    auto jobPdf = std::make_shared<ConversionJob>("job-doc-pdf", QStringList{mdFile}, "md", "pdf", tempDir.path(), EngineType::Pandoc);
    jobPdf->setFinalOutputPath(pdfFile);

    QSignalSpy pdfSpy(&engine, &PandocEngine::jobFinished);
    bool ok = engine.executeJob(jobPdf);
    if (!ok) {
        qWarning() << "executeJob failed with error:" << jobPdf->errorMessage();
    }
    QVERIFY(ok);
    QVERIFY(pdfSpy.wait(8000));
    QCOMPARE(jobPdf->state(), JobState::Completed);
    QVERIFY(QFile::exists(pdfFile));

    auto valPdf = DocumentValidator::validate(pdfFile, "pdf");
    QVERIFY(valPdf.isValid);
    QCOMPARE(valPdf.detectedType, QString("application/pdf"));
}

void CoreTests::testEngineResolverUserOverrideWins() {
    using namespace uc::app;
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Create a mock executable that outputs mock version
#ifdef Q_OS_WIN
    const QString fakeExe = tempDir.filePath("mock_ffmpeg.bat");
    QFile f(fakeExe);
    QVERIFY(f.open(QFile::WriteOnly));
    f.write("@echo mock_ffmpeg version 99.0\r\n");
    f.close();
#else
    const QString fakeExe = tempDir.filePath("mock_ffmpeg");
    QFile f(fakeExe);
    QVERIFY(f.open(QFile::WriteOnly));
    f.write("#!/bin/sh\necho 'mock_ffmpeg version 99.0'\nexit 0\n");
    f.close();
    QFile::setPermissions(fakeExe, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
#endif

    auto res = EngineResolver::resolve("ffmpeg", fakeExe, "-version", tempDir.path());
    QVERIFY(res.isAvailable);
    QCOMPARE(res.source, ToolSource::UserOverride);
    QCOMPARE(res.executablePath, fakeExe);
    QCOMPARE(res.version, QString("mock_ffmpeg version 99.0"));
}

void CoreTests::testEngineResolverInvalidOverrideReportsError() {
    using namespace uc::app;
    const QString nonExistent = "/non/existent/path/to/custom_ffmpeg";
    auto res = EngineResolver::resolve("ffmpeg", nonExistent, "-version");
    QVERIFY(!res.isAvailable);
    QCOMPARE(res.source, ToolSource::UserOverride);
    QVERIFY(res.failureReason.contains("does not exist"));
}

void CoreTests::testEngineResolverBundledPrecedenceOverSystem() {
    using namespace uc::app;
    QTemporaryDir tempAppDir;
    QVERIFY(tempAppDir.isValid());

    // Create a mock bundled engine under engines/ffmpeg/ffmpeg(.bat)
    QDir(tempAppDir.path()).mkpath("engines/ffmpeg");
#ifdef Q_OS_WIN
    const QString bundledExe = tempAppDir.filePath("engines/ffmpeg/ffmpeg.bat");
    QFile f(bundledExe);
    QVERIFY(f.open(QFile::WriteOnly));
    f.write("@echo bundled_ffmpeg version 2.0\r\n");
    f.close();
#else
    const QString bundledExe = tempAppDir.filePath("engines/ffmpeg/ffmpeg");
    QFile f(bundledExe);
    QVERIFY(f.open(QFile::WriteOnly));
    f.write("#!/bin/sh\necho 'bundled_ffmpeg version 2.0'\nexit 0\n");
    f.close();
    QFile::setPermissions(bundledExe, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
#endif

    auto res = EngineResolver::resolve("ffmpeg", QString(), "-version", tempAppDir.path());
    QVERIFY(res.isAvailable);
    QCOMPARE(res.source, ToolSource::Bundled);
    QCOMPARE(res.executablePath, bundledExe);
    QCOMPARE(res.version, QString("bundled_ffmpeg version 2.0"));
}

void CoreTests::testEngineResolverSystemFallback() {
    using namespace uc::app;
    QTemporaryDir emptyAppDir;
    QVERIFY(emptyAppDir.isValid());

    // ffmpeg exists on this system in system PATH
    auto res = EngineResolver::resolve("ffmpeg", QString(), "-version", emptyAppDir.path());
    QVERIFY(res.isAvailable);
    QCOMPARE(res.source, ToolSource::SystemPath);
#ifdef Q_OS_WIN
    QVERIFY(res.executablePath.endsWith("ffmpeg.exe", Qt::CaseInsensitive) || res.executablePath.endsWith("ffmpeg", Qt::CaseInsensitive));
#else
    QVERIFY(res.executablePath.endsWith("ffmpeg"));
#endif
}

void CoreTests::testEngineResolverMissingReportsError() {
    using namespace uc::app;
    QTemporaryDir emptyAppDir;
    QVERIFY(emptyAppDir.isValid());

    auto res = EngineResolver::resolve("completely_unknown_engine_xyz_123", QString(), "-version", emptyAppDir.path());
    QVERIFY(!res.isAvailable);
    QCOMPARE(res.source, ToolSource::None);
    QVERIFY(res.failureReason.contains("not found"));
}

void CoreTests::testEngineResolverPathsWithSpaces() {
    using namespace uc::app;
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir(tempDir.path()).mkpath("My Tools Space");
#ifdef Q_OS_WIN
    const QString spacedExe = tempDir.filePath("My Tools Space/tool_exe.bat");
    QFile f(spacedExe);
    QVERIFY(f.open(QFile::WriteOnly));
    f.write("@echo spaced tool v1.0\r\n");
    f.close();
#else
    const QString spacedExe = tempDir.filePath("My Tools Space/tool_exe");
    QFile f(spacedExe);
    QVERIFY(f.open(QFile::WriteOnly));
    f.write("#!/bin/sh\necho 'spaced tool v1.0'\nexit 0\n");
    f.close();
    QFile::setPermissions(spacedExe, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
#endif

    auto res = EngineResolver::resolve("tool_exe", spacedExe, "-v");
    QVERIFY(res.isAvailable);
    QCOMPARE(res.source, ToolSource::UserOverride);
    QCOMPARE(res.executablePath, spacedExe);
}

void CoreTests::testEngineResolverCompanionPairing() {
    using namespace uc::app;
    QTemporaryDir tempAppDir;
    QVERIFY(tempAppDir.isValid());

    // Create a custom suite directory with both ffmpeg and ffprobe
    QDir(tempAppDir.path()).mkpath("custom_suite");
#ifdef Q_OS_WIN
    const QString ffmpegPath = tempAppDir.filePath("custom_suite/ffmpeg.bat");
    const QString ffprobePath = tempAppDir.filePath("custom_suite/ffprobe.bat");

    QFile f1(ffmpegPath);
    QVERIFY(f1.open(QFile::WriteOnly));
    f1.write("@echo custom ffmpeg 5.0\r\n");
    f1.close();

    QFile f2(ffprobePath);
    QVERIFY(f2.open(QFile::WriteOnly));
    f2.write("@echo custom ffprobe 5.0\r\n");
    f2.close();
#else
    const QString ffmpegPath = tempAppDir.filePath("custom_suite/ffmpeg");
    const QString ffprobePath = tempAppDir.filePath("custom_suite/ffprobe");

    QFile f1(ffmpegPath);
    QVERIFY(f1.open(QFile::WriteOnly));
    f1.write("#!/bin/sh\necho 'custom ffmpeg 5.0'\nexit 0\n");
    f1.close();
    QFile::setPermissions(ffmpegPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    QFile f2(ffprobePath);
    QVERIFY(f2.open(QFile::WriteOnly));
    f2.write("#!/bin/sh\necho 'custom ffprobe 5.0'\nexit 0\n");
    f2.close();
    QFile::setPermissions(ffprobePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
#endif

    // Resolve ffmpeg with override
    auto resFfmpeg = EngineResolver::resolve("ffmpeg", ffmpegPath, "-version");
    QVERIFY(resFfmpeg.isAvailable);

    // Resolve ffprobe passing companionDir of ffmpeg
    const QString companionDir = QFileInfo(resFfmpeg.executablePath).absolutePath();
    auto resFfprobe = EngineResolver::resolve("ffprobe", QString(), "-version", tempAppDir.path(), companionDir);
    QVERIFY(resFfprobe.isAvailable);
    QCOMPARE(resFfprobe.executablePath, ffprobePath);
    QCOMPARE(resFfprobe.version, QString("custom ffprobe 5.0"));
}

void CoreTests::testPresetRegistryProfiles() {
    auto& reg = PresetRegistry::instance();
    const auto presets = reg.allPresets();
    QVERIFY(presets.size() >= 5);

    const auto* discord = reg.findById("compress_discord");
    QVERIFY(discord != nullptr);
    QCOMPARE(discord->targetFormatId, QString("mp4"));
    QCOMPARE(discord->sourceCategory, MediaCategory::Video);
    QVERIFY(discord->options.contains("customWidth"));
    QCOMPARE(discord->options["customWidth"].toInt(), 1280);

    const auto* flac = reg.findById("lossless_audio");
    QVERIFY(flac != nullptr);
    QCOMPARE(flac->targetFormatId, QString("flac"));
    QCOMPARE(flac->sourceCategory, MediaCategory::Audio);

    const auto* webp = reg.findById("web_ready_images");
    QVERIFY(webp != nullptr);
    QCOMPARE(webp->targetFormatId, QString("webp"));
    QCOMPARE(webp->sourceCategory, MediaCategory::Image);

    const auto* mdPdf = reg.findById("markdown_to_pdf");
    QVERIFY(mdPdf != nullptr);
    QCOMPARE(mdPdf->targetFormatId, QString("pdf"));
    QCOMPARE(mdPdf->sourceCategory, MediaCategory::Document);

    const auto videoPresets = reg.presetsForCategory(MediaCategory::Video);
    QVERIFY(videoPresets.size() >= 2);
}

void CoreTests::testConversionHistoryManagerPersistenceAndRetry() {
    auto& history = ConversionHistoryManager::instance();
    history.clearHistory();
    QCOMPARE(history.records().size(), 0);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString testIn = tempDir.filePath("sample.mp4");
    QFile f(testIn);
    QVERIFY(f.open(QFile::WriteOnly));
    f.write("test content bytes 12345");
    f.close();

    auto job = std::make_shared<ConversionJob>("test-job-99", QStringList{testIn}, "mp4", "webm", tempDir.path(), EngineType::FFmpeg);
    job->setFinalOutputPath(tempDir.filePath("sample.webm"));
    QVERIFY(job->transitionTo(JobState::Preparing));
    QVERIFY(job->transitionTo(JobState::Running));
    job->setExecutedCommand("ffmpeg -i sample.mp4 sample.webm");
    job->setDiagnosticOutput("frame=100 fps=50");
    QVERIFY(job->transitionTo(JobState::Completed));

    history.recordJob(job.get(), 1450);
    const auto records = history.records();
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().id, QString("test-job-99"));
    QCOMPARE(records.first().inputFileName, QString("sample.mp4"));
    QCOMPARE(records.first().outputFormatId, QString("webm"));
    QCOMPARE(records.first().finalState, JobState::Completed);
    QCOMPARE(records.first().durationMs, 1450);
    QCOMPARE(records.first().executedCommand, QString("ffmpeg -i sample.mp4 sample.webm"));
    QCOMPARE(records.first().diagnosticOutput, QString("frame=100 fps=50"));

    // Test persistence reload
    history.save();
    history.load();
    const auto reloaded = history.records();
    QCOMPARE(reloaded.size(), 1);
    QCOMPARE(reloaded.first().id, QString("test-job-99"));

    history.clearHistory();
    QCOMPARE(history.records().size(), 0);
}

void CoreTests::testMediaEstimatorCalculations() {
    MediaEstimationInput videoIn;
    videoIn.durationSeconds = 60.0; // 1 minute
    videoIn.targetFormatId = "mp4";
    videoIn.targetWidth = 1920;
    videoIn.targetHeight = 1080;
    videoIn.targetVideoBitrateKbps = 5000;
    videoIn.targetAudioBitrateKbps = 192;

    auto resVideo = MediaEstimator::estimateMediaOutput(videoIn);
    QVERIFY(resVideo.hasEstimate);
    QVERIFY(resVideo.estimatedSizeBytes > 35 * 1024 * 1024); // ~39 MB
    QVERIFY(resVideo.estimatedSizeBytes < 45 * 1024 * 1024);

    MediaEstimationInput audioIn;
    audioIn.durationSeconds = 120.0; // 2 minutes
    audioIn.targetFormatId = "mp3";
    audioIn.targetAudioBitrateKbps = 320;

    auto resAudio = MediaEstimator::estimateMediaOutput(audioIn);
    QVERIFY(resAudio.hasEstimate);
    QVERIFY(resAudio.estimatedSizeBytes > 4 * 1024 * 1024); // ~4.8 MB
    QVERIFY(resAudio.estimatedSizeBytes < 6 * 1024 * 1024);

    MediaEstimationInput imgIn;
    imgIn.sourceSizeBytes = 2000000; // 2 MB PNG
    imgIn.sourceWidth = 1920;
    imgIn.sourceHeight = 1080;
    imgIn.targetFormatId = "webp";
    imgIn.targetQuality = 80;

    auto resImg = MediaEstimator::estimateMediaOutput(imgIn);
    QVERIFY(resImg.hasEstimate);
    QVERIFY(resImg.estimatedSizeBytes < 2000000); // WebP should compress smaller than uncompressed PNG
}

void CoreTests::testEngineCapabilityResolverDistinction() {
    auto& resolver = EngineCapabilityResolver::instance();
    resolver.refresh();

    QVERIFY(resolver.totalRecognizedCount() > 100);

    const auto capMp4 = resolver.resolveFormat("mp4");
    QCOMPARE(capMp4.definition.id, QString("mp4"));
    QVERIFY(capMp4.readiness == FormatReadiness::Verified || capMp4.readiness == FormatReadiness::Available);

    const auto capPng = resolver.resolveFormat("png");
    QCOMPARE(capPng.definition.id, QString("png"));
    QVERIFY(capPng.readiness == FormatReadiness::Verified || capPng.readiness == FormatReadiness::Available);

    const auto capPdf = resolver.resolveFormat("pdf");
    QCOMPARE(capPdf.definition.id, QString("pdf"));
    QCOMPARE(capPdf.activeEngineName, QString("Pandoc"));
}

void CoreTests::testSubtitleFormatRegistrationAndCompatibility() {
    auto& registry = FormatRegistry::instance();

    // Verify subtitle formats exist and belong to MediaCategory::Subtitle
    const auto* srt = registry.findById("srt");
    QVERIFY(srt != nullptr);
    QCOMPARE(srt->category, MediaCategory::Subtitle);
    QCOMPARE(srt->defaultExtension, QString("srt"));
    QVERIFY(srt->canInput && srt->canOutput);

    const auto* vtt = registry.findById("vtt");
    QVERIFY(vtt != nullptr);
    QCOMPARE(vtt->category, MediaCategory::Subtitle);

    const auto* ass = registry.findById("ass");
    QVERIFY(ass != nullptr);
    QCOMPARE(ass->category, MediaCategory::Subtitle);

    const auto* ssa = registry.findById("ssa");
    QVERIFY(ssa != nullptr);
    QCOMPARE(ssa->category, MediaCategory::Subtitle);

    const auto* sub = registry.findById("sub");
    QVERIFY(sub != nullptr);
    QCOMPARE(sub->category, MediaCategory::Subtitle);
    QVERIFY(sub->canInput);
    QVERIFY(!sub->canOutput); // 'sub' is input-only

    // Verify inter-subtitle compatibility
    const auto resSrtToVtt = registry.checkCompatibility("srt", "vtt");
    QVERIFY(resSrtToVtt.isSupported);
    QCOMPARE(resSrtToVtt.responsibleEngine, QString("FFmpeg"));

    const auto resAssToSrt = registry.checkCompatibility("ass", "srt");
    QVERIFY(resAssToSrt.isSupported);
    QCOMPARE(resAssToSrt.responsibleEngine, QString("FFmpeg"));

    // MicroDVD 'sub' cannot be output
    const auto resSrtToSub = registry.checkCompatibility("srt", "sub");
    QVERIFY(!resSrtToSub.isSupported);
}

void CoreTests::testSubtitleArgBuilderGeneration() {
    using namespace uc::engines;

    auto res = FfmpegArgBuilder::buildSubtitleArguments("/in/subs.srt", "/out/subs.vtt", "vtt");
    QVERIFY(res.isValid);
    QCOMPARE(res.chosenAudioCodec, QString("webvtt"));
    QVERIFY(res.arguments.contains("-c:s"));
    QVERIFY(res.arguments.contains("webvtt"));
    QVERIFY(res.arguments.contains("/in/subs.srt"));
    QVERIFY(res.arguments.contains("/out/subs.vtt"));

    auto resAss = FfmpegArgBuilder::buildSubtitleArguments("/in/subs.vtt", "/out/subs.ass", "ass");
    QVERIFY(resAss.isValid);
    QCOMPARE(resAss.chosenAudioCodec, QString("ass"));
    QVERIFY(resAss.arguments.contains("ass"));

    // Empty path guard
    auto resEmpty = FfmpegArgBuilder::buildSubtitleArguments("", "/out/subs.vtt", "vtt");
    QVERIFY(!resEmpty.isValid);
}

void CoreTests::testRealSubtitleConversionsEndToEnd() {
    using namespace uc::engines;

    const auto& tc = uc::app::ToolchainManager::instance();
    if (!tc.ffmpeg().isAvailable) {
        QSKIP("FFmpeg executable is not available on host system.");
    }

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString inSrt = tempDir.filePath("sample.srt");
    const QString outVtt = tempDir.filePath("sample.vtt");
    const QString outAss = tempDir.filePath("sample.ass");

    // Write valid SRT test fixture
    QFile srtFile(inSrt);
    QVERIFY(srtFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray srtData =
        "1\n"
        "00:00:01,000 --> 00:00:04,000\n"
        "Hello World from Universal Converter!\n\n"
        "2\n"
        "00:00:05,500 --> 00:00:09,000\n"
        "Subtitle Engine Verification.\n\n";
    srtFile.write(srtData);
    srtFile.close();

    FfmpegEngine engine;

    // 1. SRT -> WebVTT
    auto jobVtt = std::make_shared<ConversionJob>("job-sub-vtt", QStringList{inSrt}, "srt", "vtt", tempDir.path(), EngineType::FFmpeg);
    jobVtt->setFinalOutputPath(outVtt);

    QSignalSpy vttFinishSpy(&engine, &FfmpegEngine::jobFinished);
    QVERIFY(engine.executeJob(jobVtt));
    QVERIFY(vttFinishSpy.wait(10000));
    QCOMPARE(jobVtt->state(), JobState::Completed);
    QVERIFY(QFile::exists(outVtt));

    // Verify VTT header content
    QFile vttFile(outVtt);
    QVERIFY(vttFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString vttContent = QString::fromUtf8(vttFile.readAll());
    vttFile.close();
    QVERIFY(vttContent.contains("WEBVTT"));
    QVERIFY(vttContent.contains("Hello World from Universal Converter!"));

    // 2. SRT -> ASS
    auto jobAss = std::make_shared<ConversionJob>("job-sub-ass", QStringList{inSrt}, "srt", "ass", tempDir.path(), EngineType::FFmpeg);
    jobAss->setFinalOutputPath(outAss);

    QSignalSpy assFinishSpy(&engine, &FfmpegEngine::jobFinished);
    QVERIFY(engine.executeJob(jobAss));
    QVERIFY(assFinishSpy.wait(10000));
    QCOMPARE(jobAss->state(), JobState::Completed);
    QVERIFY(QFile::exists(outAss));

    // Verify ASS header content
    QFile assFile(outAss);
    QVERIFY(assFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString assContent = QString::fromUtf8(assFile.readAll());
    assFile.close();
    QVERIFY(assContent.contains("[Script Info]"));
    QVERIFY(assContent.contains("Subtitle Engine Verification."));

    // 3. Non-UTF8 (ISO-8859-1 / Windows-1252) SRT -> WebVTT
    const QString inLatin1Srt = tempDir.filePath("latin1_sample.srt");
    const QString outLatin1Vtt = tempDir.filePath("latin1_sample.vtt");
    QFile latin1File(inLatin1Srt);
    QVERIFY(latin1File.open(QIODevice::WriteOnly));
    const QByteArray latin1Data =
        "1\n"
        "00:00:01,000 --> 00:00:04,000\n"
        "J\xE1 se passaram anos. N\xE3o sou muito bom.\n\n";
    latin1File.write(latin1Data);
    latin1File.close();

    auto jobLatin1 = std::make_shared<ConversionJob>("job-sub-latin1", QStringList{inLatin1Srt}, "srt", "vtt", tempDir.path(), EngineType::FFmpeg);
    jobLatin1->setFinalOutputPath(outLatin1Vtt);

    QSignalSpy latin1Spy(&engine, &FfmpegEngine::jobFinished);
    QVERIFY(engine.executeJob(jobLatin1));
    QVERIFY(latin1Spy.wait(10000));
    QCOMPARE(jobLatin1->state(), JobState::Completed);
    QVERIFY(QFile::exists(outLatin1Vtt));

    QFile vttLatin1File(outLatin1Vtt);
    QVERIFY(vttLatin1File.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString decodedVtt = QString::fromUtf8(vttLatin1File.readAll());
    vttLatin1File.close();
    QVERIFY(decodedVtt.contains("Já se passaram anos"));

    // 4. Real-world user fixture test if present on system
    const QString desktopFile = "/home/iamthroot/Desktop/visionquest.s01.e02.theseus.(2026).pob.1cd.(14122420)/VisionQuest.S01E02.Theseus.1080p.DSNP.WEB-DL.AAC2.0.H.264-playWEB.srt";
    if (QFile::exists(desktopFile)) {
        const QString outDesktopVtt = tempDir.filePath("desktop_test.vtt");
        auto jobDesktop = std::make_shared<ConversionJob>("job-sub-desktop", QStringList{desktopFile}, "srt", "vtt", tempDir.path(), EngineType::FFmpeg);
        jobDesktop->setFinalOutputPath(outDesktopVtt);

        QSignalSpy desktopSpy(&engine, &FfmpegEngine::jobFinished);
        QVERIFY(engine.executeJob(jobDesktop));
        QVERIFY(desktopSpy.wait(10000));
        QCOMPARE(jobDesktop->state(), JobState::Completed);
        QVERIFY(QFile::exists(outDesktopVtt));
        QVERIFY(QFileInfo(outDesktopVtt).size() > 5000);
    }
}

void CoreTests::testVectorPipelineFormatCompatibility() {
    using namespace uc::engines;
    QVERIFY(VectorPipeline::canConvert("svg", "png"));
    QVERIFY(VectorPipeline::canConvert("svg", "pdf"));
    QVERIFY(VectorPipeline::canConvert("svg", "webp"));
    QVERIFY(VectorPipeline::canConvert("svg", "jpeg"));
    QVERIFY(VectorPipeline::canConvert("svg", "bmp"));
    QVERIFY(!VectorPipeline::canConvert("png", "svg"));
    QVERIFY(!VectorPipeline::canConvert("pdf", "svg"));
}

void CoreTests::testVectorPipelineQtSvgFallbackRasterization() {
    using namespace uc::engines;
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString inSvg = tempDir.filePath("sample_vector.svg");
    const QString outPng = tempDir.filePath("rendered_qt.png");

    QFile svgFile(inSvg);
    QVERIFY(svgFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray svgData =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"100\" viewBox=\"0 0 200 100\">\n"
        "  <rect width=\"200\" height=\"100\" fill=\"blue\" />\n"
        "  <circle cx=\"100\" cy=\"50\" r=\"30\" fill=\"yellow\" />\n"
        "</svg>\n";
    svgFile.write(svgData);
    svgFile.close();

    VectorRasterOptions options;
    options.width = 400;
    options.height = 200;
    QString err;
    const bool success = VectorPipeline::renderSvgToImageWithQt(inSvg, outPng, options, &err);
    QVERIFY2(success, qPrintable(err));
    QVERIFY(QFile::exists(outPng));
    QVERIFY(QFileInfo(outPng).size() > 0);

    QImage resultImg(outPng);
    QVERIFY(!resultImg.isNull());
    QCOMPARE(resultImg.width(), 400);
    QCOMPARE(resultImg.height(), 200);
}

void CoreTests::testVectorPipelineRsvgConvertSvgToPdfAndPng() {
    using namespace uc::engines;
    const auto& tc = uc::app::ToolchainManager::instance();
    if (!tc.rsvgConvert().isAvailable) {
        QSKIP("rsvg-convert is not available on host system.");
    }

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString inSvg = tempDir.filePath("rsvg_test.svg");
    const QString outPdf = tempDir.filePath("rsvg_test.pdf");
    const QString outPng = tempDir.filePath("rsvg_test.png");

    QFile svgFile(inSvg);
    QVERIFY(svgFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray svgData =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"120\">\n"
        "  <polygon points=\"60,20 100,100 20,100\" fill=\"green\" />\n"
        "</svg>\n";
    svgFile.write(svgData);
    svgFile.close();

    VectorRasterOptions options;
    options.width = 120;
    options.height = 120;

    // SVG -> PDF via rsvg-convert
    const auto pdfRes = VectorPipeline::convertSvg(inSvg, outPdf, options);
    QVERIFY(pdfRes.isSuccess);
    QCOMPARE(pdfRes.usedEngine, QString("rsvg-convert"));
    QVERIFY(QFile::exists(outPdf));
    QVERIFY(QFileInfo(outPdf).size() > 0);

    // SVG -> PNG via rsvg-convert
    const auto pngRes = VectorPipeline::convertSvg(inSvg, outPng, options);
    QVERIFY(pngRes.isSuccess);
    QCOMPARE(pngRes.usedEngine, QString("rsvg-convert"));
    QVERIFY(QFile::exists(outPng));
    QVERIFY(QFileInfo(outPng).size() > 0);
}

void CoreTests::testImageOptimizerLosslessAndMetadataStripping() {
    using namespace uc::engines;
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString testImgPath = tempDir.filePath("optim_test.png");

    // Generate an uncompressed dummy test image
    QImage canvas(100, 100, QImage::Format_ARGB32);
    canvas.fill(Qt::red);
    QVERIFY(canvas.save(testImgPath, "PNG"));
    const qint64 origSize = QFileInfo(testImgPath).size();
    QVERIFY(origSize > 0);

    ImageOptimizerOptions options;
    options.stripMetadata = true;
    options.lossless = true;

    const auto res = ImageOptimizer::optimizeImage(testImgPath, options);
    // Even if no optimizer is installed, it returns valid result without corrupting the file
    QVERIFY(QFile::exists(testImgPath));
    QVERIFY(QFileInfo(testImgPath).size() > 0);
    QVERIFY(res.originalSizeBytes > 0);
}

void CoreTests::testRealSvgToRasterConversionEndToEnd() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString inSvg = tempDir.filePath("vector_art.svg");
    const QString outPng = tempDir.filePath("vector_art.png");

    QFile svgFile(inSvg);
    QVERIFY(svgFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray svgData =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"250\" height=\"250\">\n"
        "  <rect width=\"250\" height=\"250\" fill=\"darkmagenta\" />\n"
        "  <text x=\"50\" y=\"130\" fill=\"white\" font-size=\"24\">Universal Converter</text>\n"
        "</svg>\n";
    svgFile.write(svgData);
    svgFile.close();

    uc::engines::ImageMagickEngine engine;
    auto job = std::make_shared<ConversionJob>("job-svg-png", QStringList{inSvg}, "svg", "png", tempDir.path(), EngineType::ImageMagick);
    job->setFinalOutputPath(outPng);

    QSignalSpy finishSpy(&engine, &uc::engines::ImageMagickEngine::jobFinished);
    QVERIFY(engine.executeJob(job));
    QVERIFY(finishSpy.wait(10000));
    QCOMPARE(job->state(), JobState::Completed);
    QVERIFY(QFile::exists(outPng));
    QVERIFY(QFileInfo(outPng).size() > 0);

    // Verify raster validity
    QImage rendered(outPng);
    QVERIFY(!rendered.isNull());
    QCOMPARE(rendered.width(), 250);
    QCOMPARE(rendered.height(), 250);
}

void CoreTests::testWatchFolderConfigSerialization() {
    WatchFolderConfig cfg;
    cfg.id = "watch-1";
    cfg.folderPath = "/home/user/watch";
    cfg.targetFormatId = "mp4";
    cfg.presetId = "web_stream_1080p";
    cfg.postAction = WatchPostAction::MoveToProcessed;
    cfg.destinationFolder = "/home/user/converted";
    cfg.debounceSeconds = 3;
    cfg.isEnabled = true;

    const auto json = cfg.toJson();
    const auto restored = WatchFolderConfig::fromJson(json);

    QCOMPARE(restored.id, cfg.id);
    QCOMPARE(restored.folderPath, cfg.folderPath);
    QCOMPARE(restored.targetFormatId, cfg.targetFormatId);
    QCOMPARE(restored.presetId, cfg.presetId);
    QCOMPARE(restored.postAction, cfg.postAction);
    QCOMPARE(restored.destinationFolder, cfg.destinationFolder);
    QCOMPARE(restored.debounceSeconds, cfg.debounceSeconds);
    QCOMPARE(restored.isEnabled, cfg.isEnabled);
}

void CoreTests::testWatchFolderServiceDetectionAndDebounce() {
    QTemporaryDir watchDir;
    QTemporaryDir destDir;
    QVERIFY(watchDir.isValid());
    QVERIFY(destDir.isValid());

    ConversionOrchestrator orch(2);
    WatchFolderService service(&orch);

    WatchFolderConfig cfg;
    cfg.id = "test-watch-debounce";
    cfg.folderPath = watchDir.path();
    cfg.targetFormatId = "png";
    cfg.destinationFolder = destDir.path();
    cfg.debounceSeconds = 1;
    cfg.postAction = WatchPostAction::KeepOriginal;
    cfg.isEnabled = true;

    QSignalSpy detectedSpy(&service, &WatchFolderService::fileDetected);
    QSignalSpy dispatchedSpy(&service, &WatchFolderService::jobDispatched);

    service.addFolder(cfg);

    // Create a new image file in watched folder
    const QString sampleFile = watchDir.filePath("input_sample.bmp");
    QImage canvas(50, 50, QImage::Format_RGB32);
    canvas.fill(Qt::blue);
    QVERIFY(canvas.save(sampleFile, "BMP"));

    // Manually trigger scan or let poll pick it up
    service.scanAllNow();
    QCOMPARE(detectedSpy.count(), 1);

    // Wait for debouncer (poll timer is 1000ms and stable count >= 1)
    QVERIFY(dispatchedSpy.wait(3500));
    QCOMPARE(dispatchedSpy.count(), 1);

    // Verify job was ingested into orchestrator
    QCOMPARE(orch.totalJobCount(), 1);
    auto job = orch.allJobs().first();
    QCOMPARE(job->inputFormatId(), QString("bmp"));
    QCOMPARE(job->outputFormatId(), QString("png"));
}

void CoreTests::testWatchFolderServiceMoveToProcessed() {
    QTemporaryDir watchDir;
    QTemporaryDir destDir;
    QVERIFY(watchDir.isValid());
    QVERIFY(destDir.isValid());

    ConversionOrchestrator orch(2);
    auto magickEng = std::make_shared<uc::engines::ImageMagickEngine>(nullptr);
    orch.setImageMagickEngine(magickEng);

    WatchFolderService service(&orch);

    WatchFolderConfig cfg;
    cfg.id = "test-watch-move";
    cfg.folderPath = watchDir.path();
    cfg.targetFormatId = "png";
    cfg.destinationFolder = destDir.path();
    cfg.debounceSeconds = 1;
    cfg.postAction = WatchPostAction::MoveToProcessed;
    cfg.isEnabled = true;

    service.addFolder(cfg);

    const QString sampleFile = watchDir.filePath("to_process.bmp");
    QImage canvas(60, 60, QImage::Format_RGB32);
    canvas.fill(Qt::green);
    QVERIFY(canvas.save(sampleFile, "BMP"));

    service.scanAllNow();

    QSignalSpy dispatchedSpy(&service, &WatchFolderService::jobDispatched);
    QVERIFY(dispatchedSpy.wait(3500));

    // Wait for conversion completion
    QTRY_VERIFY_WITH_TIMEOUT(orch.allJobs().first()->isTerminal(), 10000);
    QCOMPARE(orch.allJobs().first()->state(), JobState::Completed);

    // Verify file was moved to processed subfolder
    const QString processedFile = QDir(watchDir.path()).filePath("processed/to_process.bmp");
    QVERIFY(!QFile::exists(sampleFile)); // Original removed
    QVERIFY(QFile::exists(processedFile)); // Moved to processed/
}

void CoreTests::testPresetRegistryJsonExportImport() {
    ConversionPreset customP;
    customP.id = "custom_test_h265";
    customP.name = "Custom H.265 Master";
    customP.description = "Ultra high-efficiency H.265 transcode";
    customP.category = PresetCategory::Custom;
    customP.sourceCategory = MediaCategory::Video;
    customP.targetFormatId = "mp4";
    customP.options = {
        {"customVideoCodec", "libx265"},
        {"crf", 20},
        {"customAudioCodec", "aac"},
        {"customAudioBitrateKbps", 256}
    };
    customP.isBuiltIn = false;

    PresetRegistry::instance().registerCustomPreset(customP);

    // Export to JSON
    const QString exportedJson = PresetRegistry::instance().exportProfilesToJson();
    QVERIFY(!exportedJson.isEmpty());
    QVERIFY(exportedJson.contains("custom_test_h265"));
    QVERIFY(exportedJson.contains("Custom H.265 Master"));

    // Remove custom preset
    QVERIFY(PresetRegistry::instance().removeCustomPreset("custom_test_h265"));
    QVERIFY(PresetRegistry::instance().findById("custom_test_h265") == nullptr);

    // Re-import from JSON
    QString err;
    const bool imported = PresetRegistry::instance().importProfilesFromJson(exportedJson, &err);
    QVERIFY2(imported, qPrintable(err));

    const auto* restored = PresetRegistry::instance().findById("custom_test_h265");
    QVERIFY(restored != nullptr);
    QCOMPARE(restored->name, QString("Custom H.265 Master"));
    QCOMPARE(restored->targetFormatId, QString("mp4"));
    QCOMPARE(restored->options.value("crf").toInt(), 20);

    // Clean up
    PresetRegistry::instance().removeCustomPreset("custom_test_h265");
}

void CoreTests::testPresetRegistryCustomProfilePersistence() {
    const QString testId = "custom_persistent_profile";
    ConversionPreset p;
    p.id = testId;
    p.name = "Persistent Audio Profile";
    p.description = "Opus voice compression";
    p.category = PresetCategory::Custom;
    p.sourceCategory = MediaCategory::Audio;
    p.targetFormatId = "opus";
    p.options = {{"customAudioCodec", "libopus"}, {"customAudioBitrateKbps", 64}};
    p.isBuiltIn = false;

    PresetRegistry::instance().registerCustomPreset(p);

    // Force loadCustomPresets to re-read file
    PresetRegistry::instance().loadCustomPresets();

    const auto* found = PresetRegistry::instance().findById(testId);
    QVERIFY(found != nullptr);
    QCOMPARE(found->name, QString("Persistent Audio Profile"));

    // Cleanup
    PresetRegistry::instance().removeCustomPreset(testId);
    QVERIFY(PresetRegistry::instance().findById(testId) == nullptr);
}

void CoreTests::testAnimatedCrossCategoryFormatCompatibility() {
    // Video -> Animated Images
    QVERIFY(FormatRegistry::instance().isConversionSupported("mp4", "gif"));
    QVERIFY(FormatRegistry::instance().isConversionSupported("mp4", "webp"));
    QVERIFY(FormatRegistry::instance().isConversionSupported("mp4", "avif"));
    QVERIFY(FormatRegistry::instance().isConversionSupported("mkv", "gif"));
    QVERIFY(FormatRegistry::instance().isConversionSupported("webm", "gif"));

    // Vice-versa: Animated Images -> Video
    QVERIFY(FormatRegistry::instance().isConversionSupported("gif", "mp4"));
    QVERIFY(FormatRegistry::instance().isConversionSupported("webp", "mp4"));
    QVERIFY(FormatRegistry::instance().isConversionSupported("avif", "mp4"));
    QVERIFY(FormatRegistry::instance().isConversionSupported("gif", "mkv"));
    QVERIFY(FormatRegistry::instance().isConversionSupported("gif", "webm"));

    // Check responsible engine
    const auto comp = FormatRegistry::instance().checkCompatibility("mp4", "gif");
    QVERIFY(comp.isSupported);
    QCOMPARE(comp.responsibleEngine, QString("FFmpeg"));
}

void CoreTests::testVideoToAnimatedImageArgBuilder() {
    using namespace uc::engines;
    MediaProbeResult probe;
    probe.isValid = true;
    probe.formatName = "mp4";
    probe.durationSeconds = 5.0;

    MediaStreamInfo videoStream;
    videoStream.index = 0;
    videoStream.codecType = "video";
    videoStream.codecName = "h264";
    videoStream.width = 1920;
    videoStream.height = 1080;
    probe.streams.append(videoStream);

    MediaStreamInfo audioStream;
    audioStream.index = 1;
    audioStream.codecType = "audio";
    audioStream.codecName = "aac";
    probe.streams.append(audioStream);

    // 1. Video -> GIF
    FfmpegConversionOptions gifOpts;
    gifOpts.targetFormatId = "gif";
    gifOpts.customWidth = 480;
    gifOpts.customFps = 15;

    const auto gifArgs = FfmpegArgBuilder::buildArguments("/tmp/in.mp4", "/tmp/out.gif", probe, gifOpts, {});
    QVERIFY(gifArgs.isValid);
    QCOMPARE(gifArgs.chosenVideoCodec, QString("gif"));
    QVERIFY(gifArgs.arguments.contains("-an"));
    QVERIFY(gifArgs.arguments.join(" ").contains("palettegen[p];[s1][p]paletteuse"));

    // 2. Video -> WebP
    FfmpegConversionOptions webpOpts;
    webpOpts.targetFormatId = "webp";
    const auto webpArgs = FfmpegArgBuilder::buildArguments("/tmp/in.mp4", "/tmp/out.webp", probe, webpOpts, {});
    QVERIFY(webpArgs.isValid);
    QCOMPARE(webpArgs.chosenVideoCodec, QString("libwebp"));
    QVERIFY(webpArgs.arguments.contains("-loop"));
    QVERIFY(webpArgs.arguments.contains("-an"));

    // 3. Video -> AVIF
    FfmpegConversionOptions avifOpts;
    avifOpts.targetFormatId = "avif";
    const auto avifArgs = FfmpegArgBuilder::buildArguments("/tmp/in.mp4", "/tmp/out.avif", probe, avifOpts, {});
    QVERIFY(avifArgs.isValid);
    QCOMPARE(avifArgs.chosenVideoCodec, QString("libsvtav1"));
    QVERIFY(avifArgs.arguments.contains("-an"));
}

void CoreTests::testRealVideoToAnimatedConversionsEndToEnd() {
    using namespace uc::engines;
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString inMp4 = tempDir.filePath("source_clip.mp4");
    const QString outGif = tempDir.filePath("output_clip.gif");
    const QString outWebp = tempDir.filePath("output_clip.webp");
    const QString outAvif = tempDir.filePath("output_clip.avif");
    const QString outFromGifMp4 = tempDir.filePath("restored_from_gif.mp4");

    // Generate a quick 1.5s synthetic MP4 clip
    const auto genRes = ProcessRunner::executeSync("ffmpeg", {
        "-f", "lavfi", "-i", "testsrc=duration=1.5:size=320x240:rate=10",
        "-c:v", "libx264", "-pix_fmt", "yuv420p", "-y", inMp4
    }, 10000);
    QVERIFY2(genRes.isSuccess(), "Failed to generate synthetic MP4 test fixture");

    FfmpegEngine engine;

    // 1. MP4 -> GIF
    auto jobGif = std::make_shared<ConversionJob>("job-v-gif", QStringList{inMp4}, "mp4", "gif", tempDir.path(), EngineType::FFmpeg);
    jobGif->setFinalOutputPath(outGif);
    QSignalSpy gifSpy(&engine, &FfmpegEngine::jobFinished);
    QVERIFY(engine.executeJob(jobGif));
    QVERIFY(gifSpy.wait(15000));
    QCOMPARE(jobGif->state(), JobState::Completed);
    QVERIFY(QFile::exists(outGif));
    QVERIFY(QFileInfo(outGif).size() > 1000);

    // 2. MP4 -> WebP
    auto jobWebp = std::make_shared<ConversionJob>("job-v-webp", QStringList{inMp4}, "mp4", "webp", tempDir.path(), EngineType::FFmpeg);
    jobWebp->setFinalOutputPath(outWebp);
    QSignalSpy webpSpy(&engine, &FfmpegEngine::jobFinished);
    QVERIFY(engine.executeJob(jobWebp));
    QVERIFY(webpSpy.wait(15000));
    QCOMPARE(jobWebp->state(), JobState::Completed);
    QVERIFY(QFile::exists(outWebp));
    QVERIFY(QFileInfo(outWebp).size() > 500);

    // 3. MP4 -> AVIF
    auto jobAvif = std::make_shared<ConversionJob>("job-v-avif", QStringList{inMp4}, "mp4", "avif", tempDir.path(), EngineType::FFmpeg);
    jobAvif->setFinalOutputPath(outAvif);
    QSignalSpy avifSpy(&engine, &FfmpegEngine::jobFinished);
    QVERIFY(engine.executeJob(jobAvif));
    QVERIFY(avifSpy.wait(15000));
    QCOMPARE(jobAvif->state(), JobState::Completed);
    QVERIFY(QFile::exists(outAvif));
    QVERIFY(QFileInfo(outAvif).size() > 500);

    // 4. Vice-versa: GIF -> MP4
    auto jobFromGif = std::make_shared<ConversionJob>("job-gif-mp4", QStringList{outGif}, "gif", "mp4", tempDir.path(), EngineType::FFmpeg);
    jobFromGif->setFinalOutputPath(outFromGifMp4);
    QSignalSpy fromGifSpy(&engine, &FfmpegEngine::jobFinished);
    QVERIFY(engine.executeJob(jobFromGif));
    QVERIFY(fromGifSpy.wait(15000));
    QCOMPARE(jobFromGif->state(), JobState::Completed);
    QVERIFY(QFile::exists(outFromGifMp4));
    QVERIFY(QFileInfo(outFromGifMp4).size() > 1000);
}

QTEST_GUILESS_MAIN(CoreTests)
#include "tst_core.moc"
