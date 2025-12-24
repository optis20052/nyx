"""Main application coordinator for Systemd Tray."""

import logging
from typing import Dict
from PyQt6.QtCore import QObject, QTimer
from PyQt6.QtWidgets import QMessageBox, QSystemTrayIcon, QMenu
from PyQt6.QtGui import QIcon, QAction, QPalette

from .models.service import ServiceConfig, ServiceStatus
from .core.service_manager import ServiceManager
from .core.config_manager import ConfigManager
from .core.notification_manager import NotificationManager
from .ui.tray_icon import ServiceTrayIcon
from .ui.main_window import MainWindow
from .utils.constants import APP_NAME, APP_ICON, APP_ICON_PATH, TRAY_ICON_PATH, TRAY_ICON_LIGHT_PATH, TRAY_ICON_DARK_PATH
from .utils.polkit_helper import PolkitHelper

logger = logging.getLogger(__name__)


class NyxApp(QObject):
    """Main application coordinator.

    Manages all services, tray icons, and coordinates between components.
    """

    def __init__(self):
        """Initialize the application."""
        super().__init__()

        # Initialize managers
        self.config_manager = ConfigManager()
        self.service_manager = ServiceManager(self.config_manager)
        self.notification_manager = NotificationManager()

        # Tray icons dictionary: (service_name, service_type) -> ServiceTrayIcon
        self.tray_icons: Dict[tuple, ServiceTrayIcon] = {}

        # Main application tray icon (optional)
        self.main_tray_icon = None

        # Main window
        self.main_window = MainWindow(self.config_manager)
        self._connect_main_window_signals()

        # Status update timer
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.update_all_services)

        # Load configuration and initialize
        self._initialize()

    def _initialize(self):
        """Initialize the application by loading config and creating tray icons."""
        logger.info("Initializing Systemd Tray application")

        # Load configuration first
        self.config_manager.load_config()

        # Create main application tray icon if enabled (after config is loaded)
        if self.config_manager.get_setting("show_main_tray", True):
            self._create_main_tray_icon()
        else:
            logger.info("Main tray icon disabled in settings")

        # Create tray icons for enabled services
        for service in self.config_manager.get_enabled_services():
            self._create_tray_icon(service)

        # Auto-start services if configured
        self._auto_start_services()

        # Start status update timer
        update_interval = self.config_manager.get_setting("update_interval", 5)
        self.update_timer.start(update_interval * 1000)  # Convert to milliseconds

        logger.info(f"Application initialized with {len(self.tray_icons)} services")

    def _is_dark_theme(self) -> bool:
        """Detect if the current system theme is dark.

        Returns:
            True if dark theme is detected, False otherwise
        """
        from PyQt6.QtWidgets import QApplication
        palette = QApplication.instance().palette()

        # Check the brightness of the window background color
        window_color = palette.color(QPalette.ColorRole.Window)
        # Calculate perceived brightness using the formula
        brightness = (window_color.red() * 299 + window_color.green() * 587 + window_color.blue() * 114) / 1000

        # If brightness is less than 128, it's a dark theme
        return brightness < 128

    def _update_tray_icon_for_theme(self):
        """Update the tray icon based on the current theme."""
        if not self.main_tray_icon:
            return

        is_dark = self._is_dark_theme()

        # Use light icon for dark theme, dark icon for light theme
        if is_dark and TRAY_ICON_DARK_PATH.exists():
            icon = QIcon(str(TRAY_ICON_DARK_PATH))
            logger.debug("Using light icon for dark theme")
        elif not is_dark and TRAY_ICON_LIGHT_PATH.exists():
            icon = QIcon(str(TRAY_ICON_LIGHT_PATH))
            logger.debug("Using dark icon for light theme")
        elif TRAY_ICON_PATH.exists():
            # Fallback to symbolic icon
            icon = QIcon(str(TRAY_ICON_PATH))
            logger.debug("Using fallback symbolic icon")
        else:
            icon = QIcon.fromTheme(APP_ICON, QIcon.fromTheme("applications-system"))
            logger.debug("Using theme icon")

        self.main_tray_icon.setIcon(icon)

    def _create_main_tray_icon(self):
        """Create the main application tray icon."""
        self.main_tray_icon = QSystemTrayIcon(self)

        # Set icon based on theme
        self._update_tray_icon_for_theme()

        # Listen for palette changes to update icon when theme changes
        from PyQt6.QtWidgets import QApplication
        QApplication.instance().paletteChanged.connect(self._update_tray_icon_for_theme)

        # Set tooltip
        self.main_tray_icon.setToolTip(f"{APP_NAME}\nClick to manage services")

        # Connect left-click to show main window
        self.main_tray_icon.activated.connect(self._on_main_tray_activated)

        # Create context menu
        menu = QMenu()

        # Show manager action
        show_manager_action = QAction("Show Manager", menu)
        show_manager_action.triggered.connect(self._show_main_window)
        menu.addAction(show_manager_action)

        # Settings action
        settings_action = QAction("Settings...", menu)
        settings_action.triggered.connect(self._show_settings)
        menu.addAction(settings_action)

        menu.addSeparator()

        # Add service action
        add_action = QAction("Add Service...", menu)
        add_action.triggered.connect(self._on_add_service)
        menu.addAction(add_action)

        menu.addSeparator()

        # Passwordless mode toggle
        self.passwordless_action = QAction("Enable Passwordless Mode", menu)
        self.passwordless_action.setCheckable(True)
        self.passwordless_action.setChecked(PolkitHelper.is_passwordless_enabled(self.config_manager))
        self.passwordless_action.triggered.connect(self._toggle_passwordless_mode)
        menu.addAction(self.passwordless_action)

        menu.addSeparator()

        # About action
        about_action = QAction("About", menu)
        about_action.triggered.connect(self._show_about)
        menu.addAction(about_action)

        menu.addSeparator()

        # Quit action
        quit_action = QAction("Quit", menu)
        quit_action.triggered.connect(self._on_quit)
        menu.addAction(quit_action)

        self.main_tray_icon.setContextMenu(menu)
        self.main_tray_icon.show()

        logger.info("Main tray icon created")

    def _connect_main_window_signals(self):
        """Connect main window signals to app methods."""
        self.main_window.service_start_requested.connect(self._on_start_requested)
        self.main_window.service_stop_requested.connect(self._on_stop_requested)
        self.main_window.service_restart_requested.connect(self._on_restart_requested)
        self.main_window.service_add_requested.connect(self._on_add_service)
        self.main_window.service_edit_requested.connect(self._on_edit_requested)
        self.main_window.service_remove_requested.connect(self._on_remove_requested)
        self.main_window.service_logs_requested.connect(self._on_view_logs_requested)
        self.main_window.settings_changed.connect(self._on_settings_changed)
        self.main_window.exit_app_requested.connect(self._on_exit_app_requested)

    def _on_main_tray_activated(self, reason):
        """Handle main tray icon activation.

        Args:
            reason: QSystemTrayIcon.ActivationReason
        """
        from PyQt6.QtWidgets import QSystemTrayIcon

        # Only respond to left-click (Trigger) and double-click (DoubleClick)
        if reason in (QSystemTrayIcon.ActivationReason.Trigger,
                     QSystemTrayIcon.ActivationReason.DoubleClick):
            self._show_main_window()
            logger.info(f"Main window shown via tray icon activation: {reason}")

    def _show_main_window(self):
        """Show the main management window."""
        # Update the window with current services
        self._update_main_window()
        self.main_window.show()
        self.main_window.raise_()
        self.main_window.activateWindow()
        logger.info("Main window shown")

    def _update_main_window(self):
        """Update main window with current services and statuses."""
        # Get current statuses
        statuses = {}
        for key, tray_icon in self.tray_icons.items():
            statuses[key] = tray_icon.current_status

        # Update the window
        self.main_window.update_services(self.config_manager.services, statuses)

    def _on_add_service(self):
        """Handle add service action from main tray icon."""
        from .ui.dialogs.add_service import AddServiceDialog

        dialog = AddServiceDialog()
        if dialog.exec():
            service_config = dialog.get_service_config()
            self.add_service(service_config)

    def _show_settings(self):
        """Show settings dialog from tray icon."""
        from .ui.dialogs.settings import SettingsDialog

        dialog = SettingsDialog(self.config_manager, None)
        if dialog.exec():
            # Settings were saved, trigger update
            self._on_settings_changed()

    def _show_about(self):
        """Show about dialog."""
        from . import __version__

        QMessageBox.about(
            None,
            f"About {APP_NAME}",
            f"""<h3>{APP_NAME}</h3>
            <p>Version {__version__}</p>
            <p>A system tray application for managing systemd services.</p>
            <p>Features:</p>
            <ul>
            <li>Start/Stop/Restart services from system tray</li>
            <li>Real-time status monitoring</li>
            <li>View systemd logs</li>
            <li>Desktop notifications</li>
            </ul>
            <p>Built with PyQt6. Tested on KDE Plasma and GNOME.</p>
            <p>GitHub: <a href="https://github.com/optis20052">optis20052</a></p>
            """
        )

    def _toggle_passwordless_mode(self, checked: bool):
        """Toggle passwordless mode for system services.

        Args:
            checked: True to enable, False to disable
        """
        if checked:
            # Enable passwordless mode
            username = PolkitHelper.get_current_username()

            # Show explanation dialog
            reply = QMessageBox.question(
                None,
                "Enable Passwordless Mode",
                f"This will allow user '{username}' to manage systemd services without entering a password.\n\n"
                "A PolicyKit rule will be created. You will need to enter your password ONCE to set this up.\n\n"
                "After that, you won't be asked for password when starting/stopping services.\n\n"
                "Continue?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.Yes
            )

            if reply == QMessageBox.StandardButton.Yes:
                success, message = PolkitHelper.enable_passwordless_mode(username, self.config_manager)

                if success:
                    QMessageBox.information(None, "Success", message)
                    self.passwordless_action.setChecked(True)
                    logger.info("Passwordless mode enabled")
                else:
                    QMessageBox.critical(None, "Error", message)
                    self.passwordless_action.setChecked(False)
                    logger.error(f"Failed to enable passwordless mode: {message}")
            else:
                self.passwordless_action.setChecked(False)
        else:
            # Disable passwordless mode
            reply = QMessageBox.question(
                None,
                "Disable Passwordless Mode",
                "This will remove the PolicyKit rule and you will be asked for your password again when managing services.\n\n"
                "Continue?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No
            )

            if reply == QMessageBox.StandardButton.Yes:
                success, message = PolkitHelper.disable_passwordless_mode(self.config_manager)

                if success:
                    QMessageBox.information(None, "Success", message)
                    self.passwordless_action.setChecked(False)
                    logger.info("Passwordless mode disabled")
                else:
                    QMessageBox.critical(None, "Error", message)
                    self.passwordless_action.setChecked(True)
                    logger.error(f"Failed to disable passwordless mode: {message}")
            else:
                self.passwordless_action.setChecked(True)

    def _on_settings_changed(self):
        """Handle settings changes from settings dialog."""
        logger.info("Settings changed, applying updates")

        # Update main tray icon visibility
        show_main_tray = self.config_manager.get_setting("show_main_tray", True)
        if show_main_tray and not self.main_tray_icon:
            # Create main tray icon if it doesn't exist
            self._create_main_tray_icon()
            logger.info("Main tray icon created")
        elif not show_main_tray and self.main_tray_icon:
            # Hide and remove main tray icon
            self.main_tray_icon.hide()
            self.main_tray_icon.deleteLater()
            self.main_tray_icon = None
            logger.info("Main tray icon hidden")

        # Update passwordless mode checkbox in tray menu
        if self.main_tray_icon and hasattr(self, 'passwordless_action'):
            is_passwordless_enabled = PolkitHelper.is_passwordless_enabled(self.config_manager)
            self.passwordless_action.setChecked(is_passwordless_enabled)
            logger.info(f"Passwordless mode action updated: {is_passwordless_enabled}")

        # Update timer interval
        update_interval = self.config_manager.get_setting("update_interval", 5)
        self.update_timer.setInterval(update_interval * 1000)
        logger.info(f"Update interval set to {update_interval} seconds")

    def _on_exit_app_requested(self):
        """Handle exit app request from main window."""
        logger.info("Exit app requested from main window")
        from PyQt6.QtWidgets import QApplication
        self.cleanup()
        QApplication.quit()

    def _on_quit(self):
        """Handle quit action."""
        from PyQt6.QtWidgets import QApplication

        logger.info("Quit requested from main menu")
        self.cleanup()
        QApplication.quit()

    def _create_tray_icon(self, service_config: ServiceConfig) -> ServiceTrayIcon:
        """Create a tray icon for a service.

        Args:
            service_config: Service configuration

        Returns:
            Created ServiceTrayIcon instance
        """
        key = (service_config.name, service_config.service_type)

        # Check if tray icon already exists
        if key in self.tray_icons:
            logger.warning(f"Tray icon for {service_config.name} already exists")
            return self.tray_icons[key]

        # Create new tray icon with theme detection
        tray_icon = ServiceTrayIcon(service_config, is_dark_theme_callback=self._is_dark_theme)

        # Connect signals
        tray_icon.start_requested.connect(self._on_start_requested)
        tray_icon.stop_requested.connect(self._on_stop_requested)
        tray_icon.restart_requested.connect(self._on_restart_requested)
        tray_icon.view_logs_requested.connect(self._on_view_logs_requested)
        tray_icon.edit_requested.connect(self._on_edit_requested)
        tray_icon.remove_requested.connect(self._on_remove_requested)

        # Store reference
        self.tray_icons[key] = tray_icon

        # Update initial status
        status = self.service_manager.get_service_status(
            service_config.name,
            service_config.is_user_service()
        )
        tray_icon.update_status(status)

        logger.info(f"Created tray icon for {service_config.display_name}")
        return tray_icon

    def _remove_tray_icon(self, service_name: str, service_type: str):
        """Remove a tray icon.

        Args:
            service_name: Service name
            service_type: Service type ('user' or 'system')
        """
        key = (service_name, service_type)

        if key in self.tray_icons:
            tray_icon = self.tray_icons[key]
            tray_icon.hide()
            tray_icon.deleteLater()
            del self.tray_icons[key]
            logger.info(f"Removed tray icon for {service_name}")

    def _auto_start_services(self):
        """Auto-start services that have auto_start enabled."""
        for service in self.config_manager.services:
            if service.auto_start:
                logger.info(f"Auto-starting service: {service.name}")
                success, error = self.service_manager.start_service(
                    service.name,
                    service.is_user_service()
                )
                if not success:
                    logger.error(f"Failed to auto-start {service.name}: {error}")

    def update_all_services(self):
        """Update status for all services (called by timer)."""
        # Use a copy of items to avoid issues if dict is modified during iteration
        for key, tray_icon in list(self.tray_icons.items()):
            try:
                service_name, service_type = key
                is_user = service_type == "user"

                # Get current status
                new_status = self.service_manager.get_service_status(service_name, is_user)

                # Update if changed
                if tray_icon.current_status != new_status:
                    old_status = tray_icon.current_status
                    tray_icon.update_status(new_status)

                    # Send notification if enabled
                    if self.config_manager.get_setting("show_notifications", True):
                        self._notify_status_change(
                            service_name,
                            service_type,
                            tray_icon.service_config.display_name,
                            old_status,
                            new_status
                        )
            except RuntimeError:
                # Tray icon was deleted during update, skip it
                logger.debug(f"Skipping update for deleted tray icon: {key}")
                continue

        # Update main window if visible
        if self.main_window.isVisible():
            self._update_main_window()

    def _notify_status_change(
        self,
        service_name: str,
        service_type: str,
        display_name: str,
        old_status: ServiceStatus,
        new_status: ServiceStatus
    ):
        """Send notification for status change.

        Args:
            service_name: Service name
            service_type: Service type
            display_name: Display name
            old_status: Previous status
            new_status: New status
        """
        # Only notify for significant status changes
        if new_status == ServiceStatus.ACTIVE and old_status != ServiceStatus.ACTIVE:
            self.notification_manager.notify_service_started(service_name, display_name)
        elif new_status == ServiceStatus.INACTIVE and old_status == ServiceStatus.ACTIVE:
            self.notification_manager.notify_service_stopped(service_name, display_name)
        elif new_status == ServiceStatus.FAILED:
            self.notification_manager.notify_service_failed(service_name, display_name)

    def _on_start_requested(self, service_name: str, service_type: str):
        """Handle service start request.

        Args:
            service_name: Service name
            service_type: Service type
        """
        is_user = service_type == "user"
        logger.info(f"Starting service: {service_name} ({service_type})")

        success, error = self.service_manager.start_service(service_name, is_user)

        if not success:
            logger.error(f"Failed to start {service_name}: {error}")
            self.notification_manager.notify_error(
                "Service Start Failed",
                f"Failed to start {service_name}:\n{error}"
            )

        # Trigger immediate update
        self.update_all_services()

    def _on_stop_requested(self, service_name: str, service_type: str):
        """Handle service stop request.

        Args:
            service_name: Service name
            service_type: Service type
        """
        is_user = service_type == "user"
        logger.info(f"Stopping service: {service_name} ({service_type})")

        success, error = self.service_manager.stop_service(service_name, is_user)

        if not success:
            logger.error(f"Failed to stop {service_name}: {error}")
            self.notification_manager.notify_error(
                "Service Stop Failed",
                f"Failed to stop {service_name}:\n{error}"
            )

        # Trigger immediate update
        self.update_all_services()

    def _on_restart_requested(self, service_name: str, service_type: str):
        """Handle service restart request.

        Args:
            service_name: Service name
            service_type: Service type
        """
        is_user = service_type == "user"
        logger.info(f"Restarting service: {service_name} ({service_type})")

        success, error = self.service_manager.restart_service(service_name, is_user)

        if not success:
            logger.error(f"Failed to restart {service_name}: {error}")
            self.notification_manager.notify_error(
                "Service Restart Failed",
                f"Failed to restart {service_name}:\n{error}"
            )

        # Trigger immediate update
        self.update_all_services()

    def _on_view_logs_requested(self, service_name: str, service_type: str):
        """Handle view logs request.

        Args:
            service_name: Service name
            service_type: Service type
        """
        logger.info(f"View logs requested for: {service_name} ({service_type})")

        # Import here to avoid circular dependency
        from .ui.dialogs.log_viewer import LogViewerDialog

        # Get service config
        service_config = self.config_manager.get_service(service_name, service_type)
        if not service_config:
            logger.error(f"Service config not found for {service_name}")
            return

        # Show log viewer dialog
        dialog = LogViewerDialog(service_config, self.service_manager)
        dialog.exec()

    def _on_edit_requested(self, service_name: str, service_type: str):
        """Handle edit service request.

        Args:
            service_name: Service name
            service_type: Service type
        """
        logger.info(f"Edit requested for: {service_name} ({service_type})")

        # Import here to avoid circular dependency
        from .ui.dialogs.edit_service import EditServiceDialog

        # Get service config
        service_config = self.config_manager.get_service(service_name, service_type)
        if not service_config:
            logger.error(f"Service config not found for {service_name}")
            return

        # Show edit dialog
        dialog = EditServiceDialog(service_config)
        if dialog.exec():
            updated_config = dialog.get_service_config()
            self.config_manager.update_service(service_name, service_type, updated_config)

            # Update tray icon
            key = (service_name, service_type)
            if key in self.tray_icons:
                # Remove old and create new with updated config
                self._remove_tray_icon(service_name, service_type)
                self._create_tray_icon(updated_config)

    def _on_remove_requested(self, service_name: str, service_type: str):
        """Handle remove service request.

        Args:
            service_name: Service name
            service_type: Service type
        """
        logger.info(f"Remove requested for: {service_name} ({service_type})")

        # Confirm removal
        reply = QMessageBox.question(
            None,
            "Remove Service",
            f"Remove {service_name} from system tray?\n\nThe service will not be stopped or disabled.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            # Remove from config
            self.config_manager.remove_service(service_name, service_type)

            # Remove tray icon
            self._remove_tray_icon(service_name, service_type)

            logger.info(f"Removed service: {service_name}")

    def add_service(self, service_config: ServiceConfig):
        """Add a new service.

        Args:
            service_config: Service configuration
        """
        # Add to config
        if self.config_manager.add_service(service_config):
            # Create tray icon if enabled
            if service_config.enabled:
                self._create_tray_icon(service_config)
            logger.info(f"Added new service: {service_config.name}")
        else:
            logger.warning(f"Service {service_config.name} already exists")

    def cleanup(self):
        """Clean up resources before exit."""
        logger.info("Cleaning up application")

        # Stop timer first to prevent any updates during cleanup
        if self.update_timer:
            self.update_timer.stop()

        # Remove all service tray icons first
        for tray_icon in list(self.tray_icons.values()):
            try:
                tray_icon.hide()
                tray_icon.deleteLater()
            except RuntimeError:
                # Already deleted
                pass

        self.tray_icons.clear()

        # Remove main tray icon last
        if self.main_tray_icon:
            try:
                self.main_tray_icon.hide()
                self.main_tray_icon.deleteLater()
                self.main_tray_icon = None
            except RuntimeError:
                # Already deleted
                pass

        logger.info("Cleanup complete")
