#include "ConfigManager.h"
#include "utils/Constants.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QSaveFile>

Q_LOGGING_CATEGORY(lcConfigManager, "nyx.core.configmanager")

namespace nyx::core {

using nyx::models::ServiceConfig;

ConfigManager::ConfigManager()
{
    ensureConfigDir();
}

void ConfigManager::ensureConfigDir()
{
    QDir().mkpath(nyx::utils::configDir());
    QDir().mkpath(nyx::utils::iconsDir());
}

bool ConfigManager::loadConfig()
{
    const QString path = nyx::utils::configFile();

    if (!QFile::exists(path)) {
        qCInfo(lcConfigManager) << "Config file not found, using defaults";
        loadDefaults();
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcConfigManager) << "Failed to open config file:" << file.errorString();
        loadDefaults();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(lcConfigManager) << "JSON parsing error:" << parseError.errorString();
        loadDefaults();
        return false;
    }

    if (!doc.isObject()) {
        qCWarning(lcConfigManager) << "Config must be a JSON object";
        loadDefaults();
        return false;
    }

    const QJsonObject root = doc.object();

    if (!validateConfig(root)) {
        qCWarning(lcConfigManager) << "Invalid config file, using defaults";
        loadDefaults();
        return false;
    }

    // Load services
    m_services.clear();
    const QJsonArray servicesArray = root[QLatin1String("services")].toArray();
    for (const QJsonValue &val : servicesArray) {
        if (!val.isObject()) continue;
        ServiceConfig service = ServiceConfig::fromJson(val.toObject());
        if (service.validate()) {
            m_services.append(service);
        } else {
            qCWarning(lcConfigManager) << "Skipping invalid service config";
        }
    }

    // Load settings
    m_settings = root[QLatin1String("settings")].toObject();
    ensureDefaultSettings();

    qCInfo(lcConfigManager) << "Loaded" << m_services.size() << "services from config";
    return true;
}

bool ConfigManager::saveConfig()
{
    const QString path = nyx::utils::configFile();

    // Create backup if config exists
    if (QFile::exists(path)) {
        const QString backupPath = path + QStringLiteral(".bak");
        QFile::remove(backupPath);
        QFile::copy(path, backupPath);
        qCDebug(lcConfigManager) << "Created backup at" << backupPath;
    }

    // Build JSON
    QJsonArray servicesArray;
    for (const ServiceConfig &service : m_services) {
        servicesArray.append(service.toJson());
    }

    QJsonObject root;
    root[QLatin1String("version")] = QLatin1String(CONFIG_VERSION);
    root[QLatin1String("services")] = servicesArray;
    root[QLatin1String("settings")] = m_settings;

    const QJsonDocument doc(root);

    // Atomic write via QSaveFile
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcConfigManager) << "Failed to open config for writing:" << file.errorString();
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));

    if (!file.commit()) {
        qCWarning(lcConfigManager) << "Failed to save config:" << file.errorString();
        return false;
    }

    qCInfo(lcConfigManager) << "Saved" << m_services.size() << "services to config";
    return true;
}

bool ConfigManager::addService(const ServiceConfig &service)
{
    for (const ServiceConfig &s : m_services) {
        if (s.name == service.name && s.serviceType == service.serviceType) {
            qCWarning(lcConfigManager) << "Service" << service.name
                                       << "(" << service.serviceType << ") already exists";
            return false;
        }
    }

    m_services.append(service);
    saveConfig();
    qCInfo(lcConfigManager) << "Added service:" << service.name;
    return true;
}

bool ConfigManager::removeService(const QString &serviceName, const QString &serviceType)
{
    const int originalCount = m_services.size();

    m_services.erase(
        std::remove_if(m_services.begin(), m_services.end(),
                       [&](const ServiceConfig &s) {
                           return s.name == serviceName && s.serviceType == serviceType;
                       }),
        m_services.end());

    if (m_services.size() < originalCount) {
        saveConfig();
        qCInfo(lcConfigManager) << "Removed service:" << serviceName;
        return true;
    }

    qCWarning(lcConfigManager) << "Service" << serviceName << "(" << serviceType << ") not found";
    return false;
}

bool ConfigManager::updateService(const QString &oldName, const QString &oldType,
                                   const ServiceConfig &updatedService)
{
    for (int i = 0; i < m_services.size(); ++i) {
        if (m_services[i].name == oldName && m_services[i].serviceType == oldType) {
            m_services[i] = updatedService;
            saveConfig();
            qCInfo(lcConfigManager) << "Updated service:" << oldName;
            return true;
        }
    }

    qCWarning(lcConfigManager) << "Service" << oldName << "(" << oldType << ") not found";
    return false;
}

const ServiceConfig *ConfigManager::getService(const QString &serviceName,
                                                const QString &serviceType) const
{
    for (const ServiceConfig &s : m_services) {
        if (s.name == serviceName && s.serviceType == serviceType) {
            return &s;
        }
    }
    return nullptr;
}

QList<ServiceConfig> ConfigManager::getEnabledServices() const
{
    QList<ServiceConfig> result;
    for (const ServiceConfig &s : m_services) {
        if (s.enabled) {
            result.append(s);
        }
    }
    return result;
}

QVariant ConfigManager::getSetting(const QString &key, const QVariant &defaultValue) const
{
    if (m_settings.contains(key)) {
        return m_settings[key].toVariant();
    }
    return defaultValue;
}

void ConfigManager::setSetting(const QString &key, const QVariant &value)
{
    m_settings[key] = QJsonValue::fromVariant(value);
    saveConfig();
}

bool ConfigManager::validateConfig(const QJsonObject &data) const
{
    if (data.contains(QLatin1String("services")) && !data[QLatin1String("services")].isArray()) {
        qCWarning(lcConfigManager) << "Services must be an array";
        return false;
    }

    if (data.contains(QLatin1String("settings")) && !data[QLatin1String("settings")].isObject()) {
        qCWarning(lcConfigManager) << "Settings must be an object";
        return false;
    }

    return true;
}

void ConfigManager::loadDefaults()
{
    m_services.clear();
    m_settings = QJsonObject();
    ensureDefaultSettings();
    qCInfo(lcConfigManager) << "Loaded default configuration";
}

void ConfigManager::ensureDefaultSettings()
{
    auto setDefault = [this](const QString &key, const QJsonValue &value) {
        if (!m_settings.contains(key)) {
            m_settings[key] = value;
        }
    };

    setDefault(QStringLiteral("update_interval"), nyx::utils::DEFAULT_UPDATE_INTERVAL);
    setDefault(QStringLiteral("show_notifications"), true);
    setDefault(QStringLiteral("minimize_to_tray"), false);
    setDefault(QStringLiteral("passwordless_mode"), false);
    setDefault(QStringLiteral("show_main_tray"), true);
}

} // namespace nyx::core
