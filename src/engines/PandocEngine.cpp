#include "PandocEngine.h"
#include "../app/ToolchainManager.h"
#include "../core/OutputPathPlanner.h"
#include "../core/FormatRegistry.h"
#include <QFileInfo>
#include <QDir>
#include <QUuid>

namespace uc::engines {

PandocEngine::PandocEngine(QObject* parent)
    : QObject(parent)
{
}

bool PandocEngine::canConvert(const QString& inputFormatId, const QString& outputFormatId) const {
    const auto& registry = core::FormatRegistry::instance();
    const QString in = registry.normalizeFormatId(inputFormatId);
    const QString out = registry.normalizeFormatId(outputFormatId);
    const auto* inDef = registry.findById(in);
    const auto* outDef = registry.findById(out);
    if (inDef && outDef) {
        const auto compat = registry.checkCompatibility(in, out);
        return compat.isSupported && compat.responsibleEngine == "Pandoc";
    }

    // Unknown to registry: fall back to live capability probing.
    const auto& caps = PandocCapabilities::instance();
    if (!caps.isDiscovered()) {
        const auto& tc = app::ToolchainManager::instance();
        if (tc.pandoc().isAvailable) {
            PandocCapabilities::instance().discover(tc.pandoc().executablePath);
        }
    }
    if (caps.isDiscovered()) {
        return caps.canReadFormat(inputFormatId) && caps.canWriteFormat(outputFormatId);
    }

    // Default basic document formats if undiscovered
    const QStringList basicIn = {"md", "markdown", "docx", "odt", "html", "htm", "latex", "tex", "rst", "epub"};
    const QStringList basicOut = {"md", "markdown", "docx", "odt", "html", "htm", "latex", "tex", "rst", "epub", "pdf", "rtf", "txt"};

    return basicIn.contains(in) && basicOut.contains(out);
}

bool PandocEngine::executeJob(std::shared_ptr<core::ConversionJob> job) {
    if (!job) return false;

    const auto& tc = app::ToolchainManager::instance();
    const auto pandocInfo = tc.pandoc();

    if (!pandocInfo.isAvailable) {
        job->setError("Pandoc executable is not installed or configured on this computer. Please install Pandoc via pacman/winget or set its path in Settings.");
        return false;
    }

    if (!PandocCapabilities::instance().isDiscovered()) {
        PandocCapabilities::instance().discover(pandocInfo.executablePath);
    }

    const QString inputPath = job->primaryInputPath();
    const QString targetFormat = job->outputFormatId();

    // Prepare temp output path
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

    // Build arguments from options
    DocumentConversionOptions options;
    options.targetFormatId = targetFormat;

    const auto optsMap = job->options();
    if (optsMap.contains("standalone")) options.standalone = optsMap["standalone"].toBool();
    if (optsMap.contains("generateToc")) options.generateToc = optsMap["generateToc"].toBool();
    if (optsMap.contains("tocDepth")) options.tocDepth = optsMap["tocDepth"].toInt();
    if (optsMap.contains("documentTitle")) options.documentTitle = optsMap["documentTitle"].toString();
    if (optsMap.contains("customCssPath")) options.customCssPath = optsMap["customCssPath"].toString();

    const auto& caps = PandocCapabilities::instance();
    const auto argResult = PandocArgBuilder::buildArguments(inputPath, tempOut, options, caps);
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

    job->transitionTo(core::JobState::Running, QString("Converting document to %1...").arg(targetFormat.toUpper()));
    job->setProgress(-1, "Processing document with Pandoc...");
    job->setExecutedCommand(QString("%1 %2").arg(pandocInfo.executablePath, argResult.arguments.join(' ')));

    const bool started = ctx->runner->start(pandocInfo.executablePath, argResult.arguments);
    if (!started) {
        QFile::remove(tempOut);
        m_activeContexts.remove(job->id());
        job->setError("Failed to launch Pandoc child process.");
        return false;
    }

    return true;
}

void PandocEngine::cancelJob(const QString& jobId) {
    auto ctx = m_activeContexts.value(jobId, nullptr);
    if (ctx && ctx->runner) {
        ctx->isCancelled = true;
        ctx->runner->cancel();
    }
}

void PandocEngine::onProcessFinished(const QString& jobId, const core::ProcessResult& result) {
    auto ctx = m_activeContexts.take(jobId);
    if (!ctx || !ctx->job) return;

    ctx->job->setDiagnosticOutput(QString::fromUtf8(result.standardError));

    if (ctx->isCancelled || result.exitReason == core::ProcessExitReason::Cancelled) {
        QFile::remove(ctx->tempOutputPath);
        ctx->job->transitionTo(core::JobState::Cancelled, "Document conversion cancelled by user.");
        emit jobFinished(jobId, false, "Cancelled");
        return;
    }

    if (!result.isSuccess()) {
        QFile::remove(ctx->tempOutputPath);
        const QString err = result.errorMessage.isEmpty() ?
                                "Pandoc conversion failed." : result.errorMessage;
        ctx->job->setError(err);
        emit jobFinished(jobId, false, err);
        return;
    }

    // Output inspection and validation
    const auto val = DocumentValidator::validate(ctx->tempOutputPath, ctx->job->outputFormatId());
    if (!val.isValid) {
        QFile::remove(ctx->tempOutputPath);
        ctx->job->setError(QString("Document validation failed: %1").arg(val.errorMessage));
        emit jobFinished(jobId, false, val.errorMessage);
        return;
    }

    // Publish final file (atomic replace)
    QString publishError;
    if (!core::OutputPathPlanner::publishTempToFinal(ctx->tempOutputPath, ctx->finalOutputPath, &publishError)) {
        ctx->job->setError(publishError.isEmpty()
                               ? QString("Failed to move temporary file to final path: %1").arg(ctx->finalOutputPath)
                               : publishError);
        emit jobFinished(jobId, false, "Filesystem publish failed");
        return;
    }

    ctx->job->setProgress(100, "Completed successfully");
    ctx->job->transitionTo(core::JobState::Completed, "Completed");
    emit jobFinished(jobId, true, "Completed");
}

} // namespace uc::engines
