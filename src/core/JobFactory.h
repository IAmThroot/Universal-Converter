#pragma once
#include "ConversionJob.h"
#include "OutputPathPlanner.h"
namespace uc::core {
struct JobCreation {
    std::shared_ptr<ConversionJob> job;
    QString error;
};
class JobFactory {
  public:
    static JobCreation create(const QString &input, const QString &output, const QVariantMap &options,
                              app::DestinationMode mode, const QString &destination);
};
} // namespace uc::core
