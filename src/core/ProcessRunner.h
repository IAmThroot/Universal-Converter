#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QElapsedTimer>
#include <QTimer>
#include <memory>

namespace uc::core {

enum class ProcessExitReason {
    Normal,
    FailedToStart,
    Crashed,
    TimedOut,
    Cancelled
};

struct ProcessResult {
    int exitCode{-1};
    ProcessExitReason exitReason{ProcessExitReason::Normal};
    QByteArray standardOutput;
    QByteArray standardError;
    QString errorMessage;
    qint64 durationMs{0};
    bool timedOut{false};
    QString program;
    QStringList arguments;

    bool isSuccess() const {
        return exitReason == ProcessExitReason::Normal && exitCode == 0;
    }
};

} // namespace uc::core

Q_DECLARE_METATYPE(uc::core::ProcessResult)

namespace uc::core {

class ProcessRunner : public QObject {
    Q_OBJECT

public:
    explicit ProcessRunner(QObject* parent = nullptr);
    ~ProcessRunner() override;

    // Execution
    bool start(const QString& program, const QStringList& arguments, int timeoutMs = 0);
    void cancel(int gracefulWaitMs = 1500);

    // Synchronous execution helper with AppImage LD_LIBRARY_PATH sanitization
    static ProcessResult executeSync(const QString& program, const QStringList& arguments, int timeoutMs = 5000);
    static QProcessEnvironment cleanEnvironment();

    bool isRunning() const;
    QString program() const { return m_program; }
    QStringList arguments() const { return m_arguments; }

    ProcessResult result() const { return m_result; }

signals:
    void readyReadStandardOutput(const QByteArray& data);
    void readyReadStandardError(const QByteArray& data);
    void finished(const ProcessResult& result);
    void started();

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onStdoutReady();
    void onStderrReady();
    void onTimeout();
    void onForceKillTimeout();

private:
    void finalizeProcess(ProcessExitReason reason, int exitCode, const QString& errorMsg);

    std::unique_ptr<QProcess> m_process;
    QString m_program;
    QStringList m_arguments;
    ProcessResult m_result;

    bool m_isFinalized{false};
    bool m_isCancellationRequested{false};
    QElapsedTimer m_elapsedTimer;
    QTimer m_timeoutTimer;
    QTimer m_forceKillTimer;
};

} // namespace uc::core
