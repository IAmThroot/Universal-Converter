#pragma once

#include <QObject>
#include <memory>
#include "../core/ConversionJob.h"
#include "../core/ProcessRunner.h"
#include "ImageMagickCapabilities.h"
#include "ImageMagickArgBuilder.h"
#include "ImageInspector.h"

namespace uc::engines {

class ImageMagickEngine : public QObject {
    Q_OBJECT

public:
    explicit ImageMagickEngine(QObject* parent = nullptr);
    ~ImageMagickEngine() override = default;

    bool canConvert(const QString& inputFormatId, const QString& outputFormatId) const;

    bool executeJob(std::shared_ptr<core::ConversionJob> job);
    void cancelJob(const QString& jobId);

signals:
    void jobFinished(const QString& jobId, bool success, const QString& message);

private slots:
    void onProcessFinished(const QString& jobId, const core::ProcessResult& result);

private:
    struct RunningContext {
        std::shared_ptr<core::ConversionJob> job;
        std::shared_ptr<core::ProcessRunner> runner;
        QString tempOutputPath;
        QString finalOutputPath;
        bool isCancelled{false};
    };

    QMap<QString, std::shared_ptr<RunningContext>> m_activeContexts;
};

} // namespace uc::engines
