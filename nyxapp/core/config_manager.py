"""Configuration manager for loading and saving app settings."""

import logging
import shutil
from pathlib import Path
from typing import List, Dict, Any, Optional
import yaml

from ..models.service import ServiceConfig
from ..utils.constants import CONFIG_DIR, CONFIG_FILE, DEFAULT_UPDATE_INTERVAL, ICONS_DIR

logger = logging.getLogger(__name__)


class ConfigManager:
    """Manages application configuration and service definitions."""

    CONFIG_VERSION = "1.0"

    def __init__(self):
        """Initialize the config manager."""
        self._ensure_config_dir()
        self.services: List[ServiceConfig] = []
        self.settings: Dict[str, Any] = {}

    def _ensure_config_dir(self):
        """Create config directory and subdirectories if they don't exist."""
        CONFIG_DIR.mkdir(parents=True, exist_ok=True)
        ICONS_DIR.mkdir(parents=True, exist_ok=True)

    def load_config(self) -> bool:
        """Load configuration from file.

        Returns:
            True if config loaded successfully, False otherwise
        """
        if not CONFIG_FILE.exists():
            logger.info("Config file not found, using defaults")
            self._load_defaults()
            return False

        try:
            with open(CONFIG_FILE, 'r') as f:
                data = yaml.safe_load(f)

            if not data:
                logger.warning("Empty config file, using defaults")
                self._load_defaults()
                return False

            # Validate and load
            if not self._validate_config(data):
                logger.error("Invalid config file, using defaults")
                self._load_defaults()
                return False

            # Load services
            self.services = []
            for service_data in data.get("services", []):
                try:
                    service = ServiceConfig.from_dict(service_data)
                    self.services.append(service)
                except Exception as e:
                    logger.error(f"Failed to load service config: {e}")

            # Load settings
            self.settings = data.get("settings", {})
            self._ensure_default_settings()

            logger.info(f"Loaded {len(self.services)} services from config")
            return True

        except yaml.YAMLError as e:
            logger.error(f"YAML parsing error: {e}")
            self._load_defaults()
            return False
        except Exception as e:
            logger.error(f"Failed to load config: {e}")
            self._load_defaults()
            return False

    def save_config(self) -> bool:
        """Save configuration to file.

        Returns:
            True if saved successfully, False otherwise
        """
        try:
            # Create backup if config exists
            if CONFIG_FILE.exists():
                backup_file = CONFIG_FILE.with_suffix('.yaml.bak')
                shutil.copy2(CONFIG_FILE, backup_file)
                logger.debug(f"Created backup at {backup_file}")

            # Prepare data
            data = {
                "version": self.CONFIG_VERSION,
                "services": [service.to_dict() for service in self.services],
                "settings": self.settings
            }

            # Write to temp file first (atomic write)
            temp_file = CONFIG_FILE.with_suffix('.yaml.tmp')
            with open(temp_file, 'w') as f:
                yaml.dump(data, f, default_flow_style=False, sort_keys=False)

            # Move temp to final location
            temp_file.replace(CONFIG_FILE)

            logger.info(f"Saved {len(self.services)} services to config")
            return True

        except Exception as e:
            logger.error(f"Failed to save config: {e}")
            return False

    def add_service(self, service: ServiceConfig) -> bool:
        """Add a new service to the configuration.

        Args:
            service: ServiceConfig to add

        Returns:
            True if added successfully, False if service already exists
        """
        # Check if service already exists
        if any(s.name == service.name and s.service_type == service.service_type
               for s in self.services):
            logger.warning(f"Service {service.name} ({service.service_type}) already exists")
            return False

        self.services.append(service)
        self.save_config()
        logger.info(f"Added service: {service.name}")
        return True

    def remove_service(self, service_name: str, service_type: str = "user") -> bool:
        """Remove a service from the configuration.

        Args:
            service_name: Name of the service to remove
            service_type: Type of service ('user' or 'system')

        Returns:
            True if removed successfully, False if not found
        """
        original_count = len(self.services)
        self.services = [
            s for s in self.services
            if not (s.name == service_name and s.service_type == service_type)
        ]

        if len(self.services) < original_count:
            self.save_config()
            logger.info(f"Removed service: {service_name}")
            return True

        logger.warning(f"Service {service_name} ({service_type}) not found")
        return False

    def update_service(self, old_name: str, old_type: str, updated_service: ServiceConfig) -> bool:
        """Update an existing service configuration.

        Args:
            old_name: Current name of the service
            old_type: Current type of the service
            updated_service: Updated ServiceConfig

        Returns:
            True if updated successfully, False if not found
        """
        for i, service in enumerate(self.services):
            if service.name == old_name and service.service_type == old_type:
                self.services[i] = updated_service
                self.save_config()
                logger.info(f"Updated service: {old_name}")
                return True

        logger.warning(f"Service {old_name} ({old_type}) not found")
        return False

    def get_service(self, service_name: str, service_type: str = "user") -> Optional[ServiceConfig]:
        """Get a service configuration by name.

        Args:
            service_name: Name of the service
            service_type: Type of service ('user' or 'system')

        Returns:
            ServiceConfig if found, None otherwise
        """
        for service in self.services:
            if service.name == service_name and service.service_type == service_type:
                return service
        return None

    def get_enabled_services(self) -> List[ServiceConfig]:
        """Get list of enabled services.

        Returns:
            List of enabled ServiceConfig objects
        """
        return [s for s in self.services if s.enabled]

    def get_setting(self, key: str, default: Any = None) -> Any:
        """Get a setting value.

        Args:
            key: Setting key
            default: Default value if key not found

        Returns:
            Setting value or default
        """
        return self.settings.get(key, default)

    def set_setting(self, key: str, value: Any):
        """Set a setting value.

        Args:
            key: Setting key
            value: Setting value
        """
        self.settings[key] = value
        self.save_config()

    def _validate_config(self, data: dict) -> bool:
        """Validate configuration data structure.

        Args:
            data: Configuration dictionary

        Returns:
            True if valid, False otherwise
        """
        if not isinstance(data, dict):
            logger.error("Config must be a dictionary")
            return False

        if "version" not in data:
            logger.warning("Config missing version, assuming valid")

        if "services" in data and not isinstance(data["services"], list):
            logger.error("Services must be a list")
            return False

        if "settings" in data and not isinstance(data["settings"], dict):
            logger.error("Settings must be a dictionary")
            return False

        return True

    def _load_defaults(self):
        """Load default configuration."""
        self.services = []
        self.settings = {}
        self._ensure_default_settings()
        logger.info("Loaded default configuration")

    def _ensure_default_settings(self):
        """Ensure all default settings exist."""
        defaults = {
            "update_interval": DEFAULT_UPDATE_INTERVAL,
            "show_notifications": True,
            "minimize_to_tray": False,  # Default: show window on manual launch
            "passwordless_mode": False,
            "show_main_tray": True
        }

        for key, value in defaults.items():
            if key not in self.settings:
                self.settings[key] = value
