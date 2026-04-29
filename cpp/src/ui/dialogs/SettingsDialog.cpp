#include "SettingsDialog.h"
#include "core/ConfigManager.h"
#include "utils/AutostartHelper.h"
#include "utils/PolkitHelper.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcSettingsDialog, "nyx.ui.settingsdialog")

namespace nyx::ui {

SettingsDialog::SettingsDialog(nyx::core::ConfigManager *configManager, QWidget *parent)
    : QDialog(parent)
    , m_configManager(configManager)
{
    initUi();
    loadSettings();
}

void SettingsDialog::initUi()
{
    setWindowTitle(QStringLiteral("Settings"));
    setMinimumWidth(500);

    auto *layout = new QVBoxLayout(this);

    // Appearance
    auto *appearanceGroup = new QGroupBox(QStringLiteral("Appearance"));
    auto *appearanceLayout = new QFormLayout(appearanceGroup);
    m_showMainTrayCheckbox = new QCheckBox(QStringLiteral("Show main tray icon"));
    m_showMainTrayCheckbox->setToolTip(QStringLiteral(
        "When disabled, only service tray icons will be shown.\n"
        "You can still access the manager window from service menus."));
    appearanceLayout->addRow(QString(), m_showMainTrayCheckbox);
    layout->addWidget(appearanceGroup);

    // Notifications
    auto *notifGroup = new QGroupBox(QStringLiteral("Notifications"));
    auto *notifLayout = new QFormLayout(notifGroup);
    m_showNotificationsCheckbox = new QCheckBox(QStringLiteral("Show desktop notifications"));
    m_showNotificationsCheckbox->setToolTip(QStringLiteral("Show notifications when service status changes"));
    notifLayout->addRow(QString(), m_showNotificationsCheckbox);
    layout->addWidget(notifGroup);

    // Updates
    auto *updateGroup = new QGroupBox(QStringLiteral("Updates"));
    auto *updateLayout = new QFormLayout(updateGroup);
    m_updateIntervalSpinbox = new QSpinBox();
    m_updateIntervalSpinbox->setRange(1, 60);
    m_updateIntervalSpinbox->setSuffix(QStringLiteral(" seconds"));
    m_updateIntervalSpinbox->setToolTip(QStringLiteral("How often to check service status"));
    updateLayout->addRow(QStringLiteral("Update interval:"), m_updateIntervalSpinbox);
    layout->addWidget(updateGroup);

    // Startup
    auto *startupGroup = new QGroupBox(QStringLiteral("Startup"));
    auto *startupLayout = new QFormLayout(startupGroup);
    m_autostartCheckbox = new QCheckBox(QStringLiteral("Start automatically on login"));
    m_autostartCheckbox->setToolTip(QStringLiteral("Automatically start NyxApp when you log in to your desktop"));
    startupLayout->addRow(QString(), m_autostartCheckbox);
    m_minimizeToTrayCheckbox = new QCheckBox(QStringLiteral("Start minimized to tray"));
    m_minimizeToTrayCheckbox->setToolTip(QStringLiteral(
        "When enabled, the app runs in the background without showing the main window on startup"));
    startupLayout->addRow(QString(), m_minimizeToTrayCheckbox);
    layout->addWidget(startupGroup);

    // Security
    auto *securityGroup = new QGroupBox(QStringLiteral("Security"));
    auto *securityLayout = new QFormLayout(securityGroup);
    m_passwordlessModeCheckbox = new QCheckBox(QStringLiteral("Enable passwordless mode for system services"));
    m_passwordlessModeCheckbox->setToolTip(QStringLiteral(
        "Allow starting/stopping system services without entering password.\n"
        "Creates a polkit rule for your user account."));
    securityLayout->addRow(QString(), m_passwordlessModeCheckbox);
    layout->addWidget(securityGroup);

    // Help
    auto *helpLabel = new QLabel(QStringLiteral(
        "<i>Note: Some settings may require restarting the application to take effect.</i>"));
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 9pt;"));
    layout->addWidget(helpLabel);

    layout->addSpacing(10);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void SettingsDialog::loadSettings()
{
    m_showMainTrayCheckbox->setChecked(m_configManager->getSetting(QStringLiteral("show_main_tray"), true).toBool());
    m_showNotificationsCheckbox->setChecked(m_configManager->getSetting(QStringLiteral("show_notifications"), true).toBool());
    m_updateIntervalSpinbox->setValue(m_configManager->getSetting(QStringLiteral("update_interval"), 5).toInt());
    m_autostartCheckbox->setChecked(nyx::utils::AutostartHelper::isAutostartEnabled());
    m_minimizeToTrayCheckbox->setChecked(m_configManager->getSetting(QStringLiteral("minimize_to_tray"), false).toBool());
    m_passwordlessModeCheckbox->setChecked(nyx::utils::PolkitHelper::isPasswordlessEnabled(m_configManager));
}

void SettingsDialog::saveSettings()
{
    bool changed = false;

    const bool showMainTray = m_showMainTrayCheckbox->isChecked();
    const bool showNotifications = m_showNotificationsCheckbox->isChecked();
    const int updateInterval = m_updateIntervalSpinbox->value();
    const bool minimizeToTray = m_minimizeToTrayCheckbox->isChecked();
    const bool autostartEnabled = m_autostartCheckbox->isChecked();
    const bool passwordlessMode = m_passwordlessModeCheckbox->isChecked();

    if (m_configManager->getSetting(QStringLiteral("show_main_tray"), true).toBool() != showMainTray) changed = true;
    if (m_configManager->getSetting(QStringLiteral("show_notifications"), true).toBool() != showNotifications) changed = true;
    if (m_configManager->getSetting(QStringLiteral("update_interval"), 5).toInt() != updateInterval) changed = true;
    if (m_configManager->getSetting(QStringLiteral("minimize_to_tray"), false).toBool() != minimizeToTray) changed = true;

    // Handle autostart
    const bool currentAutostart = nyx::utils::AutostartHelper::isAutostartEnabled();
    if (currentAutostart != autostartEnabled) {
        auto [ok, msg] = autostartEnabled
            ? nyx::utils::AutostartHelper::enableAutostart()
            : nyx::utils::AutostartHelper::disableAutostart();
        if (ok) {
            changed = true;
            qCInfo(lcSettingsDialog) << "Autostart" << (autostartEnabled ? "enabled" : "disabled");
        } else {
            QMessageBox::critical(this, QStringLiteral("Error"), msg);
            m_autostartCheckbox->setChecked(currentAutostart);
        }
    }

    // Handle passwordless mode
    const bool currentPasswordless = nyx::utils::PolkitHelper::isPasswordlessEnabled(m_configManager);
    if (currentPasswordless != passwordlessMode) {
        if (passwordlessMode) {
            const QString username = nyx::utils::PolkitHelper::getCurrentUsername();
            auto reply = QMessageBox::question(
                this, QStringLiteral("Enable Passwordless Mode"),
                QStringLiteral("This will create a polkit rule to allow user '%1' to "
                               "manage systemd services without password.\n\n"
                               "Do you want to continue?").arg(username));
            if (reply == QMessageBox::Yes) {
                auto [ok, msg] = nyx::utils::PolkitHelper::enablePasswordlessMode(username, m_configManager);
                if (ok) { changed = true; } else {
                    QMessageBox::critical(this, QStringLiteral("Error"), msg);
                    m_passwordlessModeCheckbox->setChecked(false);
                }
            } else {
                m_passwordlessModeCheckbox->setChecked(false);
            }
        } else {
            auto reply = QMessageBox::question(
                this, QStringLiteral("Disable Passwordless Mode"),
                QStringLiteral("This will remove the polkit rule.\nDo you want to continue?"));
            if (reply == QMessageBox::Yes) {
                auto [ok, msg] = nyx::utils::PolkitHelper::disablePasswordlessMode(m_configManager);
                if (ok) { changed = true; } else {
                    QMessageBox::critical(this, QStringLiteral("Error"), msg);
                    m_passwordlessModeCheckbox->setChecked(true);
                }
            } else {
                m_passwordlessModeCheckbox->setChecked(true);
            }
        }
    }

    m_configManager->setSetting(QStringLiteral("show_main_tray"), showMainTray);
    m_configManager->setSetting(QStringLiteral("show_notifications"), showNotifications);
    m_configManager->setSetting(QStringLiteral("update_interval"), updateInterval);
    m_configManager->setSetting(QStringLiteral("minimize_to_tray"), minimizeToTray);

    if (changed) {
        m_settingsChanged = true;
        qCInfo(lcSettingsDialog) << "Settings saved";
    }
}

void SettingsDialog::onAccept()
{
    saveSettings();
    accept();
}

} // namespace nyx::ui
