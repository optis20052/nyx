#include "core/ConfigManager.h"
#include "core/NotificationManager.h"
#include "core/ServiceManager.h"
#include "models/ServiceConfig.h"
#include "models/ServiceInfo.h"
#include "models/ServiceStatus.h"
#include "utils/Constants.h"
#include "utils/PolkitHelper.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(nyx::utils::APP_NAME);
    app.setApplicationVersion(nyx::utils::APP_VERSION);
    app.setOrganizationName(nyx::utils::ORGANIZATION_NAME);

    qInfo() << "NyxApp C++ smoke test";
    qInfo() << "Config dir:" << nyx::utils::configDir();
    qInfo() << "Config file:" << nyx::utils::configFile();

    // Test ConfigManager
    nyx::core::ConfigManager configManager;
    configManager.loadConfig();
    qInfo() << "Services loaded:" << configManager.getServices().size();
    qInfo() << "Update interval:" << configManager.getSetting("update_interval", 5);

    // Test ServiceStatus
    auto status = nyx::models::serviceStatusFromString("active");
    qInfo() << "Status from string 'active':" << nyx::models::serviceStatusToString(status);

    // Test ServiceConfig
    nyx::models::ServiceConfig testConfig;
    testConfig.name = "test-service";
    testConfig.displayName = "Test Service";
    testConfig.validate();
    auto json = testConfig.toJson();
    auto restored = nyx::models::ServiceConfig::fromJson(json);
    qInfo() << "ServiceConfig roundtrip:" << (restored.name == testConfig.name ? "OK" : "FAIL");

    // Test ServiceInfo
    nyx::models::ServiceInfo info;
    info.config = testConfig;
    info.status = nyx::models::ServiceStatus::Active;
    info.uptime = 3661;
    info.memoryUsage = 256 * 1024 * 1024;
    qInfo() << "Uptime:" << info.getUptimeStr();
    qInfo() << "Memory:" << info.getMemoryStr();

    // Test NotificationManager (just construction)
    nyx::core::NotificationManager notificationManager;

    // Test ServiceManager (just construction)
    nyx::core::ServiceManager serviceManager(&configManager);

    // Test PolkitHelper
    qInfo() << "Current username:" << nyx::utils::PolkitHelper::getCurrentUsername();

    qInfo() << "Smoke test passed.";
    return 0;
}
