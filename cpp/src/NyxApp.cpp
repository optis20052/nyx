#include "NyxApp.h"
#include "ui/MainWindow.h"
#include "ui/ServiceTrayIcon.h"
#include "ui/dialogs/AddServiceDialog.h"
#include "ui/dialogs/EditServiceDialog.h"
#include "ui/dialogs/LogViewerDialog.h"
#include "ui/dialogs/SettingsDialog.h"
#include "utils/Constants.h"
#include "utils/PolkitHelper.h"

#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QIcon>
#include <QLoggingCategory>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>

Q_LOGGING_CATEGORY(lcNyxApp, "nyx.app")

namespace nyx {

using nyx::models::ServiceConfig;
using nyx::models::ServiceStatus;

NyxApp::NyxApp(QObject *parent)
    : QObject(parent)
    , m_serviceManager(&m_configManager)
{
    m_mainWindow = new nyx::ui::MainWindow(&m_configManager);
    connectMainWindowSignals();

    connect(&m_updateTimer, &QTimer::timeout, this, &NyxApp::updateAllServices);

    qApp->installEventFilter(this);

    initialize();
}

NyxApp::~NyxApp()
{
    cleanup();
    delete m_mainWindow;
}

bool NyxApp::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange) {
        updateTrayIconForTheme();
    }
    return QObject::eventFilter(obj, event);
}

void NyxApp::initialize()
{
    qCInfo(lcNyxApp) << "Initializing NyxApp";

    m_configManager.loadConfig();

    if (m_configManager.getSetting(QStringLiteral("show_main_tray"), true).toBool()) {
        createMainTrayIcon();
    } else {
        qCInfo(lcNyxApp) << "Main tray icon disabled in settings";
    }

    for (const ServiceConfig &service : m_configManager.getEnabledServices()) {
        createServiceTrayIcon(service);
    }

    autoStartServices();

    const int interval = m_configManager.getSetting(QStringLiteral("update_interval"), 5).toInt();
    m_updateTimer.start(interval * 1000);

    qCInfo(lcNyxApp) << "Application initialized with" << m_trayIcons.size() << "services";
}

bool NyxApp::isDarkTheme() const
{
    const QPalette pal = QApplication::palette();
    const QColor bg = pal.color(QPalette::Window);
    const int brightness = (bg.red() * 299 + bg.green() * 587 + bg.blue() * 114) / 1000;
    return brightness < 128;
}

void NyxApp::updateTrayIconForTheme()
{
    if (!m_mainTrayIcon) return;

    const bool dark = isDarkTheme();
    const QString darkPath = nyx::utils::trayIconDarkPath();
    const QString lightPath = nyx::utils::trayIconLightPath();
    const QString symbolicPath = nyx::utils::trayIconPath();

    QIcon icon;
    if (dark && QFile::exists(darkPath)) {
        icon = QIcon(darkPath);
    } else if (!dark && QFile::exists(lightPath)) {
        icon = QIcon(lightPath);
    } else if (QFile::exists(symbolicPath)) {
        icon = QIcon(symbolicPath);
    } else {
        icon = QIcon::fromTheme(nyx::utils::APP_ICON, QIcon::fromTheme(QStringLiteral("applications-system")));
    }
    m_mainTrayIcon->setIcon(icon);
}

void NyxApp::createMainTrayIcon()
{
    m_mainTrayIcon = new QSystemTrayIcon(this);
    updateTrayIconForTheme();

    m_mainTrayIcon->setToolTip(QStringLiteral("%1\nClick to manage services").arg(nyx::utils::APP_NAME));
    connect(m_mainTrayIcon, &QSystemTrayIcon::activated, this, &NyxApp::onMainTrayActivated);

    auto *menu = new QMenu();

    menu->addAction(QStringLiteral("Show Manager"), this, &NyxApp::showMainWindow);

    menu->addAction(QStringLiteral("Settings..."), this, [this]() {
        nyx::ui::SettingsDialog dialog(&m_configManager, m_mainWindow);
        if (dialog.exec()) onSettingsChanged();
    });

    menu->addSeparator();
    menu->addAction(QStringLiteral("Add Service..."), this, &NyxApp::onAddService);
    menu->addSeparator();

    m_passwordlessAction = new QAction(QStringLiteral("Enable Passwordless Mode"), menu);
    m_passwordlessAction->setCheckable(true);
    m_passwordlessAction->setChecked(nyx::utils::PolkitHelper::isPasswordlessEnabled(&m_configManager));
    connect(m_passwordlessAction, &QAction::triggered, this, &NyxApp::togglePasswordlessMode);
    menu->addAction(m_passwordlessAction);

    menu->addSeparator();
    menu->addAction(QStringLiteral("About"), this, &NyxApp::showAbout);
    menu->addSeparator();
    menu->addAction(QStringLiteral("Quit"), this, &NyxApp::onQuit);

    m_mainTrayIcon->setContextMenu(menu);
    m_mainTrayIcon->show();

    qCInfo(lcNyxApp) << "Main tray icon created";
}

void NyxApp::connectMainWindowSignals()
{
    connect(m_mainWindow, &nyx::ui::MainWindow::serviceStartRequested, this, &NyxApp::onStartRequested);
    connect(m_mainWindow, &nyx::ui::MainWindow::serviceStopRequested, this, &NyxApp::onStopRequested);
    connect(m_mainWindow, &nyx::ui::MainWindow::serviceRestartRequested, this, &NyxApp::onRestartRequested);
    connect(m_mainWindow, &nyx::ui::MainWindow::serviceAddRequested, this, &NyxApp::onAddService);
    connect(m_mainWindow, &nyx::ui::MainWindow::serviceEditRequested, this, &NyxApp::onEditRequested);
    connect(m_mainWindow, &nyx::ui::MainWindow::serviceRemoveRequested, this, &NyxApp::onRemoveRequested);
    connect(m_mainWindow, &nyx::ui::MainWindow::serviceLogsRequested, this, &NyxApp::onViewLogsRequested);
    connect(m_mainWindow, &nyx::ui::MainWindow::settingsChanged, this, &NyxApp::onSettingsChanged);
    connect(m_mainWindow, &nyx::ui::MainWindow::exitAppRequested, this, &NyxApp::onExitAppRequested);
}

nyx::ui::ServiceTrayIcon *NyxApp::createServiceTrayIcon(const ServiceConfig &config)
{
    const ServiceKey key(config.name, config.serviceType);
    if (m_trayIcons.contains(key)) {
        qCWarning(lcNyxApp) << "Tray icon for" << config.name << "already exists";
        return m_trayIcons[key];
    }

    auto *trayIcon = new nyx::ui::ServiceTrayIcon(
        config, [this]() { return isDarkTheme(); }, this);

    connect(trayIcon, &nyx::ui::ServiceTrayIcon::startRequested, this, &NyxApp::onStartRequested);
    connect(trayIcon, &nyx::ui::ServiceTrayIcon::stopRequested, this, &NyxApp::onStopRequested);
    connect(trayIcon, &nyx::ui::ServiceTrayIcon::restartRequested, this, &NyxApp::onRestartRequested);
    connect(trayIcon, &nyx::ui::ServiceTrayIcon::viewLogsRequested, this, &NyxApp::onViewLogsRequested);
    connect(trayIcon, &nyx::ui::ServiceTrayIcon::editRequested, this, &NyxApp::onEditRequested);
    connect(trayIcon, &nyx::ui::ServiceTrayIcon::removeRequested, this, &NyxApp::onRemoveRequested);

    m_trayIcons[key] = trayIcon;

    const ServiceStatus status = m_serviceManager.getServiceStatus(config.name, config.isUserService());
    trayIcon->updateStatus(status);

    qCInfo(lcNyxApp) << "Created tray icon for" << config.displayName;
    return trayIcon;
}

void NyxApp::removeServiceTrayIcon(const QString &name, const QString &type)
{
    const ServiceKey key(name, type);
    if (auto *icon = m_trayIcons.take(key)) {
        icon->hide();
        icon->deleteLater();
        qCInfo(lcNyxApp) << "Removed tray icon for" << name;
    }
}

void NyxApp::autoStartServices()
{
    for (const ServiceConfig &service : m_configManager.getServices()) {
        if (service.autoStart) {
            qCInfo(lcNyxApp) << "Auto-starting service:" << service.name;
            auto [ok, err] = m_serviceManager.startService(service.name, service.isUserService());
            if (!ok) {
                qCWarning(lcNyxApp) << "Failed to auto-start" << service.name << ":" << err;
            }
        }
    }
}

void NyxApp::updateAllServices()
{
    const auto keys = m_trayIcons.keys();
    for (const ServiceKey &key : keys) {
        auto *trayIcon = m_trayIcons.value(key);
        if (!trayIcon) continue;

        const ServiceStatus newStatus = m_serviceManager.getServiceStatus(key.first, key.second == QLatin1String("user"));
        const ServiceStatus oldStatus = trayIcon->currentStatus();

        if (oldStatus != newStatus) {
            trayIcon->updateStatus(newStatus);

            if (m_configManager.getSetting(QStringLiteral("show_notifications"), true).toBool()) {
                notifyStatusChange(key.first, key.second,
                                   trayIcon->serviceConfig().displayName,
                                   oldStatus, newStatus);
            }
        }
    }

    if (m_mainWindow->isVisible()) {
        updateMainWindow();
    }
}

void NyxApp::updateMainWindow()
{
    QHash<nyx::ui::MainWindow::ServiceKey, ServiceStatus> statuses;
    for (auto it = m_trayIcons.cbegin(); it != m_trayIcons.cend(); ++it) {
        statuses[it.key()] = it.value()->currentStatus();
    }
    m_mainWindow->updateServices(m_configManager.getServices(), statuses);
}

void NyxApp::notifyStatusChange(const QString &name, const QString & /*type*/,
                                 const QString &displayName,
                                 ServiceStatus oldStatus, ServiceStatus newStatus)
{
    if (newStatus == ServiceStatus::Active && oldStatus != ServiceStatus::Active) {
        m_notificationManager.notifyServiceStarted(name, displayName);
    } else if (newStatus == ServiceStatus::Inactive && oldStatus == ServiceStatus::Active) {
        m_notificationManager.notifyServiceStopped(name, displayName);
    } else if (newStatus == ServiceStatus::Failed) {
        m_notificationManager.notifyServiceFailed(name, displayName);
    }
}

void NyxApp::onStartRequested(const QString &name, const QString &type)
{
    qCInfo(lcNyxApp) << "Starting service:" << name << "(" << type << ")";
    auto [ok, err] = m_serviceManager.startService(name, type == QLatin1String("user"));
    if (!ok) {
        m_notificationManager.notifyError(QStringLiteral("Service Start Failed"),
                                          QStringLiteral("Failed to start %1:\n%2").arg(name, err));
    }
    updateAllServices();
}

void NyxApp::onStopRequested(const QString &name, const QString &type)
{
    qCInfo(lcNyxApp) << "Stopping service:" << name << "(" << type << ")";
    auto [ok, err] = m_serviceManager.stopService(name, type == QLatin1String("user"));
    if (!ok) {
        m_notificationManager.notifyError(QStringLiteral("Service Stop Failed"),
                                          QStringLiteral("Failed to stop %1:\n%2").arg(name, err));
    }
    updateAllServices();
}

void NyxApp::onRestartRequested(const QString &name, const QString &type)
{
    qCInfo(lcNyxApp) << "Restarting service:" << name << "(" << type << ")";
    auto [ok, err] = m_serviceManager.restartService(name, type == QLatin1String("user"));
    if (!ok) {
        m_notificationManager.notifyError(QStringLiteral("Service Restart Failed"),
                                          QStringLiteral("Failed to restart %1:\n%2").arg(name, err));
    }
    updateAllServices();
}

void NyxApp::onAddService()
{
    nyx::ui::AddServiceDialog dialog(m_mainWindow);
    if (dialog.exec()) {
        addService(dialog.getServiceConfig());
    }
}

void NyxApp::onEditRequested(const QString &name, const QString &type)
{
    const ServiceConfig *config = m_configManager.getService(name, type);
    if (!config) {
        qCWarning(lcNyxApp) << "Service config not found for" << name;
        return;
    }

    nyx::ui::EditServiceDialog dialog(*config, m_mainWindow);
    if (dialog.exec()) {
        const ServiceConfig updated = dialog.getServiceConfig();
        m_configManager.updateService(name, type, updated);

        const ServiceKey key(name, type);
        if (auto *icon = m_trayIcons.value(key)) {
            icon->updateConfig(updated);
        }
    }
}

void NyxApp::onRemoveRequested(const QString &name, const QString &type)
{
    auto reply = QMessageBox::question(
        nullptr, QStringLiteral("Remove Service"),
        QStringLiteral("Remove %1 from system tray?\n\nThe service will not be stopped or disabled.").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_configManager.removeService(name, type);
        removeServiceTrayIcon(name, type);
    }
}

void NyxApp::onViewLogsRequested(const QString &name, const QString &type)
{
    const ServiceConfig *config = m_configManager.getService(name, type);
    if (!config) {
        qCWarning(lcNyxApp) << "Service config not found for" << name;
        return;
    }

    nyx::ui::LogViewerDialog dialog(*config, &m_serviceManager, m_mainWindow);
    dialog.exec();
}

void NyxApp::onSettingsChanged()
{
    qCInfo(lcNyxApp) << "Settings changed, applying updates";

    const bool showMainTray = m_configManager.getSetting(QStringLiteral("show_main_tray"), true).toBool();
    if (showMainTray && !m_mainTrayIcon) {
        createMainTrayIcon();
    } else if (!showMainTray && m_mainTrayIcon) {
        m_mainTrayIcon->hide();
        m_mainTrayIcon->deleteLater();
        m_mainTrayIcon = nullptr;
        qCInfo(lcNyxApp) << "Main tray icon hidden";
    }

    if (m_mainTrayIcon && m_passwordlessAction) {
        m_passwordlessAction->setChecked(
            nyx::utils::PolkitHelper::isPasswordlessEnabled(&m_configManager));
    }

    const int interval = m_configManager.getSetting(QStringLiteral("update_interval"), 5).toInt();
    m_updateTimer.setInterval(interval * 1000);
    qCInfo(lcNyxApp) << "Update interval set to" << interval << "seconds";
}

void NyxApp::onExitAppRequested()
{
    qCInfo(lcNyxApp) << "Exit app requested";
    cleanup();
    QApplication::quit();
}

void NyxApp::onQuit()
{
    qCInfo(lcNyxApp) << "Quit requested from main menu";
    cleanup();
    QApplication::quit();
}

void NyxApp::onMainTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        showMainWindow();
    }
}

void NyxApp::showMainWindow()
{
    updateMainWindow();
    m_mainWindow->show();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
    qCInfo(lcNyxApp) << "Main window shown";
}

void NyxApp::showAbout()
{
    QMessageBox::about(
        nullptr, QStringLiteral("About %1").arg(nyx::utils::APP_NAME),
        QStringLiteral(
            "<h3>%1</h3>"
            "<p>Version %2</p>"
            "<p>A system tray application for managing systemd services.</p>"
            "<p>Features:</p>"
            "<ul>"
            "<li>Start/Stop/Restart services from system tray</li>"
            "<li>Real-time status monitoring</li>"
            "<li>View systemd logs</li>"
            "<li>Desktop notifications</li>"
            "</ul>"
            "<p>Built with Qt6 C++. Tested on KDE Plasma and GNOME.</p>"
            "<p>GitHub: <a href=\"https://github.com/optis20052\">optis20052</a></p>"
        ).arg(nyx::utils::APP_NAME, nyx::utils::APP_VERSION));
}

void NyxApp::togglePasswordlessMode(bool checked)
{
    if (checked) {
        const QString username = nyx::utils::PolkitHelper::getCurrentUsername();
        auto reply = QMessageBox::question(
            nullptr, QStringLiteral("Enable Passwordless Mode"),
            QStringLiteral("This will allow user '%1' to manage systemd services without entering a password.\n\n"
                           "A PolicyKit rule will be created. You will need to enter your password ONCE to set this up.\n\n"
                           "Continue?").arg(username),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (reply == QMessageBox::Yes) {
            auto [ok, msg] = nyx::utils::PolkitHelper::enablePasswordlessMode(username, &m_configManager);
            if (ok) {
                QMessageBox::information(nullptr, QStringLiteral("Success"), msg);
                m_passwordlessAction->setChecked(true);
            } else {
                QMessageBox::critical(nullptr, QStringLiteral("Error"), msg);
                m_passwordlessAction->setChecked(false);
            }
        } else {
            m_passwordlessAction->setChecked(false);
        }
    } else {
        auto reply = QMessageBox::question(
            nullptr, QStringLiteral("Disable Passwordless Mode"),
            QStringLiteral("This will remove the PolicyKit rule and you will be asked for your password again.\n\nContinue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            auto [ok, msg] = nyx::utils::PolkitHelper::disablePasswordlessMode(&m_configManager);
            if (ok) {
                QMessageBox::information(nullptr, QStringLiteral("Success"), msg);
                m_passwordlessAction->setChecked(false);
            } else {
                QMessageBox::critical(nullptr, QStringLiteral("Error"), msg);
                m_passwordlessAction->setChecked(true);
            }
        } else {
            m_passwordlessAction->setChecked(true);
        }
    }
}

void NyxApp::addService(const ServiceConfig &config)
{
    if (m_configManager.addService(config)) {
        if (config.enabled) {
            createServiceTrayIcon(config);
        }
        qCInfo(lcNyxApp) << "Added new service:" << config.name;
    } else {
        qCWarning(lcNyxApp) << "Service" << config.name << "already exists";
    }
}

void NyxApp::cleanup()
{
    qCInfo(lcNyxApp) << "Cleaning up application";

    m_updateTimer.stop();

    for (auto *icon : std::as_const(m_trayIcons)) {
        icon->hide();
        icon->deleteLater();
    }
    m_trayIcons.clear();

    if (m_mainTrayIcon) {
        m_mainTrayIcon->hide();
        m_mainTrayIcon->deleteLater();
        m_mainTrayIcon = nullptr;
    }

    qCInfo(lcNyxApp) << "Cleanup complete";
}

} // namespace nyx
