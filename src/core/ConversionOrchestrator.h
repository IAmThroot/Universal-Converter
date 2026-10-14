#pragma once

#include <QObject>
#include <QList>
#include <QMap>
#include <memory>
#include "ConversionJob.h"
#include "ProcessRunner.h"

namespace uc::engines {
    class FfmpegEngine;
    class ImageMagickEngine;
    class PandocEngine;
}

namespace uc::core {

class ConversionOrchestrator : public QObject {
    Q_OBJECT

public:
    explicit ConversionOrchestrator(int maxConcurrency = 4, QObject* parent = nullptr);
    ~ConversionOrchestrator() override;

    void setFfmpegEngine(std::shared_ptr<engines::FfmpegEngine> engine);
    void setImageMagickEngine(std::shared_ptr<engines::ImageMagickEngine> engine);
    void setPandocEngine(std::shared_ptr<engines::PandocEngine> engine);

    // Concurrency configuration
    int maxConcurrency() const { return m_maxConcurrency; }
    void setMaxConcurrency(int concurrency);

    int maxFfmpegConcurrency() const { return m_maxFfmpegConcurrency; }
    void setMaxFfmpegConcurrency(int count) { m_maxFfmpegConcurrency = qMax(1, count); }

    int maxImageMagickConcurrency() const { return m_maxImageMagickConcurrency; }
    void setMaxImageMagickConcurrency(int count) { m_maxImageMagickConcurrency = qMax(1, count); }

    int maxPandocConcurrency() const { return m_maxPandocConcurrency; }
    void setMaxPandocConcurrency(int count) { m_maxPandocConcurrency = qMax(1, count); }

    int activeJobsForEngine(EngineType engine) const;

    // Queue Management
    void addJob(std::shared_ptr<ConversionJob> job);
    void cancelJob(const QString& jobId);
    void cancelAllJobs();
    void removeJob(const QString& jobId);
    void clearCompletedJobs();

    // Inspection
    std::shared_ptr<ConversionJob> findJob(const QString& jobId) const;
    QList<std::shared_ptr<ConversionJob>> allJobs() const;
    QList<std::shared_ptr<ConversionJob>> queuedJobs() const;
    QList<std::shared_ptr<ConversionJob>> activeJobs() const;
    QList<std::shared_ptr<ConversionJob>> completedJobs() const;

    int activeJobCount() const { return m_runningJobs.size(); }
    int totalJobCount() const { return m_jobs.size(); }

signals:
    void jobAdded(const QString& jobId);
    void jobStarted(const QString& jobId);
    void jobFinished(const QString& jobId, JobState state);
    void jobRemoved(const QString& jobId);
    void queueDrained();

private slots:
    void pumpQueue();

private:
    void onJobFinishedInternal(const QString& jobId, JobState finalState);
    bool isTransientError(const QString& errorMsg, const QString& diagnostic) const;

    int m_maxConcurrency{4};
    int m_maxFfmpegConcurrency{2};
    int m_maxImageMagickConcurrency{4};
    int m_maxPandocConcurrency{4};

    QList<std::shared_ptr<ConversionJob>> m_jobs;
    QMap<QString, std::shared_ptr<ConversionJob>> m_jobMap;

    // Running tracking
    QMap<QString, std::shared_ptr<ConversionJob>> m_runningJobs;
    std::shared_ptr<engines::FfmpegEngine> m_ffmpegEngine;
    std::shared_ptr<engines::ImageMagickEngine> m_imageMagickEngine;
    std::shared_ptr<engines::PandocEngine> m_pandocEngine;
};

} // namespace uc::core
