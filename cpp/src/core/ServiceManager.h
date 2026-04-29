#pragma once

#include "models/ServiceStatus.h"

#include <QString>
#include <optional>
#include <utility>

namespace nyx::core {

class ConfigManager;

class ServiceManager {
public:
    explicit ServiceManager(ConfigManager *configManager = nullptr);

    nyx::models::ServiceStatus getServiceStatus(const QString &serviceName,
                                                 bool isUserService = true) const;

    std::pair<bool, QString> startService(const QString &serviceName, bool isUserService = true);
    std::pair<bool, QString> stopService(const QString &serviceName, bool isUserService = true);
    std::pair<bool, QString> restartService(const QString &serviceName, bool isUserService = true);
    std::pair<bool, QString> enableService(const QString &serviceName, bool isUserService = true);
    std::pair<bool, QString> disableService(const QString &serviceName, bool isUserService = true);

    QString getServiceLogs(const QString &serviceName, int lines = 100,
                           bool isUserService = true) const;
    std::optional<int> getServiceUptime(const QString &serviceName, bool isUserService = true) const;
    bool serviceExists(const QString &serviceName, bool isUserService = true) const;

private:
    std::pair<bool, QString> executeSystemctlAction(const QString &action,
                                                     const QString &serviceName,
                                                     bool isUserService);

    int m_timeout = 10000; // milliseconds
    ConfigManager *m_configManager = nullptr;
};

} // namespace nyx::core
