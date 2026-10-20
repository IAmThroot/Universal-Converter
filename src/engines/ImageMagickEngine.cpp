#include "ImageMagickEngine.h"
#include "../app/ToolchainManager.h"
#include "../core/OutputPathPlanner.h"
#include "../core/FormatRegistry.h"
#include "VectorPipeline.h"
#include "ImageOptimizer.h"
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QSet>
#include <QThreadPool>

namespace uc::engines {

ImageMagickEngine::ImageMagickEngine(QObject* parent)
    : QObject(parent)
{
}

bool ImageMagickEngine::canConvert(const QString& inputFormatId, const QString& outputFormatId) const {
    if (VectorPipeline::canConvert(inputFormatId, outputFormatId)) {
        return true;
    }

    const auto& registry = core::FormatRegistry::instance();
    const QString in = registry.normalizeFormatId(inputFormatId);
    const QString out = registry.normalizeFormatId(outputFormatId);
    const auto* inDef = registry.findById(in);
    const auto* outDef = registry.findById(out);
    if (inDef && outDef) {
        const auto compat = registry.checkCompatibility(in, out);
        // Registry is authoritative when both formats are known — prevents
        // engine lists drifting from the central compatibility policy.
        return compat.isSupported && compat.responsibleEngine == "ImageMagick";
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

bool ImageMagickEngine::startMagickProcess(std::shared_ptr<RunningContext> ctx) {
    auto job = ctx->job;
    const auto& tc = app::ToolchainManager::instance();
    const auto magickInfo = tc.magick();

    if (!magickInfo.isAvailable) {
        job->setError("ImageMagick executable (magick) is not available or configured on this computer.");
        return false;
    }

    const QString inputPath = job->primaryInputPath();
    const QString targetFormat = job->outputFormatId().toLower();

    ImageConversionOptions options;
    options.targetFormatId = targetFormat;

    const auto optsMap = job->options();
    if (optsMap.contains("quality")) options.quality = optsMap["quality"].toInt();
    if (optsMap.contains("width")) options.width = optsMap["width"].toInt();
    if (optsMap.contains("height")) options.height = optsMap["height"].toInt();
    if (optsMap.contains("stripMetadata")) options.stripMetadata = optsMap["stripMetadata"].toBool();
    if (optsMap.contains("preserveAnimation")) options.preserveAnimation = optsMap["preserveAnimation"].toBool();
    if (optsMap.contains("autoOrient")) options.autoOrient = optsMap["autoOrient"].toBool();
    if (optsMap.contains("preserveAspectRatio")) options.preserveAspectRatio = optsMap["preserveAspectRatio"].toBool();

    const auto argResult = ImageMagickArgBuilder::buildArguments(inputPath, ctx->tempOutputPath, options);
    if (!argResult.isValid) {
        job->setError(argResult.errorMessage);
        return false;
    }

    job->transitionTo(core::JobState::Running, QString("Converting image to %1...").arg(targetFormat.toUpper()));
    job->setProgress(-1, "Processing image..."); // Indeterminate progress
    job->setExecutedCommand(QString("%1 %2").arg(magickInfo.executablePath, argResult.arguments.join(' ')));

    const bool started = ctx->runner->start(magickInfo.executablePath, argResult.arguments);
    if (!started) {
        QFile::remove(ctx->tempOutputPath);
        job->setError("Failed to launch ImageMagick process.");
        return false;
    }

    return true;
}

bool ImageMagickEngine::executeJob(std::shared_ptr<core::ConversionJob> job) {
    if (!job) return false;

    const QString inputPath = job->primaryInputPath();
    const QString inFormat = job->inputFormatId().toLower();
    const QString targetFormat = job->outputFormatId().toLower();
    const QString finalOut = job->finalOutputPath();
    if (finalOut.trimmed().isEmpty()) {
        job->setError("Output path is empty; job was not planned correctly.");
        return false;
    }
    const QString tempOut = core::OutputPathPlanner::generateTemporaryOutputPath(finalOut);
    if (tempOut.isEmpty()) {
        job->setError("Failed to generate temporary output path.");
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

    // 1. Vector Pipeline Fast-Path: SVG -> Raster/PDF (Offloaded to worker thread)
    if (inFormat == "svg") {
        job->transitionTo(core::JobState::Running, QString("Rendering vector graphics to %1...").arg(targetFormat.toUpper()));

        VectorRasterOptions vOpts;
        const auto optsMap = job->options();
        if (optsMap.contains("width")) vOpts.width = optsMap["width"].toInt();
        if (optsMap.contains("height")) vOpts.height = optsMap["height"].toInt();
        if (optsMap.contains("dpi")) vOpts.dpi = optsMap["dpi"].toInt();
        if (optsMap.contains("keepAspectRatio")) vOpts.keepAspectRatio = optsMap["keepAspectRatio"].toBool();
        if (optsMap.contains("backgroundColor")) vOpts.backgroundColor = optsMap["backgroundColor"].toString();

        QThreadPool::globalInstance()->start([this, ctx, job, inputPath, tempOut, finalOut, vOpts]() {
            if (ctx->isCancelled) {
                QFile::remove(tempOut);
                return;
            }

            const auto vRes = VectorPipeline::convertSvg(inputPath, tempOut, vOpts);

            QMetaObject::invokeMethod(this, [this, ctx, job, vRes, tempOut, finalOut]() {
                if (ctx->isCancelled) {
                    QFile::remove(tempOut);
                    m_activeContexts.remove(job->id());
                    return;
                }

                if (vRes.isSuccess && QFile::exists(tempOut) && QFileInfo(tempOut).size() > 0) {
                    QString publishError;
                    if (!core::OutputPathPlanner::publishTempToFinal(tempOut, finalOut, &publishError)) {
                        job->setError(publishError.isEmpty()
                                          ? QString("Failed to move temporary vector output to: %1").arg(finalOut)
                                          : publishError);
                        job->transitionTo(core::JobState::Failed, job->errorMessage());
                        m_activeContexts.remove(job->id());
                        emit jobFinished(job->id(), false, "Filesystem publish failed");
                        return;
                    }

                    job->setDiagnosticOutput(QString("Vector conversion completed using %1").arg(vRes.usedEngine));
                    job->setProgress(100, "Completed successfully");
                    job->transitionTo(core::JobState::Completed, "Completed");
                    m_activeContexts.remove(job->id());
                    emit jobFinished(job->id(), true, "Completed");
                    return;
                }

                // If SVG vector pipeline returned error, fall back to ImageMagick
                if (!startMagickProcess(ctx)) {
                    m_activeContexts.remove(job->id());
                    emit jobFinished(job->id(), false, job->errorMessage());
                }
            }, Qt::QueuedConnection);
        });

        return true;
    }

    // Standard raster conversion
    if (!startMagickProcess(ctx)) {
        m_activeContexts.remove(job->id());
        return false;
    }

    return true;
}

void ImageMagickEngine::cancelJob(const QString& jobId) {
    auto ctx = m_activeContexts.value(jobId, nullptr);
    if (ctx) {
        ctx->isCancelled = true;
        if (ctx->runner) {
            ctx->runner->cancel();
        }
    }
}

void ImageMagickEngine::onProcessFinished(const QString& jobId, const core::ProcessResult& result) {
    auto ctx = m_activeContexts.value(jobId, nullptr);
    if (!ctx || !ctx->job) return;

    ctx->job->setDiagnosticOutput(QString::fromUtf8(result.standardError));

    if (ctx->isCancelled || result.exitReason == core::ProcessExitReason::Cancelled) {
        m_activeContexts.remove(jobId);
        QFile::remove(ctx->tempOutputPath);
        ctx->job->transitionTo(core::JobState::Cancelled, "Image conversion cancelled by user.");
        emit jobFinished(jobId, false, "Cancelled");
        return;
    }

    if (!result.isSuccess()) {
        m_activeContexts.remove(jobId);
        QFile::remove(ctx->tempOutputPath);
        const QString err = result.errorMessage.isEmpty() ?
                                "ImageMagick conversion failed." : result.errorMessage;
        ctx->job->setError(err);
        emit jobFinished(jobId, false, err);
        return;
    }

    // Offload inspection, optimization, and publishing to worker thread
    const auto& tc = app::ToolchainManager::instance();
    const QString magickExe = tc.magick().executablePath;
    const auto optsMap = ctx->job->options();

    QThreadPool::globalInstance()->start([this, ctx, jobId, magickExe, optsMap]() {
        if (ctx->isCancelled) {
            QFile::remove(ctx->tempOutputPath);
            return;
        }

        const auto inspect = ImageInspector::inspect(magickExe, ctx->tempOutputPath);
        if (!inspect.isValid) {
            QFile::remove(ctx->tempOutputPath);
            QMetaObject::invokeMethod(this, [this, ctx, jobId, err = inspect.errorMessage]() {
                m_activeContexts.remove(jobId);
                ctx->job->setError(QString("Image validation failed: %1").arg(err));
                emit jobFinished(jobId, false, err);
            }, Qt::QueuedConnection);
            return;
        }

        if (ctx->isCancelled) {
            QFile::remove(ctx->tempOutputPath);
            return;
        }

        QString optDiagnostic;
        if (optsMap.value("optimize", true).toBool()) {
            ImageOptimizerOptions optOptions;
            optOptions.stripMetadata = optsMap.value("stripMetadata", false).toBool();
            optOptions.quality = optsMap.value("quality", 85).toInt();
            const auto optRes = ImageOptimizer::optimizeImage(ctx->tempOutputPath, optOptions);
            if (optRes.isOptimized) {
                optDiagnostic = QString(" (Optimized with %1: %2% saved)")
                                    .arg(optRes.usedTool)
                                    .arg(QString::number(optRes.savingsPercent, 'f', 1));
            }
        }

        if (ctx->isCancelled) {
            QFile::remove(ctx->tempOutputPath);
            return;
        }

        QString publishError;
        const bool published = core::OutputPathPlanner::publishTempToFinal(
            ctx->tempOutputPath, ctx->finalOutputPath, &publishError);

        QMetaObject::invokeMethod(this, [this, ctx, jobId, published, publishError, optDiagnostic]() {
            m_activeContexts.remove(jobId);
            if (ctx->isCancelled) {
                return;
            }

            if (!optDiagnostic.isEmpty()) {
                ctx->job->setDiagnosticOutput(ctx->job->diagnosticOutput() + optDiagnostic);
            }

            if (!published) {
                ctx->job->setError(publishError.isEmpty()
                                       ? QString("Failed to move temporary file to final path: %1").arg(ctx->finalOutputPath)
                                       : publishError);
                emit jobFinished(jobId, false, "Filesystem publish failed");
                return;
            }

            ctx->job->setProgress(100, "Completed successfully");
            ctx->job->transitionTo(core::JobState::Completed, "Completed");
            emit jobFinished(jobId, true, "Completed");
        }, Qt::QueuedConnection);
    });
}

} // namespace uc::engines
