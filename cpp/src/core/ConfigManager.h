#pragma once

#include "models/ServiceConfig.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariant>

namespace nyx::core {

class ConfigManager {
public:
    static constexpr const char *CONFIG_VERSION = "1.0";

    ConfigManager();

    bool loadConfig();
    bool saveConfig();

    bool addService(const nyx::models::ServiceConfig &service);
    bool removeService(const QString &serviceName, const QString &serviceType = QStringLiteral("user"));
    bool updateService(const QString &oldName, const QString &oldType,
                       const nyx::models::ServiceConfig &updatedService);

    const nyx::models::ServiceConfig *getService(const QString &serviceName,
                                                  const QString &serviceType = QStringLiteral("user")) const;

    QList<nyx::models::ServiceConfig> getEnabledServices() const;

    QVariant getSetting(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setSetting(const QString &key, const QVariant &value);

    const QList<nyx::models::ServiceConfig> &getServices() const { return m_services; }
    const QJsonObject &getSettings() const { return m_settings; }

private:
    void ensureConfigDir();
    void loadDefaults();
    void ensureDefaultSettings();
    bool validateConfig(const QJsonObject &data) const;

    QList<nyx::models::ServiceConfig> m_services;
    QJsonObject m_settings;
};

} // namespace nyx::core
