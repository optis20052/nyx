"""Main window for managing systemd services."""

import logging
from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QTableWidget, QTableWidgetItem,
    QHeaderView, QMessageBox, QMenu, QLabel
)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QIcon, QAction

from ..models.service import ServiceStatus

logger = logging.getLogger(__name__)


class MainWindow(QMainWindow):
    """Main window for managing systemd services."""

    # Signals
    service_start_requested = pyqtSignal(str, str)  # service_name, service_type
    service_stop_requested = pyqtSignal(str, str)
    service_restart_requested = pyqtSignal(str, str)
    service_add_requested = pyqtSignal()
    service_edit_requested = pyqtSignal(str, str)
    service_remove_requested = pyqtSignal(str, str)
    service_logs_requested = pyqtSignal(str, str)
    settings_changed = pyqtSignal()
    exit_app_requested = pyqtSignal()  # Request to completely exit the application

    def __init__(self, config_manager=None, parent=None):
        """Initialize the main window.

        Args:
            config_manager: ConfigManager instance
            parent: Parent widget
        """
        super().__init__(parent)

        self.config_manager = config_manager
        self.services = {}  # {(name, type): service_config}

        self._init_ui()

    def _init_ui(self):
        """Initialize the user interface."""
        from ..utils.constants import APP_NAME

        self.setWindowTitle(f"{APP_NAME} - Service Manager")
        self.setMinimumSize(800, 600)

        # Set the window class name to match the app name
        # This prevents KDE from showing "python3" as the app name
        self.setWindowFilePath("")  # Clear any file path
        if hasattr(Qt, 'AA_UseDesktopOpenGL'):
            self.setAttribute(Qt.ApplicationAttribute.AA_UseDesktopOpenGL)

        # Central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        # Main layout
        layout = QVBoxLayout(central_widget)

        # Header
        header_layout = QHBoxLayout()
        header_label = QLabel("<h2>Service Manager</h2>")
        header_layout.addWidget(header_label)
        header_layout.addStretch()

        # Add service button
        add_btn = QPushButton("Add Service")
        add_btn.clicked.connect(self.service_add_requested.emit)
        header_layout.addWidget(add_btn)

        layout.addLayout(header_layout)

        # Services table
        self.services_table = QTableWidget()
        self.services_table.setColumnCount(6)
        self.services_table.setHorizontalHeaderLabels([
            "Service", "Display Name", "Type", "Status", "Auto-Start", "Actions"
        ])

        # Configure table
        header = self.services_table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(4, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(5, QHeaderView.ResizeMode.ResizeToContents)

        self.services_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.services_table.setSelectionMode(QTableWidget.SelectionMode.SingleSelection)
        self.services_table.setAlternatingRowColors(True)

        # Loading indicator overlay
        from PyQt6.QtWidgets import QStackedWidget
        self.table_stack = QStackedWidget()

        # Loading widget
        loading_widget = QWidget()
        loading_layout = QVBoxLayout(loading_widget)
        loading_layout.addStretch()

        loading_label = QLabel("Loading services...")
        loading_label.setStyleSheet("font-size: 14pt; color: gray;")
        loading_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        loading_layout.addWidget(loading_label)

        loading_sublabel = QLabel("Please wait while service statuses are being retrieved")
        loading_sublabel.setStyleSheet("font-size: 10pt; color: gray;")
        loading_sublabel.setAlignment(Qt.AlignmentFlag.AlignCenter)
        loading_layout.addWidget(loading_sublabel)

        loading_layout.addStretch()

        # Empty state widget
        empty_widget = QWidget()
        empty_layout = QVBoxLayout(empty_widget)
        empty_layout.addStretch()

        empty_label = QLabel("No Services Configured")
        empty_label.setStyleSheet("font-size: 14pt; color: gray;")
        empty_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        empty_layout.addWidget(empty_label)

        empty_sublabel = QLabel("Click 'Add Service' to add your first service")
        empty_sublabel.setStyleSheet("font-size: 10pt; color: gray;")
        empty_sublabel.setAlignment(Qt.AlignmentFlag.AlignCenter)
        empty_layout.addWidget(empty_sublabel)

        empty_layout.addStretch()

        # Add all widgets to stack
        self.table_stack.addWidget(loading_widget)  # Index 0
        self.table_stack.addWidget(self.services_table)  # Index 1
        self.table_stack.addWidget(empty_widget)  # Index 2

        # Show loading state initially
        self.table_stack.setCurrentIndex(0)

        layout.addWidget(self.table_stack)

        # Bottom buttons
        bottom_layout = QHBoxLayout()
        # Exit App button (red, on the left)
        exit_app_btn = QPushButton("Exit App")
        exit_app_btn.setStyleSheet("""
            QPushButton {
                background-color: #e74c3c;
                color: white;
                font-weight: bold;
                padding: 5px 15px;
                border-radius: 3px;
            }
            QPushButton:hover {
                background-color: #c0392b;
            }
            QPushButton:pressed {
                background-color: #a93226;
            }
        """)
        exit_app_btn.setToolTip("Completely exit the application (closes all tray icons)")
        exit_app_btn.clicked.connect(self.exit_app_requested.emit)
        bottom_layout.addWidget(exit_app_btn)

        bottom_layout.addStretch()

        settings_btn = QPushButton("Settings")
        settings_btn.clicked.connect(self._show_settings)
        bottom_layout.addWidget(settings_btn)

        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.close)
        bottom_layout.addWidget(close_btn)

        layout.addLayout(bottom_layout)

        logger.info("Main window initialized")

    def update_services(self, services, statuses):
        """Update the services table.

        Args:
            services: List of ServiceConfig objects
            statuses: Dictionary mapping (name, type) to ServiceStatus
        """
        # Check if there are any services
        if not services:
            # Show empty state
            self.table_stack.setCurrentIndex(2)
            return

        # Switch to table view (hide loading indicator)
        self.table_stack.setCurrentIndex(1)

        self.services_table.setRowCount(len(services))

        for row, service in enumerate(services):
            key = (service.name, service.service_type)

            # Service name
            name_item = QTableWidgetItem(service.name)
            name_item.setFlags(name_item.flags() & ~Qt.ItemFlag.ItemIsEditable)
            self.services_table.setItem(row, 0, name_item)

            # Display name
            display_item = QTableWidgetItem(service.display_name)
            display_item.setFlags(display_item.flags() & ~Qt.ItemFlag.ItemIsEditable)
            self.services_table.setItem(row, 1, display_item)

            # Type
            type_item = QTableWidgetItem(service.service_type)
            type_item.setFlags(type_item.flags() & ~Qt.ItemFlag.ItemIsEditable)
            self.services_table.setItem(row, 2, type_item)

            # Status
            status = statuses.get(key, ServiceStatus.UNKNOWN)
            status_item = QTableWidgetItem(status.value.title())
            status_item.setFlags(status_item.flags() & ~Qt.ItemFlag.ItemIsEditable)

            # Color code status
            if status == ServiceStatus.ACTIVE:
                status_item.setForeground(Qt.GlobalColor.darkGreen)
            elif status == ServiceStatus.FAILED:
                status_item.setForeground(Qt.GlobalColor.red)
            elif status == ServiceStatus.INACTIVE:
                status_item.setForeground(Qt.GlobalColor.gray)

            self.services_table.setItem(row, 3, status_item)

            # Auto-start
            auto_start_item = QTableWidgetItem("Yes" if service.auto_start else "No")
            auto_start_item.setFlags(auto_start_item.flags() & ~Qt.ItemFlag.ItemIsEditable)
            self.services_table.setItem(row, 4, auto_start_item)

            # Actions
            actions_widget = self._create_actions_widget(service, status)
            self.services_table.setCellWidget(row, 5, actions_widget)

    def _create_actions_widget(self, service, status):
        """Create actions widget for a service row.

        Args:
            service: ServiceConfig object
            status: Current ServiceStatus

        Returns:
            QWidget with action buttons
        """
        widget = QWidget()
        layout = QHBoxLayout(widget)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(4)

        # Start button
        start_btn = QPushButton("Start")
        start_btn.setMaximumWidth(60)
        start_btn.setEnabled(status != ServiceStatus.ACTIVE)
        start_btn.clicked.connect(
            lambda: self.service_start_requested.emit(service.name, service.service_type)
        )
        layout.addWidget(start_btn)

        # Stop button
        stop_btn = QPushButton("Stop")
        stop_btn.setMaximumWidth(60)
        stop_btn.setEnabled(status == ServiceStatus.ACTIVE)
        stop_btn.clicked.connect(
            lambda: self.service_stop_requested.emit(service.name, service.service_type)
        )
        layout.addWidget(stop_btn)

        # More menu button
        more_btn = QPushButton("⋮")
        more_btn.setMaximumWidth(30)
        more_btn.clicked.connect(
            lambda: self._show_service_menu(service, more_btn)
        )
        layout.addWidget(more_btn)

        return widget

    def _show_service_menu(self, service, button):
        """Show context menu for a service.

        Args:
            service: ServiceConfig object
            button: Button to position menu near
        """
        menu = QMenu(self)

        # Restart action
        restart_action = QAction("Restart", menu)
        restart_action.triggered.connect(
            lambda: self.service_restart_requested.emit(service.name, service.service_type)
        )
        menu.addAction(restart_action)

        # Logs action
        logs_action = QAction("View Logs", menu)
        logs_action.triggered.connect(
            lambda: self.service_logs_requested.emit(service.name, service.service_type)
        )
        menu.addAction(logs_action)

        menu.addSeparator()

        # Edit action
        edit_action = QAction("Edit", menu)
        edit_action.triggered.connect(
            lambda: self.service_edit_requested.emit(service.name, service.service_type)
        )
        menu.addAction(edit_action)

        # Remove action
        remove_action = QAction("Remove", menu)
        remove_action.triggered.connect(
            lambda: self.service_remove_requested.emit(service.name, service.service_type)
        )
        menu.addAction(remove_action)

        # Show menu
        menu.exec(button.mapToGlobal(button.rect().bottomLeft()))

    def _show_settings(self):
        """Show settings dialog."""
        if not self.config_manager:
            QMessageBox.warning(
                self,
                "Settings",
                "Settings are not available at this time."
            )
            return

        from .dialogs.settings import SettingsDialog

        dialog = SettingsDialog(self.config_manager, self)
        if dialog.exec():
            if dialog.has_changes():
                # Emit signal to notify app of settings changes
                self.settings_changed.emit()
                QMessageBox.information(
                    self,
                    "Settings Saved",
                    "Settings have been saved.\n\n"
                    "Some changes may require restarting the application."
                )

    def closeEvent(self, event):
        """Handle window close event.

        Args:
            event: Close event
        """
        # Just hide the window instead of closing
        event.ignore()
        self.hide()
        logger.info("Main window hidden")
