#include "ServiceConfig.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcServiceConfig, "nyx.models.serviceconfig")

namespace nyx::models {

bool ServiceConfig::validate()
{
    if (name.isEmpty()) {
        qCWarning(lcServiceConfig) << "Service name cannot be empty";
        return false;
    }

    if (serviceType != QLatin1String("user") && serviceType != QLatin1String("system")) {
        qCWarning(lcServiceConfig) << "Invalid service_type:" << serviceType
                                   << "- must be 'user' or 'system'";
        return false;
    }

    if (displayName.isEmpty()) {
        displayName = name.at(0).toUpper() + name.mid(1);
    }

    return true;
}

bool ServiceConfig::isUserService() const
{
    return serviceType == QLatin1String("user");
}

QString ServiceConfig::getIconForTheme(bool isDarkTheme) const
{
    if (isDarkTheme) {
        return iconDark.isEmpty() ? icon : iconDark;
    }
    return iconLight.isEmpty() ? icon : iconLight;
}

QJsonObject ServiceConfig::toJson() const
{
    QJsonObject obj;
    obj[QLatin1String("name")] = name;
    obj[QLatin1String("display_name")] = displayName;
    obj[QLatin1String("icon")] = icon;
    obj[QLatin1String("service_type")] = serviceType;
    obj[QLatin1String("auto_start")] = autoStart;
    obj[QLatin1String("enabled")] = enabled;

    if (!iconLight.isEmpty() && iconLight != icon) {
        obj[QLatin1String("icon_light")] = iconLight;
    }
    if (!iconDark.isEmpty() && iconDark != icon) {
        obj[QLatin1String("icon_dark")] = iconDark;
    }

    return obj;
}

ServiceConfig ServiceConfig::fromJson(const QJsonObject &obj)
{
    ServiceConfig config;
    config.name = obj[QLatin1String("name")].toString();

    const QString defaultDisplay = config.name.isEmpty()
        ? QString()
        : config.name.at(0).toUpper() + config.name.mid(1);

    config.displayName = obj[QLatin1String("display_name")].toString(defaultDisplay);
    config.icon = obj[QLatin1String("icon")].toString(QStringLiteral("application-x-executable"));
    config.iconLight = obj[QLatin1String("icon_light")].toString();
    config.iconDark = obj[QLatin1String("icon_dark")].toString();
    config.serviceType = obj[QLatin1String("service_type")].toString(QStringLiteral("user"));
    config.autoStart = obj[QLatin1String("auto_start")].toBool(false);
    config.enabled = obj[QLatin1String("enabled")].toBool(true);

    return config;
}

} // namespace nyx::models
