#include "ConversionJob.h"

namespace uc::core {

QString jobStateToString(JobState state) {
    switch (state) {
        case JobState::Queued: return "Queued";
        case JobState::Preparing: return "Preparing";
        case JobState::Running: return "Running";
        case JobState::Completed: return "Completed";
        case JobState::Failed: return "Failed";
        case JobState::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

QString engineTypeToString(EngineType engine) {
    switch (engine) {
        case EngineType::Unknown: return "Unknown";
        case EngineType::FFmpeg: return "FFmpeg";
        case EngineType::ImageMagick: return "ImageMagick";
        case EngineType::Pandoc: return "Pandoc";
    }
    return "Unknown";
}

ConversionJob::ConversionJob(const QString& id,
                             const QStringList& inputPaths,
                             const QString& inputFormatId,
                             const QString& outputFormatId,
                             const QString& destinationPath,
                             EngineType engine,
                             QObject* parent)
    : QObject(parent)
    , m_id(id)
    , m_inputPaths(inputPaths)
    , m_inputFormatId(inputFormatId.toLower())
    , m_outputFormatId(outputFormatId.toLower())
    , m_destinationDirectory(destinationPath)
    , m_engine(engine)
    , m_createdAt(QDateTime::currentDateTime())
{
}

bool ConversionJob::isTerminal() const {
    return m_state == JobState::Completed ||
           m_state == JobState::Failed ||
           m_state == JobState::Cancelled;
}

bool ConversionJob::isValidTransition(JobState from, JobState to) const {
    // Completed / Cancelled are strictly terminal and can never transition.
    if (from == JobState::Completed || from == JobState::Cancelled) {
        return false;
    }

    // Failed may only re-enter the queue via a gated retry (orchestrator
    // checks retryCount < maxRetries + transient error before calling this).
    if (from == JobState::Failed) {
        return to == JobState::Queued;
    }

    switch (from) {
        case JobState::Queued:
            return to == JobState::Preparing || to == JobState::Cancelled || to == JobState::Failed;

        case JobState::Preparing:
            // Preparing -> Queued allows unknown-engine recovery / re-dispatch
            // without stranding the job (previously a dead/stuck path).
            return to == JobState::Running || to == JobState::Cancelled || to == JobState::Failed ||
                   to == JobState::Queued;

        case JobState::Running:
            return to == JobState::Completed || to == JobState::Failed || to == JobState::Cancelled;

        default:
            return false;
    }
}

bool ConversionJob::transitionTo(JobState newState, const QString& message) {
    if (m_state == newState) {
        return true;
    }

    if (!isValidTransition(m_state, newState)) {
        return false;
    }

    const JobState oldState = m_state;
    m_state = newState;

    if (!message.isEmpty()) {
        m_statusMessage = message;
    } else {
        m_statusMessage = jobStateToString(newState);
    }

    if (newState == JobState::Running && !m_startedAt.isValid()) {
        m_startedAt = QDateTime::currentDateTime();
    } else if (isTerminal() && !m_completedAt.isValid()) {
        m_completedAt = QDateTime::currentDateTime();
    }

    emit stateChanged(newState, oldState);
    return true;
}

void ConversionJob::requestCancellation() {
    if (isTerminal()) {
        return;
    }
    m_cancellationRequested = true;
    emit cancellationRequested();
}

void ConversionJob::setProgress(int percentage, const QString& message) {
    if (isTerminal()) return;

    m_progressPercentage = qBound(-1, percentage, 100);
    if (!message.isEmpty()) {
        m_statusMessage = message;
    }
    emit progressChanged(m_progressPercentage, m_statusMessage);
}

void ConversionJob::setFinalOutputPath(const QString& path) {
    m_finalOutputPath = path;
}

void ConversionJob::setError(const QString& errorMessage) {
    m_errorMessage = errorMessage;
    transitionTo(JobState::Failed, errorMessage);
}

} // namespace uc::core
