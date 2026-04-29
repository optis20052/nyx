#pragma once

#include <QDialog>

class QCheckBox;
class QSpinBox;

namespace nyx::core {
class ConfigManager;
}

namespace nyx::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(nyx::core::ConfigManager *configManager, QWidget *parent = nullptr);

    bool hasChanges() const { return m_settingsChanged; }

private slots:
    void onAccept();

private:
    void initUi();
    void loadSettings();
    void saveSettings();

    nyx::core::ConfigManager *m_configManager;
    bool m_settingsChanged = false;

    QCheckBox *m_showMainTrayCheckbox = nullptr;
    QCheckBox *m_showNotificationsCheckbox = nullptr;
    QSpinBox *m_updateIntervalSpinbox = nullptr;
    QCheckBox *m_autostartCheckbox = nullptr;
    QCheckBox *m_minimizeToTrayCheckbox = nullptr;
    QCheckBox *m_passwordlessModeCheckbox = nullptr;
};

} // namespace nyx::ui
