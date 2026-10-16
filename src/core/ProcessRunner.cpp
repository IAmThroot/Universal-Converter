#include "ProcessRunner.h"

namespace uc::core {

ProcessRunner::ProcessRunner(QObject* parent)
    : QObject(parent)
{
    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &ProcessRunner::onTimeout);

    m_forceKillTimer.setSingleShot(true);
    connect(&m_forceKillTimer, &QTimer::timeout, this, &ProcessRunner::onForceKillTimeout);
}

ProcessRunner::~ProcessRunner() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->disconnect();
        m_process->terminate();
        if (!m_process->waitForFinished(200)) {
            m_process->kill();
            m_process->waitForFinished(300);
        }
    }
}

bool ProcessRunner::isRunning() const {
    return m_process && m_process->state() != QProcess::NotRunning;
}

QProcessEnvironment ProcessRunner::cleanEnvironment() {
    auto env = QProcessEnvironment::systemEnvironment();
    // Sanitize LD_LIBRARY_PATH if running within an AppImage to prevent host tools (ffmpeg, magick, ffprobe)
    // from crashing due to ABI mismatches with AppImage's bundled libraries.
    if (env.contains("ORIGINAL_LD_LIBRARY_PATH")) {
        const QString orig = env.value("ORIGINAL_LD_LIBRARY_PATH");
        if (orig.isEmpty()) {
            env.remove("LD_LIBRARY_PATH");
        } else {
            env.insert("LD_LIBRARY_PATH", orig);
        }
    } else if (env.contains("APPIMAGE") || env.contains("APPDIR")) {
        env.remove("LD_LIBRARY_PATH");
    }
    return env;
}

ProcessResult ProcessRunner::executeSync(const QString& program, const QStringList& arguments, int timeoutMs) {
    ProcessResult res;
    res.program = program;
    res.arguments = arguments;

    QElapsedTimer timer;
    timer.start();

    QProcess proc;
    proc.setProcessEnvironment(cleanEnvironment());
    proc.start(program, arguments);

    // Distinguish "failed to start" (missing binary / permissions) from a
    // genuine timeout. waitForStarted() returns false immediately when the
    // OS cannot launch the process.
    if (!proc.waitForStarted(3000)) {
        res.exitReason = ProcessExitReason::FailedToStart;
        res.timedOut = false;
        res.durationMs = timer.elapsed();
        res.exitCode = -1;
        res.errorMessage = QString("Failed to start process '%1' (not found or permission denied).").arg(program);
        return res;
    }

    // timeoutMs <= 0 means "no timeout" (aligned with async start()).
    const bool hasTimeout = timeoutMs > 0;
    const bool finished = hasTimeout ? proc.waitForFinished(timeoutMs)
                                     : proc.waitForFinished(-1);
    if (!finished) {
        // Genuine timeout: forcefully terminate to avoid zombie / fd leaks.
        proc.kill();
        proc.waitForFinished(1000);
        res.exitReason = ProcessExitReason::TimedOut;
        res.timedOut = true;
        res.durationMs = timer.elapsed();
        res.standardOutput = proc.readAllStandardOutput();
        res.standardError = proc.readAllStandardError();
        res.errorMessage = QString("Process '%1' timed out after %2 ms").arg(program).arg(timeoutMs);
        return res;
    }

    res.durationMs = timer.elapsed();
    res.exitCode = proc.exitCode();
    res.exitReason = (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0)
                         ? ProcessExitReason::Normal
                         : (proc.exitStatus() == QProcess::CrashExit ? ProcessExitReason::Crashed : ProcessExitReason::Normal);
    res.standardOutput = proc.readAllStandardOutput();
    res.standardError = proc.readAllStandardError();
    if (proc.exitCode() != 0) {
        res.errorMessage = QString::fromUtf8(res.standardError).trimmed();
    }
    return res;
}

bool ProcessRunner::start(const QString& program, const QStringList& arguments, int timeoutMs) {
    if (isRunning()) {
        return false;
    }

    m_isFinalized = false;
    m_isCancellationRequested = false;
    m_program = program;
    m_arguments = arguments;
    m_result = ProcessResult();

    m_process = std::make_unique<QProcess>();
    m_process->setProcessEnvironment(cleanEnvironment());

    connect(m_process.get(), &QProcess::started, this, &ProcessRunner::onProcessStarted);
    connect(m_process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProcessRunner::onProcessFinished);
    connect(m_process.get(), &QProcess::errorOccurred, this, &ProcessRunner::onProcessError);
    connect(m_process.get(), &QProcess::readyReadStandardOutput, this, &ProcessRunner::onStdoutReady);
    connect(m_process.get(), &QProcess::readyReadStandardError, this, &ProcessRunner::onStderrReady);

    m_elapsedTimer.start();
    m_process->start(program, arguments);

    if (timeoutMs > 0) {
        m_timeoutTimer.start(timeoutMs);
    }

    return true;
}

void ProcessRunner::cancel(int gracefulWaitMs) {
    if (!isRunning() || m_isCancellationRequested) {
        return;
    }

    m_isCancellationRequested = true;
    m_timeoutTimer.stop();

    // Request termination gracefully
    m_process->terminate();
    m_forceKillTimer.start(gracefulWaitMs);
}

void ProcessRunner::onProcessStarted() {
    emit started();
}

void ProcessRunner::onStdoutReady() {
    if (!m_process) return;
    const QByteArray chunk = m_process->readAllStandardOutput();
    // Cap buffered output (~2 MiB per stream) so high-frequency -progress
    // output cannot accumulate gigabytes of RAM on long conversions.
    constexpr qsizetype kMaxBufferedBytes = 2 * 1024 * 1024;
    if (m_result.standardOutput.size() < kMaxBufferedBytes) {
        m_result.standardOutput.append(chunk.left(kMaxBufferedBytes - m_result.standardOutput.size()));
    }
    emit readyReadStandardOutput(chunk);
}

void ProcessRunner::onStderrReady() {
    if (!m_process) return;
    const QByteArray chunk = m_process->readAllStandardError();
    constexpr qsizetype kMaxBufferedBytes = 2 * 1024 * 1024;
    if (m_result.standardError.size() < kMaxBufferedBytes) {
        m_result.standardError.append(chunk.left(kMaxBufferedBytes - m_result.standardError.size()));
    }
    emit readyReadStandardError(chunk);
}

void ProcessRunner::onTimeout() {
    if (m_isFinalized || !isRunning()) return;

    finalizeProcess(ProcessExitReason::TimedOut, -1, "Process execution timed out.");
    if (m_process) {
        m_process->kill();
    }
}

void ProcessRunner::onForceKillTimeout() {
    if (isRunning()) {
        m_process->kill();
    }
}

void ProcessRunner::onProcessError(QProcess::ProcessError error) {
    if (m_isFinalized) return;

    if (error == QProcess::FailedToStart) {
        finalizeProcess(ProcessExitReason::FailedToStart, -1,
                        QString("Executable '%1' failed to start (file not found or permission denied).").arg(m_program));
    } else if (error == QProcess::Crashed && !m_isCancellationRequested) {
        finalizeProcess(ProcessExitReason::Crashed, -1, "Process crashed unexpectedly during execution.");
    }
}

void ProcessRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (m_isFinalized) return;

    m_timeoutTimer.stop();
    m_forceKillTimer.stop();

    // Read any remaining output buffers
    onStdoutReady();
    onStderrReady();

    if (m_isCancellationRequested) {
        finalizeProcess(ProcessExitReason::Cancelled, exitCode, "Process was cancelled by user.");
    } else if (exitStatus == QProcess::CrashExit) {
        finalizeProcess(ProcessExitReason::Crashed, exitCode, "Process crashed or terminated abnormally.");
    } else {
        QString errStr;
        if (exitCode != 0) {
            errStr = QString("Process exited with non-zero exit code: %1. Stderr: %2")
                         .arg(exitCode)
                         .arg(QString::fromUtf8(m_result.standardError).trimmed());
        }
        finalizeProcess(ProcessExitReason::Normal, exitCode, errStr);
    }
}

void ProcessRunner::finalizeProcess(ProcessExitReason reason, int exitCode, const QString& errorMsg) {
    if (m_isFinalized) return;
    m_isFinalized = true;

    m_timeoutTimer.stop();
    m_forceKillTimer.stop();

    if (m_process) {
        m_process->disconnect(this);
    }

    m_result.program = m_program;
    m_result.arguments = m_arguments;
    m_result.durationMs = m_elapsedTimer.isValid() ? m_elapsedTimer.elapsed() : 0;
    m_result.timedOut = (reason == ProcessExitReason::TimedOut);
    m_result.exitReason = reason;
    m_result.exitCode = exitCode;
    m_result.errorMessage = errorMsg;

    // Emit finished asynchronously via queued invocation so that callers/listeners
    // have time to connect and set up spies even if QProcess fails immediately/synchronously during start()
    const auto resCopy = m_result;
    QMetaObject::invokeMethod(this, [this, resCopy]() {
        emit finished(resCopy);
    }, Qt::QueuedConnection);
}

} // namespace uc::core
