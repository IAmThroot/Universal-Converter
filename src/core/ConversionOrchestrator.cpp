#include "ConversionOrchestrator.h"
#include "ConversionHistoryManager.h"
#include "OutputPathPlanner.h"
#include "../app/ToolchainManager.h"
#include "../engines/FfmpegEngine.h"
#include "../engines/ImageMagickEngine.h"
#include "../engines/PandocEngine.h"
#include <QTimer>

namespace uc::core {

ConversionOrchestrator::ConversionOrchestrator(int maxConcurrency, QObject* parent)
    : QObject(parent)
    , m_maxConcurrency(qMax(1, maxConcurrency))
{
    connect(&app::ToolchainManager::instance(), &app::ToolchainManager::toolchainStatusChanged,
            this, &ConversionOrchestrator::pumpQueue);
}

ConversionOrchestrator::~ConversionOrchestrator() {
    cancelAllJobs();
}

void ConversionOrchestrator::setFfmpegEngine(std::shared_ptr<engines::FfmpegEngine> engine) {
    m_ffmpegEngine = engine;
    if (m_ffmpegEngine) {
        connect(m_ffmpegEngine.get(), &engines::FfmpegEngine::jobFinished, this,
                [this](const QString& jobId, bool success, const QString& /*msg*/) {
                    auto job = findJob(jobId);
                    JobState state = (job && job->state() == JobState::Cancelled)
                                         ? JobState::Cancelled
                                         : (success ? JobState::Completed : JobState::Failed);
                    onJobFinishedInternal(jobId, state);
                });
    }
}

void ConversionOrchestrator::setImageMagickEngine(std::shared_ptr<engines::ImageMagickEngine> engine) {
    m_imageMagickEngine = engine;
    if (m_imageMagickEngine) {
        connect(m_imageMagickEngine.get(), &engines::ImageMagickEngine::jobFinished, this,
                [this](const QString& jobId, bool success, const QString& /*msg*/) {
                    auto job = findJob(jobId);
                    JobState state = (job && job->state() == JobState::Cancelled)
                                         ? JobState::Cancelled
                                         : (success ? JobState::Completed : JobState::Failed);
                    onJobFinishedInternal(jobId, state);
                });
    }
}

void ConversionOrchestrator::setPandocEngine(std::shared_ptr<engines::PandocEngine> engine) {
    m_pandocEngine = engine;
    if (m_pandocEngine) {
        connect(m_pandocEngine.get(), &engines::PandocEngine::jobFinished, this,
                [this](const QString& jobId, bool success, const QString& /*msg*/) {
                    auto job = findJob(jobId);
                    JobState state = (job && job->state() == JobState::Cancelled)
                                         ? JobState::Cancelled
                                         : (success ? JobState::Completed : JobState::Failed);
                    onJobFinishedInternal(jobId, state);
                });
    }
}

void ConversionOrchestrator::setMaxConcurrency(int concurrency) {
    m_maxConcurrency = qMax(1, concurrency);
    pumpQueue();
}

void ConversionOrchestrator::addJob(std::shared_ptr<ConversionJob> job) {
    if (!job || m_jobMap.contains(job->id())) {
        return;
    }

    m_jobs.append(job);
    m_jobMap.insert(job->id(), job);

    connect(job.get(), &ConversionJob::cancellationRequested, this, [this, id = job->id()]() {
        cancelJob(id);
    });
    connect(job.get(), &ConversionJob::progressChanged, this, [this, id = job->id()](int progress, const QString& msg) {
        emit jobProgressChanged(id, progress, msg);
    });
    connect(job.get(), &ConversionJob::stateChanged, this, [this, id = job->id(), jobPtr = job.get()](JobState state, JobState /*oldState*/) {
        emit jobStateChanged(id, state, jobPtr->statusMessage());
    });

    emit jobAdded(job->id());
    pumpQueue();
}

void ConversionOrchestrator::cancelJob(const QString& jobId) {
    auto job = findJob(jobId);
    if (!job || job->isTerminal()) return;

    if (m_runningJobs.contains(jobId)) {
        if (job->engine() == EngineType::FFmpeg && m_ffmpegEngine) {
            m_ffmpegEngine->cancelJob(jobId);
        } else if (job->engine() == EngineType::ImageMagick && m_imageMagickEngine) {
            m_imageMagickEngine->cancelJob(jobId);
        } else if (job->engine() == EngineType::Pandoc && m_pandocEngine) {
            m_pandocEngine->cancelJob(jobId);
        } else {
            job->transitionTo(JobState::Cancelled, "Cancelled by user.");
            onJobFinishedInternal(jobId, JobState::Cancelled);
        }
    } else if (job->state() == JobState::Queued || job->state() == JobState::Preparing) {
        job->transitionTo(JobState::Cancelled, "Cancelled while queued.");
        onJobFinishedInternal(jobId, JobState::Cancelled);
    }
}

void ConversionOrchestrator::cancelAllJobs() {
    const auto jobIds = m_jobMap.keys();
    for (const auto& id : jobIds) {
        cancelJob(id);
    }
}

void ConversionOrchestrator::removeJob(const QString& jobId) {
    auto job = findJob(jobId);
    if (job) {
        OutputPathPlanner::releasePath(job->finalOutputPath());
    }
    cancelJob(jobId);

    if (m_jobMap.contains(jobId)) {
        m_jobMap.remove(jobId);
        for (int i = 0; i < m_jobs.size(); ++i) {
            if (m_jobs[i]->id() == jobId) {
                m_jobs.removeAt(i);
                break;
            }
        }
        emit jobRemoved(jobId);
    }
}

void ConversionOrchestrator::clearCompletedJobs() {
    QList<QString> toRemove;
    for (const auto& job : m_jobs) {
        if (job->isTerminal()) {
            toRemove.append(job->id());
        }
    }
    for (const auto& id : toRemove) {
        removeJob(id);
    }
}

std::shared_ptr<ConversionJob> ConversionOrchestrator::findJob(const QString& jobId) const {
    return m_jobMap.value(jobId, nullptr);
}

QList<std::shared_ptr<ConversionJob>> ConversionOrchestrator::allJobs() const {
    return m_jobs;
}

QList<std::shared_ptr<ConversionJob>> ConversionOrchestrator::queuedJobs() const {
    QList<std::shared_ptr<ConversionJob>> list;
    for (const auto& j : m_jobs) {
        if (j->state() == JobState::Queued) list.append(j);
    }
    return list;
}

QList<std::shared_ptr<ConversionJob>> ConversionOrchestrator::activeJobs() const {
    return m_runningJobs.values();
}

QList<std::shared_ptr<ConversionJob>> ConversionOrchestrator::completedJobs() const {
    QList<std::shared_ptr<ConversionJob>> list;
    for (const auto& j : m_jobs) {
        if (j->isTerminal()) list.append(j);
    }
    return list;
}

int ConversionOrchestrator::activeJobsForEngine(EngineType engine) const {
    int count = 0;
    for (const auto& job : m_runningJobs) {
        if (job->engine() == engine) {
            ++count;
        }
    }
    return count;
}

bool ConversionOrchestrator::isTransientError(const QString& errorMsg, const QString& diagnostic) const {
    const QString combined = (errorMsg + " " + diagnostic).toLower();
    // Transient indicators: OS file locks, out of memory spikes, busy temporary IO
    if (combined.contains("resource temporarily unavailable") ||
        combined.contains("device or resource busy") ||
        combined.contains("text file busy") ||
        combined.contains("out of memory") ||
        combined.contains("interrupted system call") ||
        combined.contains("sharing violation")) {
        return true;
    }
    return false;
}

void ConversionOrchestrator::pumpQueue() {
    if (app::ToolchainManager::instance().isScanning()) {
        return; // Defer launching jobs until toolchain scan completes
    }

    while (m_runningJobs.size() < m_maxConcurrency) {
        std::shared_ptr<ConversionJob> nextJob = nullptr;
        for (const auto& j : m_jobs) {
            if (j->state() == JobState::Queued) {
                // Check per-engine concurrency ceiling
                const EngineType eng = j->engine();
                if (eng == EngineType::FFmpeg && activeJobsForEngine(EngineType::FFmpeg) >= m_maxFfmpegConcurrency) {
                    continue;
                }
                if (eng == EngineType::ImageMagick && activeJobsForEngine(EngineType::ImageMagick) >= m_maxImageMagickConcurrency) {
                    continue;
                }
                if (eng == EngineType::Pandoc && activeJobsForEngine(EngineType::Pandoc) >= m_maxPandocConcurrency) {
                    continue;
                }

                nextJob = j;
                break;
            }
        }

        if (!nextJob) {
            break;
        }

        if (!nextJob->transitionTo(JobState::Preparing)) {
            continue;
        }

        m_runningJobs.insert(nextJob->id(), nextJob);
        emit jobStarted(nextJob->id());

        // Dispatch based on engine type
        if (nextJob->engine() == EngineType::FFmpeg) {
            if (m_ffmpegEngine) {
                const bool launched = m_ffmpegEngine->executeJob(nextJob);
                if (!launched) {
                    onJobFinishedInternal(nextJob->id(), JobState::Failed);
                }
            } else {
                nextJob->setError("FFmpeg engine is not initialized.");
                onJobFinishedInternal(nextJob->id(), JobState::Failed);
            }
        } else if (nextJob->engine() == EngineType::ImageMagick) {
            if (m_imageMagickEngine) {
                const bool launched = m_imageMagickEngine->executeJob(nextJob);
                if (!launched) {
                    onJobFinishedInternal(nextJob->id(), JobState::Failed);
                }
            } else {
                nextJob->setError("ImageMagick engine is not initialized.");
                onJobFinishedInternal(nextJob->id(), JobState::Failed);
            }
        } else if (nextJob->engine() == EngineType::Pandoc) {
            if (m_pandocEngine) {
                const bool launched = m_pandocEngine->executeJob(nextJob);
                if (!launched) {
                    onJobFinishedInternal(nextJob->id(), JobState::Failed);
                }
            } else {
                nextJob->setError("Pandoc engine is not initialized.");
                onJobFinishedInternal(nextJob->id(), JobState::Failed);
            }
        } else {
            nextJob->transitionTo(JobState::Queued, "Unknown or unsupported engine type");
            m_runningJobs.remove(nextJob->id());
            break;
        }
    }

    if (m_runningJobs.isEmpty() && queuedJobs().isEmpty()) {
        emit queueDrained();
    }
}

void ConversionOrchestrator::onJobFinishedInternal(const QString& jobId, JobState finalState) {
    auto job = findJob(jobId);
    if (!job) {
        m_runningJobs.remove(jobId);
        return;
    }

    // Preserve cancellation or terminal state accurately
    if (job->state() == JobState::Cancelled) {
        finalState = JobState::Cancelled;
    } else if (job->isTerminal()) {
        finalState = job->state();
    }

    // Guard against duplicate execution: only handle completion once per run
    if (job->isFinishedHandled()) {
        m_runningJobs.remove(jobId);
        return;
    }

    // Targeted transient retry (only if failed, retryCount == 0, and transient)
    if (finalState == JobState::Failed && job->retryCount() < 1 &&
        isTransientError(job->errorMessage(), job->diagnosticOutput())) {
        job->incrementRetryCount();
        m_runningJobs.remove(jobId);
        job->transitionTo(JobState::Queued, "Retrying after transient failure...");
        QTimer::singleShot(500, this, &ConversionOrchestrator::pumpQueue);
        return;
    }

    job->setFinishedHandled(true);

    // Transition job to the final state if not already there
    if (job->state() != finalState) {
        const QString msg = (finalState == JobState::Failed && !job->errorMessage().isEmpty())
                                ? job->errorMessage()
                                : (finalState == JobState::Completed ? "Completed" : "Cancelled");
        job->transitionTo(finalState, msg);
    }

    qint64 durationMs = 0;
    if (job->startedAt().isValid() && job->completedAt().isValid()) {
        durationMs = job->startedAt().msecsTo(job->completedAt());
    }
    ConversionHistoryManager::instance().recordJob(job.get(), durationMs);

    m_runningJobs.remove(jobId);
    OutputPathPlanner::releasePath(job->finalOutputPath());

    emit jobFinished(jobId, finalState);
    QTimer::singleShot(0, this, &ConversionOrchestrator::pumpQueue);
}

} // namespace uc::core
