#include "JobFactory.h"
#include "EngineCapabilityResolver.h"
#include <QUuid>
namespace uc::core {
JobCreation JobFactory::create(const QString &input, const QString &output, const QVariantMap &options,
                               app::DestinationMode mode, const QString &destination) {
    const auto *format = FormatRegistry::instance().findByFilePath(input);
    if (!format)
        return {{}, "Unrecognized source format"};
    const auto route = EngineCapabilityResolver::instance().resolveRoute(format->id, output, options);
    if (!route.available)
        return {{}, route.reason};
    if (options.contains("quality") &&
        (options.value("quality").toInt() < 1 || options.value("quality").toInt() > 100))
        return {{}, "Quality must be between 1 and 100"};
    for (const auto &key : {"customWidth", "customHeight", "width", "height", "customVideoBitrateKbps",
                            "customAudioBitrateKbps"})
        if (options.contains(key) && options.value(key).toInt() < 0)
            return {{}, QString("Invalid negative value: %1").arg(key)};
    if (options.contains("preset") &&
        (options.value("preset").toInt() < 0 || options.value("preset").toInt() > 6))
        return {{}, "Invalid encoder speed preset"};
    const auto plan = OutputPathPlanner::planOutputPath(input, output, mode, destination);
    if (!plan.isValid)
        return {{}, plan.errorMessage};
    EngineType engine = route.engine == "FFmpeg"        ? EngineType::FFmpeg
                        : route.engine == "ImageMagick" ? EngineType::ImageMagick
                                                        : EngineType::Pandoc;
    auto job =
        std::make_shared<ConversionJob>(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                        QStringList{input}, format->id, output, plan.targetDirectory, engine);
    job->setOptions(options);
    job->setFinalOutputPath(plan.finalFilePath);
    return {job, {}};
}
} // namespace uc::core
