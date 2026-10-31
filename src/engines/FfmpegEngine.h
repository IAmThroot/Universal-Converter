#pragma once

#include <QObject>
#include <memory>
#include <atomic>
#include <QThreadPool>
#include "../core/ConversionJob.h"
#include "../core/ProcessRunner.h"
#include "MediaProbe.h"
#include "FfmpegArgBuilder.h"
#include "FfmpegProgressParser.h"
#include "OutputValidator.h"

namespace uc::engines {

class FfmpegEngine : public QObject {
    Q_OBJECT

  public:
    explicit FfmpegEngine(QObject *parent = nullptr);
    ~FfmpegEngine() override;
    void shutdown();

    bool canConvert(const QString &inputFormatId, const QString &outputFormatId) const;

    // Asynchronously executes the given job using ProcessRunner
    bool executeJob(std::shared_ptr<core::ConversionJob> job);

    void cancelJob(const QString &jobId);

  signals:
    void jobFinished(const QString &jobId, bool success, const QString &message);

  private slots:
    void onProcessOutput(const QString &jobId, const QByteArray &data);
    void onProcessFinished(const QString &jobId, const core::ProcessResult &result);

  private:
    struct RunningContext {
        std::shared_ptr<core::ConversionJob> job;
        std::shared_ptr<core::ProcessRunner> runner;
        std::unique_ptr<FfmpegProgressParser> parser;
        QString tempOutputPath;
        QString finalOutputPath;
        std::atomic_bool isCancelled{false};
    };

    QThreadPool m_workers;
    QMap<QString, std::shared_ptr<RunningContext>> m_activeContexts;
    void finishCancellation(const QString &id);
};

} // namespace uc::engines
