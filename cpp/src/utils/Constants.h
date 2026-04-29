#pragma once

#include <QColor>
#include <QDir>
#include <QHash>
#include <QStandardPaths>
#include <QString>

#include "models/ServiceStatus.h"

namespace nyx::utils {

inline const QString APP_NAME = QStringLiteral("NyxApp");
inline const QString APP_VERSION = QStringLiteral("1.0.0");
inline const QString ORGANIZATION_NAME = QStringLiteral("NyxApp");

inline const QString DEFAULT_SERVICE_ICON = QStringLiteral("application-x-executable");
inline const QString APP_ICON = QStringLiteral("preferences-system");

inline constexpr int DEFAULT_UPDATE_INTERVAL = 5;       // seconds
inline constexpr int DEFAULT_LOG_LINES = 100;
inline constexpr int MAX_LOG_LINES = 10000;

inline constexpr int NOTIFICATION_TIMEOUT = 5000;       // milliseconds
inline constexpr int NOTIFICATION_RATE_LIMIT = 30;      // seconds

inline QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/nyxapp");
}

inline QString configFile()
{
    return configDir() + QStringLiteral("/config.json");
}

inline QString logFile()
{
    return configDir() + QStringLiteral("/app.log");
}

inline QString iconsDir()
{
    return configDir() + QStringLiteral("/icons");
}

inline QString appIconPath()
{
    return QStringLiteral(":/icons/nyxapp.svg");
}

inline QString trayIconPath()
{
    return QStringLiteral(":/icons/nyxapp-symbolic.svg");
}

inline QString trayIconLightPath()
{
    return QStringLiteral(":/icons/nyxapp-light.svg");
}

inline QString trayIconDarkPath()
{
    return QStringLiteral(":/icons/nyxapp-dark.svg");
}

inline QColor statusColor(nyx::models::ServiceStatus status)
{
    switch (status) {
    case nyx::models::ServiceStatus::Active:       return QColor(46, 204, 113);   // Green
    case nyx::models::ServiceStatus::Inactive:     return QColor(149, 165, 166);  // Gray
    case nyx::models::ServiceStatus::Failed:       return QColor(231, 76, 60);    // Red
    case nyx::models::ServiceStatus::Activating:   return QColor(241, 196, 15);   // Yellow
    case nyx::models::ServiceStatus::Deactivating: return QColor(241, 196, 15);   // Yellow
    case nyx::models::ServiceStatus::Unknown:      return QColor(149, 165, 166);  // Gray
    }
    return QColor(149, 165, 166);
}

} // namespace nyx::utils
