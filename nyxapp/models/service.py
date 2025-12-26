"""Data models for systemd service management."""

from dataclasses import dataclass, field
from enum import Enum
from typing import Optional


class ServiceStatus(Enum):
    """Enumeration of possible service states."""

    ACTIVE = "active"
    INACTIVE = "inactive"
    FAILED = "failed"
    ACTIVATING = "activating"
    DEACTIVATING = "deactivating"
    UNKNOWN = "unknown"

    @classmethod
    def from_string(cls, status_str: str) -> 'ServiceStatus':
        """Convert a string to ServiceStatus enum.

        Args:
            status_str: Status string from systemctl

        Returns:
            ServiceStatus enum value
        """
        try:
            return cls(status_str.lower())
        except ValueError:
            return cls.UNKNOWN


@dataclass
class ServiceConfig:
    """Configuration for a systemd service.

    Attributes:
        name: Systemd unit name (e.g., 'postgresql', 'docker')
        display_name: User-friendly display name
        icon: Icon name (freedesktop) or path to custom icon
        service_type: Either 'user' or 'system'
        auto_start: Whether to start service when app launches
        enabled: Whether to show tray icon for this service
    """

    name: str
    display_name: str
    icon: str = "application-x-executable"
    icon_light: Optional[str] = None  # Dark-colored icon for light themes
    icon_dark: Optional[str] = None   # Light-colored icon for dark themes
    service_type: str = "user"  # 'user' or 'system'
    auto_start: bool = False
    enabled: bool = True

    def __post_init__(self):
        """Validate service configuration after initialization."""
        if not self.name:
            raise ValueError("Service name cannot be empty")

        if self.service_type not in ("user", "system"):
            raise ValueError(f"Invalid service_type: {self.service_type}. Must be 'user' or 'system'")

        if not self.display_name:
            self.display_name = self.name.capitalize()

    def is_user_service(self) -> bool:
        """Check if this is a user service.

        Returns:
            True if user service, False if system service
        """
        return self.service_type == "user"

    def get_icon_for_theme(self, is_dark_theme: bool) -> str:
        """Get the appropriate icon for the current theme.

        Args:
            is_dark_theme: True if dark theme, False if light theme

        Returns:
            Icon path or theme name for the current theme
        """
        if is_dark_theme:
            # For dark theme, use icon_dark (light colored icon)
            return self.icon_dark or self.icon
        else:
            # For light theme, use icon_light (dark colored icon)
            return self.icon_light or self.icon

    def to_dict(self) -> dict:
        """Convert to dictionary for serialization.

        Returns:
            Dictionary representation of the service config
        """
        result = {
            "name": self.name,
            "display_name": self.display_name,
            "icon": self.icon,
            "service_type": self.service_type,
            "auto_start": self.auto_start,
            "enabled": self.enabled
        }

        # Only include theme icons if they're different from base icon
        if self.icon_light and self.icon_light != self.icon:
            result["icon_light"] = self.icon_light
        if self.icon_dark and self.icon_dark != self.icon:
            result["icon_dark"] = self.icon_dark

        return result

    @classmethod
    def from_dict(cls, data: dict) -> 'ServiceConfig':
        """Create ServiceConfig from dictionary.

        Args:
            data: Dictionary with service configuration

        Returns:
            ServiceConfig instance
        """
        return cls(
            name=data["name"],
            display_name=data.get("display_name", data["name"].capitalize()),
            icon=data.get("icon", "application-x-executable"),
            icon_light=data.get("icon_light"),  # May be None
            icon_dark=data.get("icon_dark"),    # May be None
            service_type=data.get("service_type", "user"),
            auto_start=data.get("auto_start", False),
            enabled=data.get("enabled", True)
        )


@dataclass
class ServiceInfo:
    """Runtime information about a service.

    Attributes:
        config: Service configuration
        status: Current service status
        uptime: Service uptime in seconds (if available)
        memory_usage: Memory usage in bytes (if available)
    """

    config: ServiceConfig
    status: ServiceStatus = ServiceStatus.UNKNOWN
    uptime: Optional[int] = None
    memory_usage: Optional[int] = None

    def get_uptime_str(self) -> str:
        """Get human-readable uptime string.

        Returns:
            Formatted uptime string (e.g., '2h 34m')
        """
        if self.uptime is None:
            return "N/A"

        hours, remainder = divmod(self.uptime, 3600)
        minutes, seconds = divmod(remainder, 60)

        if hours > 0:
            return f"{hours}h {minutes}m"
        elif minutes > 0:
            return f"{minutes}m {seconds}s"
        else:
            return f"{seconds}s"

    def get_memory_str(self) -> str:
        """Get human-readable memory usage string.

        Returns:
            Formatted memory string (e.g., '245 MB')
        """
        if self.memory_usage is None:
            return "N/A"

        mb = self.memory_usage / (1024 * 1024)
        if mb >= 1024:
            gb = mb / 1024
            return f"{gb:.1f} GB"
        else:
            return f"{mb:.0f} MB"
