#pragma once

#include "ServiceConfig.h"
#include "ServiceStatus.h"

#include <QString>
#include <optional>

namespace nyx::models {

struct ServiceInfo {
    ServiceConfig config;
    ServiceStatus status = ServiceStatus::Unknown;
    std::optional<int> uptime;
    std::optional<qint64> memoryUsage;

    QString getUptimeStr() const;
    QString getMemoryStr() const;
};

} // namespace nyx::models
