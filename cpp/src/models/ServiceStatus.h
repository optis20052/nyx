#pragma once

#include <QString>

namespace nyx::models {

enum class ServiceStatus {
    Active,
    Inactive,
    Failed,
    Activating,
    Deactivating,
    Unknown
};

inline ServiceStatus serviceStatusFromString(const QString &str)
{
    const QString lower = str.toLower();
    if (lower == QLatin1String("active"))        return ServiceStatus::Active;
    if (lower == QLatin1String("inactive"))      return ServiceStatus::Inactive;
    if (lower == QLatin1String("failed"))        return ServiceStatus::Failed;
    if (lower == QLatin1String("activating"))    return ServiceStatus::Activating;
    if (lower == QLatin1String("deactivating"))  return ServiceStatus::Deactivating;
    return ServiceStatus::Unknown;
}

inline QString serviceStatusToString(ServiceStatus status)
{
    switch (status) {
    case ServiceStatus::Active:       return QStringLiteral("active");
    case ServiceStatus::Inactive:     return QStringLiteral("inactive");
    case ServiceStatus::Failed:       return QStringLiteral("failed");
    case ServiceStatus::Activating:   return QStringLiteral("activating");
    case ServiceStatus::Deactivating: return QStringLiteral("deactivating");
    case ServiceStatus::Unknown:      return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

} // namespace nyx::models
