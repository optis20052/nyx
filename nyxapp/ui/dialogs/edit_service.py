"""Edit service dialog for modifying existing service configurations."""

import logging
import shutil
from pathlib import Path
from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QFormLayout, QHBoxLayout,
    QLineEdit, QComboBox, QCheckBox, QPushButton,
    QDialogButtonBox, QMessageBox, QLabel, QFileDialog
)
from PyQt6.QtCore import Qt

from ...models.service import ServiceConfig
from ...utils.constants import ICONS_DIR

logger = logging.getLogger(__name__)


class EditServiceDialog(QDialog):
    """Dialog for editing an existing service configuration."""

    def __init__(self, service_config: ServiceConfig, parent=None):
        """Initialize the edit service dialog.

        Args:
            service_config: Current service configuration
            parent: Parent widget
        """
        super().__init__(parent)

        self.original_config = service_config
        self.updated_config = None

        # Set up the UI
        self._init_ui()

        # Populate with current values
        self._populate_fields()

    def _init_ui(self):
        """Initialize the user interface."""
        self.setWindowTitle(f"Edit Service - {self.original_config.display_name}")
        self.setMinimumWidth(450)

        # Main layout
        layout = QVBoxLayout()

        # Form layout
        form_layout = QFormLayout()

        # Service name (read-only)
        self.name_input = QLineEdit()
        self.name_input.setReadOnly(True)
        form_layout.addRow("Service Name:", self.name_input)

        # Display name
        self.display_name_input = QLineEdit()
        form_layout.addRow("Display Name*:", self.display_name_input)

        # Icon selection - side by side layout
        icons_group_layout = QVBoxLayout()
        icons_label = QLabel("Service Icons:")
        icons_label.setStyleSheet("font-weight: bold;")
        icons_group_layout.addWidget(icons_label)

        # Container for both pickers
        icons_container = QHBoxLayout()

        # Light theme icon (left side)
        light_icon_layout = QVBoxLayout()
        light_label = QLabel("Light Theme Icon\n(use dark-colored icon)")
        light_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        light_icon_layout.addWidget(light_label)

        self.light_icon_preview = QLabel()
        self.light_icon_preview.setFixedSize(64, 64)
        self.light_icon_preview.setStyleSheet("background-color: #F5F5F5; border: 1px solid #ccc; border-radius: 4px;")
        self.light_icon_preview.setScaledContents(True)
        light_icon_layout.addWidget(self.light_icon_preview, alignment=Qt.AlignmentFlag.AlignCenter)

        self.light_icon_input = QLineEdit()
        self.light_icon_input.setPlaceholderText("Icon name or path")
        self.light_icon_input.textChanged.connect(lambda: self._update_icon_preview('light'))
        light_icon_layout.addWidget(self.light_icon_input)

        light_browse_btn = QPushButton("Browse...")
        light_browse_btn.clicked.connect(lambda: self._browse_icon('light'))
        light_icon_layout.addWidget(light_browse_btn)

        icons_container.addLayout(light_icon_layout)

        # Dark theme icon (right side)
        dark_icon_layout = QVBoxLayout()
        dark_label = QLabel("Dark Theme Icon\n(use light-colored icon)")
        dark_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        dark_icon_layout.addWidget(dark_label)

        self.dark_icon_preview = QLabel()
        self.dark_icon_preview.setFixedSize(64, 64)
        self.dark_icon_preview.setStyleSheet("background-color: #2D2D2D; border: 1px solid #555; border-radius: 4px;")
        self.dark_icon_preview.setScaledContents(True)
        dark_icon_layout.addWidget(self.dark_icon_preview, alignment=Qt.AlignmentFlag.AlignCenter)

        self.dark_icon_input = QLineEdit()
        self.dark_icon_input.setPlaceholderText("Icon name or path")
        self.dark_icon_input.textChanged.connect(lambda: self._update_icon_preview('dark'))
        dark_icon_layout.addWidget(self.dark_icon_input)

        dark_browse_btn = QPushButton("Browse...")
        dark_browse_btn.clicked.connect(lambda: self._browse_icon('dark'))
        dark_icon_layout.addWidget(dark_browse_btn)

        icons_container.addLayout(dark_icon_layout)

        icons_group_layout.addLayout(icons_container)
        form_layout.addRow("", icons_group_layout)

        # Store icon paths
        self._light_icon_path = None
        self._dark_icon_path = None

        # Service type (read-only)
        self.service_type_input = QLineEdit()
        self.service_type_input.setReadOnly(True)
        form_layout.addRow("Service Type:", self.service_type_input)

        # Auto-start checkbox
        self.auto_start_checkbox = QCheckBox("Start service when app launches")
        form_layout.addRow("", self.auto_start_checkbox)

        # Enabled checkbox
        self.enabled_checkbox = QCheckBox("Show tray icon")
        form_layout.addRow("", self.enabled_checkbox)

        layout.addLayout(form_layout)

        layout.addSpacing(20)

        # Buttons
        button_box = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok |
            QDialogButtonBox.StandardButton.Cancel
        )
        button_box.accepted.connect(self._on_accept)
        button_box.rejected.connect(self.reject)

        layout.addWidget(button_box)

        self.setLayout(layout)

    def _populate_fields(self):
        """Populate form fields with current service configuration."""
        self.name_input.setText(self.original_config.name)
        self.display_name_input.setText(self.original_config.display_name)

        # Handle light theme icon
        if self.original_config.icon_light:
            icon_light = self.original_config.icon_light
            if icon_light.startswith('/') or icon_light.startswith('./'):
                # It's a file path - show just the filename
                self._light_icon_path = icon_light
                self.light_icon_input.setText(Path(icon_light).name)
            else:
                # It's a theme name
                self._light_icon_path = None
                self.light_icon_input.setText(icon_light)
        else:
            # Fall back to base icon
            icon = self.original_config.icon
            if icon.startswith('/') or icon.startswith('./'):
                self._light_icon_path = icon
                self.light_icon_input.setText(Path(icon).name)
            else:
                self._light_icon_path = None
                self.light_icon_input.setText(icon)

        # Handle dark theme icon
        if self.original_config.icon_dark:
            icon_dark = self.original_config.icon_dark
            if icon_dark.startswith('/') or icon_dark.startswith('./'):
                # It's a file path - show just the filename
                self._dark_icon_path = icon_dark
                self.dark_icon_input.setText(Path(icon_dark).name)
            else:
                # It's a theme name
                self._dark_icon_path = None
                self.dark_icon_input.setText(icon_dark)
        else:
            # Fall back to base icon
            icon = self.original_config.icon
            if icon.startswith('/') or icon.startswith('./'):
                self._dark_icon_path = icon
                self.dark_icon_input.setText(Path(icon).name)
            else:
                self._dark_icon_path = None
                self.dark_icon_input.setText(icon)

        self.service_type_input.setText(self.original_config.service_type)
        self.auto_start_checkbox.setChecked(self.original_config.auto_start)
        self.enabled_checkbox.setChecked(self.original_config.enabled)

        # Update previews after populating
        self._update_icon_preview('light')
        self._update_icon_preview('dark')

    def _update_icon_preview(self, icon_type):
        """Update the icon preview based on current input.

        Args:
            icon_type: Either 'light' or 'dark'
        """
        from PyQt6.QtGui import QIcon, QPixmap

        if icon_type == 'light':
            icon_path = self._light_icon_path
            icon_input = self.light_icon_input
            preview_label = self.light_icon_preview
        else:  # dark
            icon_path = self._dark_icon_path
            icon_input = self.dark_icon_input
            preview_label = self.dark_icon_preview

        # Use the stored path if available (from browsing), otherwise use the input text
        if icon_path and Path(icon_path).exists():
            icon = QIcon(icon_path)
        else:
            icon_value = icon_input.text().strip()
            if not icon_value:
                icon_value = "application-x-executable"
            # It's a theme icon name
            icon = QIcon.fromTheme(icon_value, QIcon.fromTheme("application-x-executable"))

        # Set the preview
        pixmap = icon.pixmap(64, 64)
        preview_label.setPixmap(pixmap)

    def _browse_icon(self, icon_type):
        """Open file dialog to browse for an icon file.

        Args:
            icon_type: Either 'light' or 'dark'
        """
        title = f"Select {icon_type.title()} Theme Icon"
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            title,
            "",
            "Image Files (*.png *.svg *.ico *.xpm);;All Files (*)"
        )

        if file_path:
            try:
                # Copy the icon file to the icons directory
                source_path = Path(file_path)

                # Generate a unique filename based on timestamp to avoid conflicts
                import time
                timestamp = int(time.time() * 1000)
                dest_filename = f"icon_{icon_type}_{timestamp}{source_path.suffix}"
                dest_path = ICONS_DIR / dest_filename

                # Copy the file
                shutil.copy2(source_path, dest_path)

                # Store the full path internally
                if icon_type == 'light':
                    self._light_icon_path = str(dest_path)
                    self.light_icon_input.setText(source_path.name)
                else:  # dark
                    self._dark_icon_path = str(dest_path)
                    self.dark_icon_input.setText(source_path.name)

                # Update preview
                self._update_icon_preview(icon_type)

                logger.info(f"Copied {icon_type} icon from {file_path} to {dest_path}")

            except Exception as e:
                logger.error(f"Failed to copy icon file: {e}")
                QMessageBox.warning(
                    self,
                    "Icon Copy Failed",
                    f"Could not copy icon file:\n{str(e)}\n\nUsing original path."
                )
                # Fall back to original path if copy fails
                if icon_type == 'light':
                    self._light_icon_path = file_path
                    self.light_icon_input.setText(Path(file_path).name)
                else:  # dark
                    self._dark_icon_path = file_path
                    self.dark_icon_input.setText(Path(file_path).name)
                self._update_icon_preview(icon_type)

    def _on_accept(self):
        """Handle OK button click."""
        # Validate input
        display_name = self.display_name_input.text().strip()

        if not display_name:
            QMessageBox.warning(
                self,
                "Invalid Input",
                "Display name is required."
            )
            return

        # Get light theme icon - use stored path if available, otherwise use input text
        if self._light_icon_path:
            icon_light = self._light_icon_path
        else:
            icon_light = self.light_icon_input.text().strip()
            if not icon_light:
                icon_light = "application-x-executable"

        # Get dark theme icon - use stored path if available, otherwise use input text
        if self._dark_icon_path:
            icon_dark = self._dark_icon_path
        else:
            icon_dark = self.dark_icon_input.text().strip()
            if not icon_dark:
                icon_dark = "application-x-executable"

        # Use first icon as base icon (for backward compatibility)
        icon = icon_light

        # Get checkboxes
        auto_start = self.auto_start_checkbox.isChecked()
        enabled = self.enabled_checkbox.isChecked()

        # Create updated service config
        try:
            self.updated_config = ServiceConfig(
                name=self.original_config.name,
                display_name=display_name,
                icon=icon,
                icon_light=icon_light if icon_light != icon else None,
                icon_dark=icon_dark if icon_dark != icon else None,
                service_type=self.original_config.service_type,
                auto_start=auto_start,
                enabled=enabled
            )

            logger.info(f"Updated service config for {self.original_config.name}")
            self.accept()

        except ValueError as e:
            QMessageBox.critical(
                self,
                "Invalid Configuration",
                f"Failed to update service configuration:\n{str(e)}"
            )
            logger.error(f"Failed to update service config: {e}")

    def get_service_config(self) -> ServiceConfig:
        """Get the updated service configuration.

        Returns:
            Updated ServiceConfig instance
        """
        return self.updated_config
