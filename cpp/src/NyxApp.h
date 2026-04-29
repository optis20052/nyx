#pragma once

#include "core/ConfigManager.h"
#include "core/NotificationManager.h"
#include "core/ServiceManager.h"
#include "models/ServiceConfig.h"
#include "models/ServiceStatus.h"

#include <QAction>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QSystemTrayIcon>
#include <QTimer>

namespace nyx::ui {
class MainWindow;
class ServiceTrayIcon;
}

namespace nyx {

class NyxApp : public QObject {
    Q_OBJECT

public:
    explicit NyxApp(QObject *parent = nullptr);
    ~NyxApp() override;

    bool eventFilter(QObject *obj, QEvent *event) override;

    void cleanup();
    void addService(const nyx::models::ServiceConfig &config);

    nyx::core::ConfigManager *configManager() { return &m_configManager; }
    nyx::ui::MainWindow *mainWindow() { return m_mainWindow; }
    nyx::ui::ServiceTrayIcon *mainTrayIcon() { return nullptr; }

    QSystemTrayIcon *mainTray() { return m_mainTrayIcon; }

private slots:
    void updateAllServices();

private:
    using ServiceKey = QPair<QString, QString>;

    void initialize();
    bool isDarkTheme() const;
    void updateTrayIconForTheme();
    void createMainTrayIcon();
    nyx::ui::ServiceTrayIcon *createServiceTrayIcon(const nyx::models::ServiceConfig &config);
    void removeServiceTrayIcon(const QString &name, const QString &type);
    void autoStartServices();
    void connectMainWindowSignals();
    void updateMainWindow();
    void notifyStatusChange(const QString &name, const QString &type,
                            const QString &displayName,
                            nyx::models::ServiceStatus oldStatus,
                            nyx::models::ServiceStatus newStatus);

    // Slots for signals
    void onStartRequested(const QString &name, const QString &type);
    void onStopRequested(const QString &name, const QString &type);
    void onRestartRequested(const QString &name, const QString &type);
    void onAddService();
    void onEditRequested(const QString &name, const QString &type);
    void onRemoveRequested(const QString &name, const QString &type);
    void onViewLogsRequested(const QString &name, const QString &type);
    void onSettingsChanged();
    void onExitAppRequested();
    void onQuit();
    void onMainTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void showMainWindow();
    void showAbout();
    void togglePasswordlessMode(bool checked);

    nyx::core::ConfigManager m_configManager;
    nyx::core::ServiceManager m_serviceManager;
    nyx::core::NotificationManager m_notificationManager;

    QHash<ServiceKey, nyx::ui::ServiceTrayIcon *> m_trayIcons;
    QSystemTrayIcon *m_mainTrayIcon = nullptr;
    nyx::ui::MainWindow *m_mainWindow = nullptr;
    QTimer m_updateTimer;

    QAction *m_passwordlessAction = nullptr;
};

} // namespace nyx
