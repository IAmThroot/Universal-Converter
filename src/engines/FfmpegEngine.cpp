#include "FfmpegEngine.h"
#include "../app/ToolchainManager.h"
#include "../app/AppSettings.h"
#include "../core/SystemInfo.h"
#include "../core/OutputPathPlanner.h"
#include <QFileInfo>
#include <QDir>
#include <QUuid>

namespace uc::engines {

FfmpegEngine::FfmpegEngine(QObject* parent)
    : QObject(parent)
{
}

bool FfmpegEngine::canConvert(const QString& inputFormatId, const QString& outputFormatId) const {
    const QString in = inputFormatId.toLower();
    const QString out = outputFormatId.toLower();

    const QStringList videoFormats = {"mp4", "mkv", "webm", "mov", "avi", "wmv", "flv", "ts", "mts", "m2ts", "vob", "ogv", "3gp", "3g2", "asf", "mxf", "gxf", "nut", "amv", "rm", "rmvb"};
    const QStringList audioFormats = {"mp3", "wav", "flac", "aac", "ogg", "oga", "opus", "m4a", "wma", "ac3", "aiff", "mp2", "au", "m4b", "voc", "qoa", "caf", "mogg", "mp1", "mpc", "dsd"};
    const QStringList subtitleFormats = {"srt", "vtt", "ass", "ssa", "sub"};

    const bool inIsVideo = videoFormats.contains(in);
    const bool inIsAudio = audioFormats.contains(in);
    const bool inIsSub = subtitleFormats.contains(in);

    const bool outIsVideo = videoFormats.contains(out);
    const bool outIsAudio = audioFormats.contains(out);
    const bool outIsSub = subtitleFormats.contains(out);

    // Subtitle transcoding (e.g. srt -> vtt, ass -> srt)
    if (inIsSub && outIsSub) {
        return out != "sub"; // 'sub' (MicroDVD) is read-only
    }

    if (!inIsVideo && !inIsAudio) return false;
    if (!outIsVideo && !outIsAudio) return false;

    // rm / rmvb cannot be encoded
    if (out == "rm" || out == "rmvb") return false;

    return true;
}

bool FfmpegEngine::executeJob(std::shared_ptr<core::ConversionJob> job) {
    if (!job) return false;

    const auto& tc = app::ToolchainManager::instance();
    const auto ffmpegInfo = tc.ffmpeg();
    const auto ffprobeInfo = tc.ffprobe();

    if (!ffmpegInfo.isAvailable) {
        job->setError("FFmpeg executable is not found or not configured on this computer.");
        return false;
    }

    const QString inputPath = job->primaryInputPath();
    const QString targetFormat = job->outputFormatId();

    const bool isSubJob = FfmpegArgBuilder::isSubtitleContainer(job->inputFormatId()) &&
                          FfmpegArgBuilder::isSubtitleContainer(targetFormat);

    // 1. Probe input media with FFprobe (only for video/audio streams)
    MediaProbeResult probe;
    if (!isSubJob && ffprobeInfo.isAvailable) {
        probe = MediaProbe::probeFile(ffprobeInfo.executablePath, inputPath);
        if (!probe.isValid) {
            job->setError(QString("Failed to probe input file: %1").arg(probe.errorMessage));
            return false;
        }
    }

    // 2. Prepare temporary output path in destination directory
    const QString finalOut = job->finalOutputPath();
    const QString tempOut = core::OutputPathPlanner::generateTemporaryOutputPath(finalOut);

    ArgumentBuildResult argResult;
    if (isSubJob) {
        argResult = FfmpegArgBuilder::buildSubtitleArguments(inputPath, tempOut, targetFormat);
    } else {
        // 3. Hardware acceleration & options
        const auto sysCaps = core::SystemInfo::probe();
        const auto& settings = app::AppSettings::instance();

        FfmpegConversionOptions options;
        options.targetFormatId = targetFormat;
        options.hwMode = settings.hardwareAccelerationEnabled() ?
                             HardwareEncoderMode::Auto : HardwareEncoderMode::Disabled;

        argResult = FfmpegArgBuilder::buildArguments(
            inputPath, tempOut, probe, options, sysCaps.availableHwEncoders);
    }

    if (!argResult.isValid) {
        job->setError(argResult.errorMessage);
        return false;
    }

    // 4. Create running context
    auto ctx = std::make_shared<RunningContext>();
    ctx->job = job;
    ctx->runner = std::make_shared<core::ProcessRunner>(this);
    ctx->parser = std::make_unique<FfmpegProgressParser>(probe.durationSeconds);
    ctx->tempOutputPath = tempOut;
    ctx->finalOutputPath = finalOut;

    m_activeContexts[job->id()] = ctx;

    // Connect progress and completion
    connect(ctx->runner.get(), &core::ProcessRunner::readyReadStandardOutput, this,
            [this, id = job->id()](const QByteArray& chunk) {
                onProcessOutput(id, chunk);
            });

    connect(ctx->runner.get(), &core::ProcessRunner::finished, this,
            [this, id = job->id()](const core::ProcessResult& res) {
                onProcessFinished(id, res);
            });

    // 5. Start conversion
    if (job->state() == core::JobState::Queued) {
        job->transitionTo(core::JobState::Preparing);
    }

    job->transitionTo(core::JobState::Running,
                      QString("Converting with %1 (%2)...")
                          .arg(argResult.chosenVideoCodec.isEmpty() ? argResult.chosenAudioCodec : argResult.chosenVideoCodec,
                               argResult.isHardwareAccelerated ? "GPU NVENC" : "CPU"));

    job->setExecutedCommand(QString("%1 %2").arg(ffmpegInfo.executablePath, argResult.arguments.join(' ')));

    const bool started = ctx->runner->start(ffmpegInfo.executablePath, argResult.arguments);
    if (!started) {
        QFile::remove(tempOut);
        m_activeContexts.remove(job->id());
        job->setError("Failed to launch FFmpeg child process.");
        return false;
    }

    return true;
}

void FfmpegEngine::cancelJob(const QString& jobId) {
    auto ctx = m_activeContexts.value(jobId, nullptr);
    if (ctx && ctx->runner) {
        ctx->isCancelled = true;
        ctx->runner->cancel();
    }
}

void FfmpegEngine::onProcessOutput(const QString& jobId, const QByteArray& data) {
    auto ctx = m_activeContexts.value(jobId, nullptr);
    if (!ctx || !ctx->job || ctx->isCancelled) return;

    const auto snapshots = ctx->parser->feedChunk(data);
    for (const auto& snap : snapshots) {
        const int pct = ctx->parser->calculatePercentage(snap);
        QString statusMsg;
        if (snap.speed > 0.0) {
            statusMsg = QString("Converting... %1% (Speed: %2x)")
                            .arg(pct >= 0 ? QString::number(pct) : "?")
                            .arg(QString::number(snap.speed, 'f', 1));
        } else {
            statusMsg = QString("Converting... %1%").arg(pct >= 0 ? QString::number(pct) : "?");
        }
        ctx->job->setProgress(pct, statusMsg);
    }
}

void FfmpegEngine::onProcessFinished(const QString& jobId, const core::ProcessResult& result) {
    auto ctx = m_activeContexts.take(jobId);
    if (!ctx || !ctx->job) return;

    ctx->job->setDiagnosticOutput(QString::fromUtf8(result.standardError));

    if (ctx->isCancelled || result.exitReason == core::ProcessExitReason::Cancelled) {
        QFile::remove(ctx->tempOutputPath);
        ctx->job->transitionTo(core::JobState::Cancelled, "Conversion cancelled by user.");
        emit jobFinished(jobId, false, "Cancelled");
        return;
    }

    if (!result.isSuccess()) {
        QFile::remove(ctx->tempOutputPath);
        const QString err = result.errorMessage.isEmpty() ?
                                "FFmpeg conversion failed." : result.errorMessage;
        ctx->job->setError(err);
        emit jobFinished(jobId, false, err);
        return;
    }

    const bool isSub = FfmpegArgBuilder::isSubtitleContainer(ctx->job->outputFormatId());
    const bool isAnimImg = FfmpegArgBuilder::isAnimatedImageContainer(ctx->job->outputFormatId());

    if (!isSub && !isAnimImg) {
        // Output validation via ffprobe for media streams
        const auto& tc = app::ToolchainManager::instance();
        const QString ffprobePath = tc.ffprobe().executablePath;
        const bool isAudioOnly = FfmpegArgBuilder::isAudioContainer(ctx->job->outputFormatId());

        const auto val = OutputValidator::validate(ffprobePath, ctx->tempOutputPath, ctx->job->outputFormatId(), isAudioOnly);
        if (!val.isValid) {
            QFile::remove(ctx->tempOutputPath);
            ctx->job->setError(QString("Output validation failed: %1").arg(val.errorMessage));
            emit jobFinished(jobId, false, val.errorMessage);
            return;
        }
    } else {
        // Subtitle or Animated image validation: ensure file was produced and has non-zero size
        QFileInfo outInfo(ctx->tempOutputPath);
        if (!outInfo.exists() || outInfo.size() <= 16) {
            QFile::remove(ctx->tempOutputPath);
            ctx->job->setError("Conversion failed to produce a valid non-empty file.");
            emit jobFinished(jobId, false, "Output file empty or missing");
            return;
        }
    }

    // Publish final file (atomic rename or replace)
    if (QFile::exists(ctx->finalOutputPath)) {
        QFile::remove(ctx->finalOutputPath);
    }
    if (!QFile::rename(ctx->tempOutputPath, ctx->finalOutputPath)) {
        ctx->job->setError(QString("Failed to move temporary file to final path: %1").arg(ctx->finalOutputPath));
        emit jobFinished(jobId, false, "Filesystem publish failed");
        return;
    }

    ctx->job->setProgress(100, "Completed successfully");
    ctx->job->transitionTo(core::JobState::Completed, "Completed");
    emit jobFinished(jobId, true, "Completed");
}

} // namespace uc::engines
