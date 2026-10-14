#include "ImageMagickEngine.h"
#include "../app/ToolchainManager.h"
#include "../core/OutputPathPlanner.h"
#include "VectorPipeline.h"
#include "ImageOptimizer.h"
#include <QFileInfo>
#include <QDir>
#include <QUuid>

namespace uc::engines {

ImageMagickEngine::ImageMagickEngine(QObject* parent)
    : QObject(parent)
{
}

bool ImageMagickEngine::canConvert(const QString& inputFormatId, const QString& outputFormatId) const {
    if (VectorPipeline::canConvert(inputFormatId, outputFormatId)) {
        return true;
    }

    const auto& caps = ImageMagickCapabilities::instance();
    if (!caps.isDiscovered()) {
        const auto& tc = app::ToolchainManager::instance();
        if (tc.magick().isAvailable) {
            ImageMagickCapabilities::instance().discover(tc.magick().executablePath);
        }
    }

    if (caps.isDiscovered()) {
        return caps.canReadFormat(inputFormatId) && caps.canWriteFormat(outputFormatId);
    }

    // Default basic image formats if undiscovered
    const QStringList basic = {"png", "jpeg", "jpg", "webp", "gif", "bmp", "tiff", "ico", "avif", "heic", "svg"};
    return basic.contains(inputFormatId.toLower()) && basic.contains(outputFormatId.toLower());
}

bool ImageMagickEngine::executeJob(std::shared_ptr<core::ConversionJob> job) {
    if (!job) return false;

    const QString inputPath = job->primaryInputPath();
    const QString inFormat = job->inputFormatId().toLower();
    const QString targetFormat = job->outputFormatId().toLower();
    const QString finalOut = job->finalOutputPath();
    const QString tempOut = core::OutputPathPlanner::generateTemporaryOutputPath(finalOut);

    // 1. Vector Pipeline Fast-Path: SVG -> Raster/PDF
    if (inFormat == "svg") {
        if (job->state() == core::JobState::Queued) {
            job->transitionTo(core::JobState::Preparing);
        }
        job->transitionTo(core::JobState::Running, QString("Rendering vector graphics to %1...").arg(targetFormat.toUpper()));

        VectorRasterOptions vOpts;
        const auto optsMap = job->options();
        if (optsMap.contains("width")) vOpts.width = optsMap["width"].toInt();
        if (optsMap.contains("height")) vOpts.height = optsMap["height"].toInt();
        if (optsMap.contains("dpi")) vOpts.dpi = optsMap["dpi"].toInt();
        if (optsMap.contains("keepAspectRatio")) vOpts.keepAspectRatio = optsMap["keepAspectRatio"].toBool();
        if (optsMap.contains("backgroundColor")) vOpts.backgroundColor = optsMap["backgroundColor"].toString();

        const auto vRes = VectorPipeline::convertSvg(inputPath, tempOut, vOpts);
        if (vRes.isSuccess && QFile::exists(tempOut) && QFileInfo(tempOut).size() > 0) {
            if (QFile::exists(finalOut)) {
                QFile::remove(finalOut);
            }
            if (!QFile::rename(tempOut, finalOut)) {
                job->setError(QString("Failed to move temporary vector output to: %1").arg(finalOut));
                emit jobFinished(job->id(), false, "Filesystem publish failed");
                return false;
            }

            job->setDiagnosticOutput(QString("Vector conversion completed using %1").arg(vRes.usedEngine));
            job->setProgress(100, "Completed successfully");
            job->transitionTo(core::JobState::Completed, "Completed");
            QMetaObject::invokeMethod(this, [this, id = job->id()]() {
                emit jobFinished(id, true, "Completed");
            }, Qt::QueuedConnection);
            return true;
        }

        // If SVG vector pipeline returned error, continue to try ImageMagick
    }

    const auto& tc = app::ToolchainManager::instance();
    const auto magickInfo = tc.magick();

    if (!magickInfo.isAvailable) {
        job->setError("ImageMagick executable (magick) is not available or configured on this computer.");
        return false;
    }

    // Build arguments from options
    ImageConversionOptions options;
    options.targetFormatId = targetFormat;

    const auto optsMap = job->options();
    if (optsMap.contains("quality")) options.quality = optsMap["quality"].toInt();
    if (optsMap.contains("width")) options.width = optsMap["width"].toInt();
    if (optsMap.contains("height")) options.height = optsMap["height"].toInt();
    if (optsMap.contains("stripMetadata")) options.stripMetadata = optsMap["stripMetadata"].toBool();
    if (optsMap.contains("preserveAnimation")) options.preserveAnimation = optsMap["preserveAnimation"].toBool();

    const auto argResult = ImageMagickArgBuilder::buildArguments(inputPath, tempOut, options);
    if (!argResult.isValid) {
        job->setError(argResult.errorMessage);
        return false;
    }

    auto ctx = std::make_shared<RunningContext>();
    ctx->job = job;
    ctx->runner = std::make_shared<core::ProcessRunner>(this);
    ctx->tempOutputPath = tempOut;
    ctx->finalOutputPath = finalOut;

    m_activeContexts[job->id()] = ctx;

    connect(ctx->runner.get(), &core::ProcessRunner::finished, this,
            [this, id = job->id()](const core::ProcessResult& res) {
                onProcessFinished(id, res);
            });

    if (job->state() == core::JobState::Queued) {
        job->transitionTo(core::JobState::Preparing);
    }

    job->transitionTo(core::JobState::Running, QString("Converting image to %1...").arg(targetFormat.toUpper()));
    job->setProgress(-1, "Processing image..."); // Indeterminate progress
    job->setExecutedCommand(QString("%1 %2").arg(magickInfo.executablePath, argResult.arguments.join(' ')));

    const bool started = ctx->runner->start(magickInfo.executablePath, argResult.arguments);
    if (!started) {
        QFile::remove(tempOut);
        m_activeContexts.remove(job->id());
        job->setError("Failed to launch ImageMagick process.");
        return false;
    }

    return true;
}

void ImageMagickEngine::cancelJob(const QString& jobId) {
    auto ctx = m_activeContexts.value(jobId, nullptr);
    if (ctx && ctx->runner) {
        ctx->isCancelled = true;
        ctx->runner->cancel();
    }
}

void ImageMagickEngine::onProcessFinished(const QString& jobId, const core::ProcessResult& result) {
    auto ctx = m_activeContexts.take(jobId);
    if (!ctx || !ctx->job) return;

    ctx->job->setDiagnosticOutput(QString::fromUtf8(result.standardError));

    if (ctx->isCancelled || result.exitReason == core::ProcessExitReason::Cancelled) {
        QFile::remove(ctx->tempOutputPath);
        ctx->job->transitionTo(core::JobState::Cancelled, "Image conversion cancelled by user.");
        emit jobFinished(jobId, false, "Cancelled");
        return;
    }

    if (!result.isSuccess()) {
        QFile::remove(ctx->tempOutputPath);
        const QString err = result.errorMessage.isEmpty() ?
                                "ImageMagick conversion failed." : result.errorMessage;
        ctx->job->setError(err);
        emit jobFinished(jobId, false, err);
        return;
    }

    // Output inspection and validation
    const auto& tc = app::ToolchainManager::instance();
    const auto inspect = ImageInspector::inspect(tc.magick().executablePath, ctx->tempOutputPath);
    if (!inspect.isValid) {
        QFile::remove(ctx->tempOutputPath);
        ctx->job->setError(QString("Image validation failed: %1").arg(inspect.errorMessage));
        emit jobFinished(jobId, false, inspect.errorMessage);
        return;
    }

    // Publish final file
    if (QFile::exists(ctx->finalOutputPath)) {
        QFile::remove(ctx->finalOutputPath);
    }
    if (!QFile::rename(ctx->tempOutputPath, ctx->finalOutputPath)) {
        ctx->job->setError(QString("Failed to move temporary file to final path: %1").arg(ctx->finalOutputPath));
        emit jobFinished(jobId, false, "Filesystem publish failed");
        return;
    }

    // Optional post-processing optimization (oxipng / pngquant / strip)
    const auto optsMap = ctx->job->options();
    if (optsMap.value("optimize", true).toBool()) {
        ImageOptimizerOptions optOptions;
        optOptions.stripMetadata = optsMap.value("stripMetadata", false).toBool();
        optOptions.quality = optsMap.value("quality", 85).toInt();
        const auto optRes = ImageOptimizer::optimizeImage(ctx->finalOutputPath, optOptions);
        if (optRes.isOptimized) {
            ctx->job->setDiagnosticOutput(
                QString("%1 (Optimized with %2: %3% saved)")
                    .arg(ctx->job->diagnosticOutput())
                    .arg(optRes.usedTool)
                    .arg(QString::number(optRes.savingsPercent, 'f', 1)));
        }
    }

    ctx->job->setProgress(100, "Completed successfully");
    ctx->job->transitionTo(core::JobState::Completed, "Completed");
    emit jobFinished(jobId, true, "Completed");
}

} // namespace uc::engines
