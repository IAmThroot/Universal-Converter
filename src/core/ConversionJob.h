#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>
#include <QObject>
#include <memory>
#include "FormatRegistry.h"

namespace uc::core {

enum class JobState {
    Queued,
    Preparing,
    Running,
    Completed,
    Failed,
    Cancelled
};

enum class EngineType {
    Unknown,
    FFmpeg,
    ImageMagick,
    Pandoc
};

class ConversionJob : public QObject {
    Q_OBJECT

public:
    explicit ConversionJob(const QString& id,
                           const QStringList& inputPaths,
                           const QString& inputFormatId,
                           const QString& outputFormatId,
                           const QString& destinationPath,
                           EngineType engine = EngineType::Unknown,
                           QObject* parent = nullptr);
    ~ConversionJob() override = default;

    // Getters
    QString id() const { return m_id; }
    QStringList inputPaths() const { return m_inputPaths; }
    QString primaryInputPath() const { return m_inputPaths.isEmpty() ? QString() : m_inputPaths.first(); }
    QString inputFormatId() const { return m_inputFormatId; }
    QString outputFormatId() const { return m_outputFormatId; }
    QString destinationDirectory() const { return m_destinationDirectory; }
    QString finalOutputPath() const { return m_finalOutputPath; }
    EngineType engine() const { return m_engine; }

    JobState state() const { return m_state; }
    bool isTerminal() const;
    bool isRunning() const { return m_state == JobState::Running; }

    int progressPercentage() const { return m_progressPercentage; }
    bool hasKnownProgress() const { return m_progressPercentage >= 0; }
    QString statusMessage() const { return m_statusMessage; }
    QString errorMessage() const { return m_errorMessage; }

    QDateTime createdAt() const { return m_createdAt; }
    QDateTime startedAt() const { return m_startedAt; }
    QDateTime completedAt() const { return m_completedAt; }

    bool isCancellationRequested() const { return m_cancellationRequested; }

    QVariantMap options() const { return m_options; }
    void setOptions(const QVariantMap& options) { m_options = options; }

    // State Transitions
    bool transitionTo(JobState newState, const QString& message = QString());

    void requestCancellation();
    void setProgress(int percentage, const QString& message = QString());
    void setFinalOutputPath(const QString& path);
    QString executedCommand() const { return m_executedCommand; }
    void setExecutedCommand(const QString& cmd) { m_executedCommand = cmd; }

    QString diagnosticOutput() const { return m_diagnosticOutput; }
    void setDiagnosticOutput(const QString& diag) { m_diagnosticOutput = diag; }

    int retryCount() const { return m_retryCount; }
    void incrementRetryCount() { ++m_retryCount; }

    void setError(const QString& errorMessage);

signals:
    void stateChanged(JobState newState, JobState oldState);
    void progressChanged(int percentage, const QString& message);
    void cancellationRequested();

private:
    bool isValidTransition(JobState from, JobState to) const;

    QString m_id;
    QStringList m_inputPaths;
    QString m_inputFormatId;
    QString m_outputFormatId;
    QString m_destinationDirectory;
    QString m_finalOutputPath;
    EngineType m_engine{EngineType::Unknown};

    JobState m_state{JobState::Queued};
    int m_progressPercentage{-1}; // -1 means indeterminate/unknown
    QString m_statusMessage{"Queued"};
    QString m_errorMessage;

    QDateTime m_createdAt;
    QDateTime m_startedAt;
    QDateTime m_completedAt;

    bool m_cancellationRequested{false};
    QVariantMap m_options;

    QString m_executedCommand;
    QString m_diagnosticOutput;
    int m_retryCount{0};
};

QString jobStateToString(JobState state);
QString engineTypeToString(EngineType engine);

} // namespace uc::core
