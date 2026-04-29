#include "ServiceInfo.h"

namespace nyx::models {

QString ServiceInfo::getUptimeStr() const
{
    if (!uptime.has_value()) {
        return QStringLiteral("N/A");
    }

    int total = uptime.value();
    int hours = total / 3600;
    int remainder = total % 3600;
    int minutes = remainder / 60;
    int seconds = remainder % 60;

    if (hours > 0) {
        return QStringLiteral("%1h %2m").arg(hours).arg(minutes);
    }
    if (minutes > 0) {
        return QStringLiteral("%1m %2s").arg(minutes).arg(seconds);
    }
    return QStringLiteral("%1s").arg(seconds);
}

QString ServiceInfo::getMemoryStr() const
{
    if (!memoryUsage.has_value()) {
        return QStringLiteral("N/A");
    }

    double mb = static_cast<double>(memoryUsage.value()) / (1024.0 * 1024.0);
    if (mb >= 1024.0) {
        double gb = mb / 1024.0;
        return QStringLiteral("%1 GB").arg(gb, 0, 'f', 1);
    }
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 0);
}

} // namespace nyx::models
