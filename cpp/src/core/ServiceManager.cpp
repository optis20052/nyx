#include "ServiceManager.h"
#include "ConfigManager.h"
#include "utils/PolkitHelper.h"

#include <QLoggingCategory>
#include <QProcess>

Q_LOGGING_CATEGORY(lcServiceManager, "nyx.core.servicemanager")

namespace nyx::core {

using nyx::models::ServiceStatus;

ServiceManager::ServiceManager(ConfigManager *configManager)
    : m_configManager(configManager)
{
}

ServiceStatus ServiceManager::getServiceStatus(const QString &serviceName,
                                                bool isUserService) const
{
    QStringList args;
    if (isUserService) {
        args << QStringLiteral("--user");
    }
    args << QStringLiteral("show") << serviceName
         << QStringLiteral("--property=ActiveState") << QStringLiteral("--value");

    QProcess process;
    process.start(QStringLiteral("systemctl"), args);

    if (!process.waitForFinished(m_timeout)) {
        qCWarning(lcServiceManager) << "Timeout getting status for" << serviceName;
        return ServiceStatus::Unknown;
    }

    if (process.exitCode() != 0) {
        qCWarning(lcServiceManager) << "Failed to get status for" << serviceName
                                    << ":" << QString::fromUtf8(process.readAllStandardError());
        return ServiceStatus::Unknown;
    }

    const QString statusStr = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return nyx::models::serviceStatusFromString(statusStr);
}

std::pair<bool, QString> ServiceManager::startService(const QString &serviceName, bool isUserService)
{
    return executeSystemctlAction(QStringLiteral("start"), serviceName, isUserService);
}

std::pair<bool, QString> ServiceManager::stopService(const QString &serviceName, bool isUserService)
{
    return executeSystemctlAction(QStringLiteral("stop"), serviceName, isUserService);
}

std::pair<bool, QString> ServiceManager::restartService(const QString &serviceName, bool isUserService)
{
    return executeSystemctlAction(QStringLiteral("restart"), serviceName, isUserService);
}

std::pair<bool, QString> ServiceManager::enableService(const QString &serviceName, bool isUserService)
{
    return executeSystemctlAction(QStringLiteral("enable"), serviceName, isUserService);
}

std::pair<bool, QString> ServiceManager::disableService(const QString &serviceName, bool isUserService)
{
    return executeSystemctlAction(QStringLiteral("disable"), serviceName, isUserService);
}

QString ServiceManager::getServiceLogs(const QString &serviceName, int lines,
                                        bool isUserService) const
{
    QStringList args;
    if (isUserService) {
        args << QStringLiteral("--user");
    }
    args << QStringLiteral("-u") << serviceName
         << QStringLiteral("-n") << QString::number(lines)
         << QStringLiteral("--no-pager")
         << QStringLiteral("--output=short-iso");

    QProcess process;
    process.start(QStringLiteral("journalctl"), args);

    if (!process.waitForFinished(m_timeout)) {
        qCWarning(lcServiceManager) << "Timeout getting logs for" << serviceName;
        return QStringLiteral("Error: Timeout while fetching logs for %1").arg(serviceName);
    }

    if (process.exitCode() != 0) {
        const QString err = QString::fromUtf8(process.readAllStandardError()).trimmed();
        qCWarning(lcServiceManager) << "Failed to get logs for" << serviceName << ":" << err;
        return QStringLiteral("Error: %1").arg(err);
    }

    return QString::fromUtf8(process.readAllStandardOutput());
}

std::optional<int> ServiceManager::getServiceUptime(const QString &serviceName,
                                                     bool isUserService) const
{
    QStringList args;
    if (isUserService) {
        args << QStringLiteral("--user");
    }
    args << QStringLiteral("show") << serviceName
         << QStringLiteral("--property=ActiveEnterTimestampMonotonic")
         << QStringLiteral("--value");

    QProcess process;
    process.start(QStringLiteral("systemctl"), args);

    if (!process.waitForFinished(m_timeout)) {
        qCDebug(lcServiceManager) << "Timeout getting uptime for" << serviceName;
        return std::nullopt;
    }

    if (process.exitCode() != 0) {
        return std::nullopt;
    }

    const QString timestamp = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (timestamp.isEmpty() || timestamp == QLatin1String("0")) {
        return std::nullopt;
    }

    bool ok = false;
    const qint64 microseconds = timestamp.toLongLong(&ok);
    if (!ok) {
        return std::nullopt;
    }

    return static_cast<int>(microseconds / 1000000);
}

bool ServiceManager::serviceExists(const QString &serviceName, bool isUserService) const
{
    QStringList args;
    if (isUserService) {
        args << QStringLiteral("--user");
    }
    args << QStringLiteral("list-unit-files") << serviceName
         << QStringLiteral("--no-pager") << QStringLiteral("--no-legend");

    QProcess process;
    process.start(QStringLiteral("systemctl"), args);

    if (!process.waitForFinished(m_timeout)) {
        qCWarning(lcServiceManager) << "Timeout checking if service" << serviceName << "exists";
        return false;
    }

    return !QString::fromUtf8(process.readAllStandardOutput()).trimmed().isEmpty();
}

std::pair<bool, QString> ServiceManager::executeSystemctlAction(const QString &action,
                                                                 const QString &serviceName,
                                                                 bool isUserService)
{
    const bool requiresPrivilege = !isUserService
        && !nyx::utils::PolkitHelper::isPasswordlessEnabled(m_configManager);

    QString program;
    QStringList args;

    if (requiresPrivilege) {
        program = QStringLiteral("pkexec");
        args << QStringLiteral("systemctl");
    } else {
        program = QStringLiteral("systemctl");
    }

    if (isUserService) {
        args << QStringLiteral("--user");
    }
    args << action << serviceName;

    QProcess process;
    process.start(program, args);

    if (!process.waitForFinished(m_timeout)) {
        const QString msg = QStringLiteral("Timeout while trying to %1 %2").arg(action, serviceName);
        qCWarning(lcServiceManager) << msg;
        return {false, msg};
    }

    if (process.exitCode() != 0) {
        const QString err = QString::fromUtf8(process.readAllStandardError()).trimmed();
        const QString msg = err.isEmpty()
            ? QStringLiteral("Failed to %1 %2").arg(action, serviceName)
            : err;
        qCWarning(lcServiceManager) << "Failed to" << action << serviceName << ":" << msg;
        return {false, msg};
    }

    qCInfo(lcServiceManager) << "Successfully" << action + QStringLiteral("ed") << serviceName;
    return {true, {}};
}

} // namespace nyx::core
