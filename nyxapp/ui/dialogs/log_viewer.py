"""Log viewer dialog for displaying systemd service logs."""

import logging
from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QTextEdit,
    QPushButton, QSpinBox, QLabel, QCheckBox, QFileDialog, QMessageBox
)
from PyQt6.QtGui import QFont, QTextCursor
from PyQt6.QtCore import QTimer

from ...models.service import ServiceConfig
from ...core.service_manager import ServiceManager
from ...utils.constants import DEFAULT_LOG_LINES, MAX_LOG_LINES

logger = logging.getLogger(__name__)


class LogViewerDialog(QDialog):
    """Dialog for viewing systemd service logs."""

    def __init__(self, service_config: ServiceConfig, service_manager: ServiceManager, parent=None):
        """Initialize the log viewer dialog.

        Args:
            service_config: Service configuration
            service_manager: Service manager instance
            parent: Parent widget
        """
        super().__init__(parent)

        self.service_config = service_config
        self.service_manager = service_manager
        self.auto_refresh_enabled = False

        # Set up the UI
        self._init_ui()

        # Load initial logs
        self.load_logs()

        # Set up auto-refresh timer
        self.refresh_timer = QTimer()
        self.refresh_timer.timeout.connect(self.load_logs)

    def _init_ui(self):
        """Initialize the user interface."""
        self.setWindowTitle(f"Logs - {self.service_config.display_name}")
        self.setMinimumSize(800, 600)

        # Main layout
        layout = QVBoxLayout()

        # Controls layout
        controls_layout = QHBoxLayout()

        # Number of lines control
        lines_label = QLabel("Lines:")
        self.lines_spinbox = QSpinBox()
        self.lines_spinbox.setMinimum(10)
        self.lines_spinbox.setMaximum(MAX_LOG_LINES)
        self.lines_spinbox.setValue(DEFAULT_LOG_LINES)
        self.lines_spinbox.setSingleStep(100)

        # Refresh button
        self.refresh_button = QPushButton("Refresh")
        self.refresh_button.clicked.connect(self.load_logs)

        # Auto-refresh checkbox
        self.auto_refresh_checkbox = QCheckBox("Auto-refresh (5s)")
        self.auto_refresh_checkbox.stateChanged.connect(self._toggle_auto_refresh)

        # Export button
        self.export_button = QPushButton("Export...")
        self.export_button.clicked.connect(self.export_logs)

        # Add controls to layout
        controls_layout.addWidget(lines_label)
        controls_layout.addWidget(self.lines_spinbox)
        controls_layout.addStretch()
        controls_layout.addWidget(self.auto_refresh_checkbox)
        controls_layout.addWidget(self.refresh_button)
        controls_layout.addWidget(self.export_button)

        layout.addLayout(controls_layout)

        # Log text area
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)

        # Use monospace font for logs
        font = QFont("Monospace")
        font.setStyleHint(QFont.StyleHint.TypeWriter)
        font.setPointSize(9)
        self.log_text.setFont(font)

        layout.addWidget(self.log_text)

        # Bottom buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()

        close_button = QPushButton("Close")
        close_button.clicked.connect(self.accept)
        button_layout.addWidget(close_button)

        layout.addLayout(button_layout)

        self.setLayout(layout)

    def load_logs(self):
        """Load and display logs."""
        lines = self.lines_spinbox.value()
        is_user_service = self.service_config.is_user_service()

        logger.debug(f"Loading {lines} lines of logs for {self.service_config.name}")

        # Get logs from service manager
        logs = self.service_manager.get_service_logs(
            self.service_config.name,
            lines,
            is_user_service
        )

        # Display logs
        self.log_text.setPlainText(logs)

        # Auto-scroll to bottom
        cursor = self.log_text.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        self.log_text.setTextCursor(cursor)

    def export_logs(self):
        """Export logs to a file."""
        # Get file path from user
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Export Logs",
            f"{self.service_config.name}_logs.txt",
            "Text Files (*.txt);;All Files (*)"
        )

        if not file_path:
            return

        try:
            # Write logs to file
            with open(file_path, 'w') as f:
                f.write(self.log_text.toPlainText())

            QMessageBox.information(
                self,
                "Export Successful",
                f"Logs exported successfully to:\n{file_path}"
            )
            logger.info(f"Exported logs to {file_path}")

        except Exception as e:
            logger.error(f"Failed to export logs: {e}")
            QMessageBox.critical(
                self,
                "Export Failed",
                f"Failed to export logs:\n{str(e)}"
            )

    def _toggle_auto_refresh(self, state):
        """Toggle auto-refresh feature.

        Args:
            state: Checkbox state
        """
        if state:
            # Enable auto-refresh (every 5 seconds)
            self.refresh_timer.start(5000)
            self.auto_refresh_enabled = True
            logger.debug("Auto-refresh enabled")
        else:
            # Disable auto-refresh
            self.refresh_timer.stop()
            self.auto_refresh_enabled = False
            logger.debug("Auto-refresh disabled")

    def closeEvent(self, event):
        """Handle dialog close event.

        Args:
            event: Close event
        """
        # Stop auto-refresh timer
        if self.refresh_timer.isActive():
            self.refresh_timer.stop()

        event.accept()
