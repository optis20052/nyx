"""Application constants and configuration."""

from pathlib import Path
from PyQt6.QtGui import QColor

# Application metadata
APP_NAME = "NyxApp"
APP_VERSION = "1.0.0"
ORGANIZATION_NAME = "NyxApp"

# Paths
CONFIG_DIR = Path.home() / ".config" / "nyxapp"
CONFIG_FILE = CONFIG_DIR / "config.yaml"
LOG_FILE = CONFIG_DIR / "app.log"
ICONS_DIR = CONFIG_DIR / "icons"

# Application resources
RESOURCES_DIR = Path(__file__).parent.parent / "resources"
APP_ICON_PATH = RESOURCES_DIR / "icons" / "nyxapp.svg"  # Colorful icon for app window
TRAY_ICON_PATH = RESOURCES_DIR / "icons" / "nyxapp-symbolic.svg"  # Simple icon for tray (fallback)
TRAY_ICON_LIGHT_PATH = RESOURCES_DIR / "icons" / "nyxapp-light.svg"  # Dark icon for light theme
TRAY_ICON_DARK_PATH = RESOURCES_DIR / "icons" / "nyxapp-dark.svg"  # Light icon for dark theme

# Default settings
DEFAULT_UPDATE_INTERVAL = 5  # seconds
DEFAULT_LOG_LINES = 100
MAX_LOG_LINES = 10000

# Status colors for visual indicators
STATUS_COLORS = {
    "active": QColor(46, 204, 113),      # Green
    "inactive": QColor(149, 165, 166),   # Gray
    "failed": QColor(231, 76, 60),       # Red
    "activating": QColor(241, 196, 15),  # Yellow
    "deactivating": QColor(241, 196, 15), # Yellow
    "unknown": QColor(149, 165, 166)     # Gray
}

# Icon names (freedesktop icon theme standard)
DEFAULT_SERVICE_ICON = "application-x-executable"
APP_ICON = "preferences-system"  # Fallback for theme icon

# Notification settings
NOTIFICATION_TIMEOUT = 5000  # milliseconds
NOTIFICATION_RATE_LIMIT = 30  # seconds
