"""System tray icon for individual services."""

import logging
from pathlib import Path
from PyQt6.QtWidgets import QSystemTrayIcon, QMenu
from PyQt6.QtGui import QIcon, QPixmap, QPainter, QColor, QAction
from PyQt6.QtCore import pyqtSignal, QObject, QSize

from ..models.service import ServiceConfig, ServiceStatus
from ..utils.constants import DEFAULT_SERVICE_ICON, STATUS_COLORS

logger = logging.getLogger(__name__)


class ServiceTrayIcon(QSystemTrayIcon):
    """System tray icon for a single service.

    Signals:
        start_requested: Emitted when user requests service start
        stop_requested: Emitted when user requests service stop
        restart_requested: Emitted when user requests service restart
        view_logs_requested: Emitted when user requests to view logs
        edit_requested: Emitted when user requests to edit service
        remove_requested: Emitted when user requests to remove service
    """

    start_requested = pyqtSignal(str, str)  # (service_name, service_type)
    stop_requested = pyqtSignal(str, str)
    restart_requested = pyqtSignal(str, str)
    view_logs_requested = pyqtSignal(str, str)
    edit_requested = pyqtSignal(str, str)
    remove_requested = pyqtSignal(str, str)

    def __init__(self, service_config: ServiceConfig, is_dark_theme_callback=None, parent=None):
        """Initialize the service tray icon.

        Args:
            service_config: Configuration for this service
            is_dark_theme_callback: Optional callback function to detect dark theme
            parent: Parent QObject
        """
        super().__init__(parent)

        self.service_config = service_config
        self.current_status = ServiceStatus.UNKNOWN
        self._is_dark_theme = is_dark_theme_callback

        # Listen for theme changes if callback provided
        if is_dark_theme_callback:
            from PyQt6.QtWidgets import QApplication
            QApplication.instance().paletteChanged.connect(self._on_theme_changed)

        # Create initial icon
        self._update_icon()

        # Create context menu
        self._create_menu()

        # Set initial tooltip
        self._update_tooltip()

        # Connect activated signal (click on tray icon)
        self.activated.connect(self._on_activated)

        # Show the tray icon
        self.show()

        logger.debug(f"Created tray icon for {service_config.display_name}")

    def update_status(self, status: ServiceStatus):
        """Update the service status and refresh the icon.

        Args:
            status: New service status
        """
        if self.current_status != status:
            logger.debug(f"{self.service_config.display_name} status changed: {self.current_status} -> {status}")
            self.current_status = status
            self._update_icon()
            self._update_tooltip()
            self._update_menu_actions()

    def _create_menu(self):
        """Create the context menu for the tray icon."""
        menu = QMenu()

        # Service name header (disabled)
        header_action = QAction(self.service_config.display_name, menu)
        header_action.setEnabled(False)
        menu.addAction(header_action)

        menu.addSeparator()

        # Start action
        self.start_action = QAction("Start", menu)
        self.start_action.triggered.connect(self._on_start)
        menu.addAction(self.start_action)

        # Stop action
        self.stop_action = QAction("Stop", menu)
        self.stop_action.triggered.connect(self._on_stop)
        menu.addAction(self.stop_action)

        # Restart action
        self.restart_action = QAction("Restart", menu)
        self.restart_action.triggered.connect(self._on_restart)
        menu.addAction(self.restart_action)

        menu.addSeparator()

        # View logs action
        logs_action = QAction("View Logs...", menu)
        logs_action.triggered.connect(self._on_view_logs)
        menu.addAction(logs_action)

        menu.addSeparator()

        # Edit service action
        edit_action = QAction("Edit Service...", menu)
        edit_action.triggered.connect(self._on_edit)
        menu.addAction(edit_action)

        # Remove from tray action
        remove_action = QAction("Remove from Tray", menu)
        remove_action.triggered.connect(self._on_remove)
        menu.addAction(remove_action)

        self.setContextMenu(menu)
        self._update_menu_actions()

    def _update_menu_actions(self):
        """Update menu action states based on current status."""
        is_active = self.current_status == ServiceStatus.ACTIVE
        is_transitioning = self.current_status in (ServiceStatus.ACTIVATING, ServiceStatus.DEACTIVATING)

        # Disable all actions during transitions
        if is_transitioning:
            self.start_action.setEnabled(False)
            self.stop_action.setEnabled(False)
            self.restart_action.setEnabled(False)
        else:
            # Start enabled if not active
            self.start_action.setEnabled(not is_active)
            # Stop and Restart enabled if active
            self.stop_action.setEnabled(is_active)
            self.restart_action.setEnabled(is_active)

    def _update_icon(self):
        """Update the tray icon based on current status and theme."""
        # Determine current theme
        is_dark = self._is_dark_theme() if self._is_dark_theme else False

        # Get appropriate icon for theme
        icon_path = self.service_config.get_icon_for_theme(is_dark)

        # Check if it's a file path
        if icon_path and (icon_path.startswith('/') or icon_path.startswith('./')):
            # It's a file path
            if Path(icon_path).exists():
                base_icon = QIcon(icon_path)
            else:
                logger.warning(f"Icon file not found: {icon_path}, using default")
                base_icon = QIcon.fromTheme(DEFAULT_SERVICE_ICON)
        else:
            # It's a theme icon name
            base_icon = QIcon.fromTheme(
                icon_path or DEFAULT_SERVICE_ICON,
                QIcon.fromTheme(DEFAULT_SERVICE_ICON)
            )

        # Get status color
        status_color = STATUS_COLORS.get(
            self.current_status.value,
            STATUS_COLORS["unknown"]
        )

        # Create icon with status overlay
        icon_with_overlay = self._add_status_overlay(base_icon, status_color)
        self.setIcon(icon_with_overlay)

    def _on_theme_changed(self):
        """Handle system theme change."""
        logger.debug(f"Theme changed for {self.service_config.display_name}, updating icon")
        self._update_icon()

    def _add_status_overlay(self, base_icon: QIcon, color: QColor) -> QIcon:
        """Add a colored status overlay to the icon.

        Args:
            base_icon: Base icon
            color: Color for the status indicator

        Returns:
            Icon with status overlay
        """
        # Get pixmap from base icon
        size = QSize(48, 48)
        pixmap = base_icon.pixmap(size)

        # Create painter
        painter = QPainter(pixmap)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        # Draw status indicator (small circle in bottom-right corner)
        indicator_size = 16
        x = size.width() - indicator_size - 2
        y = size.height() - indicator_size - 2

        # Draw white border
        painter.setBrush(QColor(255, 255, 255))
        painter.setPen(QColor(255, 255, 255))
        painter.drawEllipse(x, y, indicator_size, indicator_size)

        # Draw colored indicator
        painter.setBrush(color)
        painter.setPen(color)
        painter.drawEllipse(x + 2, y + 2, indicator_size - 4, indicator_size - 4)

        painter.end()

        return QIcon(pixmap)

    def _update_tooltip(self):
        """Update the tooltip text."""
        status_text = self.current_status.value.capitalize()
        tooltip = f"{self.service_config.display_name}\nStatus: {status_text}"

        # Add service type info
        tooltip += f"\nType: {self.service_config.service_type} service"

        self.setToolTip(tooltip)

    def _on_activated(self, reason):
        """Handle tray icon activation.

        Args:
            reason: Activation reason (click, double-click, etc.)
        """
        # Currently, we just show the context menu on any activation
        # You can customize this for different behaviors
        pass

    def _on_start(self):
        """Handle start action."""
        logger.info(f"Start requested for {self.service_config.name}")
        self.start_requested.emit(self.service_config.name, self.service_config.service_type)

    def _on_stop(self):
        """Handle stop action."""
        logger.info(f"Stop requested for {self.service_config.name}")
        self.stop_requested.emit(self.service_config.name, self.service_config.service_type)

    def _on_restart(self):
        """Handle restart action."""
        logger.info(f"Restart requested for {self.service_config.name}")
        self.restart_requested.emit(self.service_config.name, self.service_config.service_type)

    def _on_view_logs(self):
        """Handle view logs action."""
        logger.info(f"View logs requested for {self.service_config.name}")
        self.view_logs_requested.emit(self.service_config.name, self.service_config.service_type)

    def _on_edit(self):
        """Handle edit action."""
        logger.info(f"Edit requested for {self.service_config.name}")
        self.edit_requested.emit(self.service_config.name, self.service_config.service_type)

    def _on_remove(self):
        """Handle remove action."""
        logger.info(f"Remove requested for {self.service_config.name}")
        self.remove_requested.emit(self.service_config.name, self.service_config.service_type)
