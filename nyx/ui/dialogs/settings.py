"""Settings dialog for application configuration."""

import logging
from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QFormLayout,
    QCheckBox, QSpinBox, QPushButton, QDialogButtonBox,
    QLabel, QGroupBox, QMessageBox
)
from PyQt6.QtCore import Qt

logger = logging.getLogger(__name__)


class SettingsDialog(QDialog):
    """Dialog for managing application settings."""

    def __init__(self, config_manager, parent=None):
        """Initialize the settings dialog.

        Args:
            config_manager: ConfigManager instance
            parent: Parent widget
        """
        super().__init__(parent)

        self.config_manager = config_manager
        self.settings_changed = False

        self._init_ui()
        self._load_settings()

    def _init_ui(self):
        """Initialize the user interface."""
        self.setWindowTitle("Settings")
        self.setMinimumWidth(500)

        # Main layout
        layout = QVBoxLayout()

        # Appearance settings
        appearance_group = QGroupBox("Appearance")
        appearance_layout = QFormLayout()

        self.show_main_tray_checkbox = QCheckBox("Show main tray icon")
        self.show_main_tray_checkbox.setToolTip(
            "When disabled, only service tray icons will be shown.\n"
            "You can still access the manager window from service menus."
        )
        appearance_layout.addRow("", self.show_main_tray_checkbox)

        appearance_group.setLayout(appearance_layout)
        layout.addWidget(appearance_group)

        # Notifications settings
        notifications_group = QGroupBox("Notifications")
        notifications_layout = QFormLayout()

        self.show_notifications_checkbox = QCheckBox("Show desktop notifications")
        self.show_notifications_checkbox.setToolTip(
            "Show notifications when service status changes"
        )
        notifications_layout.addRow("", self.show_notifications_checkbox)

        notifications_group.setLayout(notifications_layout)
        layout.addWidget(notifications_group)

        # Update settings
        update_group = QGroupBox("Updates")
        update_layout = QFormLayout()

        self.update_interval_spinbox = QSpinBox()
        self.update_interval_spinbox.setMinimum(1)
        self.update_interval_spinbox.setMaximum(60)
        self.update_interval_spinbox.setSuffix(" seconds")
        self.update_interval_spinbox.setToolTip(
            "How often to check service status"
        )
        update_layout.addRow("Update interval:", self.update_interval_spinbox)

        update_group.setLayout(update_layout)
        layout.addWidget(update_group)

        # Startup settings
        startup_group = QGroupBox("Startup")
        startup_layout = QFormLayout()

        self.autostart_checkbox = QCheckBox("Start automatically on login")
        self.autostart_checkbox.setToolTip(
            "Automatically start Nyx when you log in to your desktop"
        )
        startup_layout.addRow("", self.autostart_checkbox)

        self.minimize_to_tray_checkbox = QCheckBox("Start minimized to tray")
        self.minimize_to_tray_checkbox.setToolTip(
            "When enabled, the app runs in the background without showing the main window on startup"
        )
        startup_layout.addRow("", self.minimize_to_tray_checkbox)

        startup_group.setLayout(startup_layout)
        layout.addWidget(startup_group)

        # Security settings
        security_group = QGroupBox("Security")
        security_layout = QFormLayout()

        self.passwordless_mode_checkbox = QCheckBox("Enable passwordless mode for system services")
        self.passwordless_mode_checkbox.setToolTip(
            "Allow starting/stopping system services without entering password.\n"
            "Creates a polkit rule for your user account.\n"
            "Only affects systemctl commands for managing services."
        )
        security_layout.addRow("", self.passwordless_mode_checkbox)

        security_group.setLayout(security_layout)
        layout.addWidget(security_group)

        # Help text
        help_label = QLabel(
            "<i>Note: Some settings may require restarting the application to take effect.</i>"
        )
        help_label.setWordWrap(True)
        help_label.setStyleSheet("color: gray; font-size: 9pt;")
        layout.addWidget(help_label)

        layout.addSpacing(10)

        # Buttons
        button_box = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Save |
            QDialogButtonBox.StandardButton.Cancel
        )
        button_box.accepted.connect(self._on_accept)
        button_box.rejected.connect(self.reject)

        layout.addWidget(button_box)

        self.setLayout(layout)

    def _load_settings(self):
        """Load current settings from config manager."""
        from ...utils.polkit_helper import PolkitHelper
        from ...utils.autostart_helper import AutostartHelper

        self.show_main_tray_checkbox.setChecked(
            self.config_manager.get_setting("show_main_tray", True)
        )
        self.show_notifications_checkbox.setChecked(
            self.config_manager.get_setting("show_notifications", True)
        )
        self.update_interval_spinbox.setValue(
            self.config_manager.get_setting("update_interval", 5)
        )
        self.autostart_checkbox.setChecked(
            AutostartHelper.is_autostart_enabled()
        )
        self.minimize_to_tray_checkbox.setChecked(
            self.config_manager.get_setting("minimize_to_tray", True)
        )
        self.passwordless_mode_checkbox.setChecked(
            PolkitHelper.is_passwordless_enabled(self.config_manager)
        )

    def _save_settings(self):
        """Save settings to config manager."""
        from ...utils.polkit_helper import PolkitHelper
        from ...utils.autostart_helper import AutostartHelper

        # Get current values
        show_main_tray = self.show_main_tray_checkbox.isChecked()
        show_notifications = self.show_notifications_checkbox.isChecked()
        update_interval = self.update_interval_spinbox.value()
        autostart_enabled = self.autostart_checkbox.isChecked()
        minimize_to_tray = self.minimize_to_tray_checkbox.isChecked()
        passwordless_mode = self.passwordless_mode_checkbox.isChecked()

        # Check if settings changed
        changed = False
        if self.config_manager.get_setting("show_main_tray", True) != show_main_tray:
            changed = True
        if self.config_manager.get_setting("show_notifications", True) != show_notifications:
            changed = True
        if self.config_manager.get_setting("update_interval", 5) != update_interval:
            changed = True
        if self.config_manager.get_setting("minimize_to_tray", True) != minimize_to_tray:
            changed = True

        # Handle autostart change
        current_autostart = AutostartHelper.is_autostart_enabled()
        if current_autostart != autostart_enabled:
            if autostart_enabled:
                success, message = AutostartHelper.enable_autostart()
                if success:
                    changed = True
                    logger.info("Autostart enabled")
                else:
                    QMessageBox.critical(self, "Error", f"Failed to enable autostart:\n{message}")
                    self.autostart_checkbox.setChecked(False)
                    logger.error(f"Failed to enable autostart: {message}")
            else:
                success, message = AutostartHelper.disable_autostart()
                if success:
                    changed = True
                    logger.info("Autostart disabled")
                else:
                    QMessageBox.critical(self, "Error", f"Failed to disable autostart:\n{message}")
                    self.autostart_checkbox.setChecked(True)
                    logger.error(f"Failed to disable autostart: {message}")

        # Handle passwordless mode change
        current_passwordless = PolkitHelper.is_passwordless_enabled(self.config_manager)
        if current_passwordless != passwordless_mode:
            if passwordless_mode:
                # Enable passwordless mode
                username = PolkitHelper.get_current_username()
                reply = QMessageBox.question(
                    self,
                    "Enable Passwordless Mode",
                    f"This will create a polkit rule to allow user '{username}' to "
                    f"manage systemd services without password.\n\n"
                    f"The rule will be created at:\n"
                    f"/etc/polkit-1/rules.d/50-nyx-systemctl.rules\n\n"
                    f"Do you want to continue?"
                )

                if reply == QMessageBox.StandardButton.Yes:
                    success, message = PolkitHelper.enable_passwordless_mode(username, self.config_manager)
                    if success:
                        changed = True
                        logger.info("Passwordless mode enabled")
                    else:
                        QMessageBox.critical(self, "Error", message)
                        self.passwordless_mode_checkbox.setChecked(False)
                        logger.error(f"Failed to enable passwordless mode: {message}")
                else:
                    self.passwordless_mode_checkbox.setChecked(False)
            else:
                # Disable passwordless mode
                reply = QMessageBox.question(
                    self,
                    "Disable Passwordless Mode",
                    "This will remove the polkit rule.\n"
                    "You will need to enter your password again when managing system services.\n\n"
                    "Do you want to continue?"
                )

                if reply == QMessageBox.StandardButton.Yes:
                    success, message = PolkitHelper.disable_passwordless_mode(self.config_manager)
                    if success:
                        changed = True
                        logger.info("Passwordless mode disabled")
                    else:
                        QMessageBox.critical(self, "Error", message)
                        self.passwordless_mode_checkbox.setChecked(True)
                        logger.error(f"Failed to disable passwordless mode: {message}")
                else:
                    self.passwordless_mode_checkbox.setChecked(True)

        # Save settings
        self.config_manager.set_setting("show_main_tray", show_main_tray)
        self.config_manager.set_setting("show_notifications", show_notifications)
        self.config_manager.set_setting("update_interval", update_interval)
        self.config_manager.set_setting("minimize_to_tray", minimize_to_tray)

        if changed:
            self.settings_changed = True
            logger.info("Settings saved")

    def _on_accept(self):
        """Handle OK button click."""
        self._save_settings()
        self.accept()

    def has_changes(self) -> bool:
        """Check if settings were changed.

        Returns:
            True if settings were changed
        """
        return self.settings_changed
