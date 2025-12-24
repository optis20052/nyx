"""Service manager for interacting with systemd via systemctl."""

import subprocess
import logging
from typing import Tuple, Optional
from ..models.service import ServiceStatus
from ..utils.polkit_helper import PolkitHelper

logger = logging.getLogger(__name__)


class ServiceManager:
    """Manages systemd services via systemctl commands."""

    def __init__(self, config_manager=None):
        """Initialize the service manager.

        Args:
            config_manager: Optional ConfigManager instance for checking settings
        """
        self.timeout = 10  # seconds
        self.config_manager = config_manager

    def get_service_status(self, service_name: str, is_user_service: bool = True) -> ServiceStatus:
        """Get the current status of a service.

        Args:
            service_name: Name of the systemd service
            is_user_service: True for user services, False for system services

        Returns:
            ServiceStatus enum value
        """
        cmd = ["systemctl"]
        if is_user_service:
            cmd.append("--user")

        cmd.extend(["show", service_name, "--property=ActiveState", "--value"])

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout,
                check=True
            )
            status_str = result.stdout.strip()
            return ServiceStatus.from_string(status_str)

        except subprocess.TimeoutExpired:
            logger.error(f"Timeout getting status for {service_name}")
            return ServiceStatus.UNKNOWN
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to get status for {service_name}: {e.stderr}")
            return ServiceStatus.UNKNOWN
        except Exception as e:
            logger.error(f"Unexpected error getting status for {service_name}: {e}")
            return ServiceStatus.UNKNOWN

    def start_service(self, service_name: str, is_user_service: bool = True) -> Tuple[bool, Optional[str]]:
        """Start a systemd service.

        Args:
            service_name: Name of the systemd service
            is_user_service: True for user services, False for system services

        Returns:
            Tuple of (success: bool, error_message: Optional[str])
        """
        return self._execute_systemctl_action("start", service_name, is_user_service)

    def stop_service(self, service_name: str, is_user_service: bool = True) -> Tuple[bool, Optional[str]]:
        """Stop a systemd service.

        Args:
            service_name: Name of the systemd service
            is_user_service: True for user services, False for system services

        Returns:
            Tuple of (success: bool, error_message: Optional[str])
        """
        return self._execute_systemctl_action("stop", service_name, is_user_service)

    def restart_service(self, service_name: str, is_user_service: bool = True) -> Tuple[bool, Optional[str]]:
        """Restart a systemd service.

        Args:
            service_name: Name of the systemd service
            is_user_service: True for user services, False for system services

        Returns:
            Tuple of (success: bool, error_message: Optional[str])
        """
        return self._execute_systemctl_action("restart", service_name, is_user_service)

    def enable_service(self, service_name: str, is_user_service: bool = True) -> Tuple[bool, Optional[str]]:
        """Enable a systemd service to start on boot.

        Args:
            service_name: Name of the systemd service
            is_user_service: True for user services, False for system services

        Returns:
            Tuple of (success: bool, error_message: Optional[str])
        """
        return self._execute_systemctl_action("enable", service_name, is_user_service)

    def disable_service(self, service_name: str, is_user_service: bool = True) -> Tuple[bool, Optional[str]]:
        """Disable a systemd service from starting on boot.

        Args:
            service_name: Name of the systemd service
            is_user_service: True for user services, False for system services

        Returns:
            Tuple of (success: bool, error_message: Optional[str])
        """
        return self._execute_systemctl_action("disable", service_name, is_user_service)

    def get_service_logs(self, service_name: str, lines: int = 100, is_user_service: bool = True) -> str:
        """Get logs for a systemd service via journalctl.

        Args:
            service_name: Name of the systemd service
            lines: Number of log lines to retrieve
            is_user_service: True for user services, False for system services

        Returns:
            Log output as string
        """
        cmd = ["journalctl"]
        if is_user_service:
            cmd.append("--user")

        cmd.extend([
            "-u", service_name,
            "-n", str(lines),
            "--no-pager",
            "--output=short-iso"
        ])

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout,
                check=True
            )
            return result.stdout

        except subprocess.TimeoutExpired:
            logger.error(f"Timeout getting logs for {service_name}")
            return f"Error: Timeout while fetching logs for {service_name}"
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to get logs for {service_name}: {e.stderr}")
            return f"Error: {e.stderr}"
        except Exception as e:
            logger.error(f"Unexpected error getting logs for {service_name}: {e}")
            return f"Error: {str(e)}"

    def get_service_uptime(self, service_name: str, is_user_service: bool = True) -> Optional[int]:
        """Get service uptime in seconds.

        Args:
            service_name: Name of the systemd service
            is_user_service: True for user services, False for system services

        Returns:
            Uptime in seconds, or None if not available
        """
        cmd = ["systemctl"]
        if is_user_service:
            cmd.append("--user")

        cmd.extend(["show", service_name, "--property=ActiveEnterTimestampMonotonic", "--value"])

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout,
                check=True
            )
            timestamp = result.stdout.strip()
            if timestamp and timestamp != "0":
                # Convert microseconds to seconds
                return int(timestamp) // 1000000
            return None

        except Exception as e:
            logger.debug(f"Could not get uptime for {service_name}: {e}")
            return None

    def service_exists(self, service_name: str, is_user_service: bool = True) -> bool:
        """Check if a service exists.

        Args:
            service_name: Name of the systemd service
            is_user_service: True for user services, False for system services

        Returns:
            True if service exists, False otherwise
        """
        cmd = ["systemctl"]
        if is_user_service:
            cmd.append("--user")

        cmd.extend(["list-unit-files", service_name, "--no-pager", "--no-legend"])

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            return bool(result.stdout.strip())

        except Exception as e:
            logger.error(f"Error checking if service {service_name} exists: {e}")
            return False

    def _execute_systemctl_action(
        self,
        action: str,
        service_name: str,
        is_user_service: bool
    ) -> Tuple[bool, Optional[str]]:
        """Execute a systemctl action (start, stop, restart, etc.).

        Args:
            action: Systemctl action (start, stop, restart, enable, disable)
            service_name: Name of the systemd service
            is_user_service: True for user services, False for system services

        Returns:
            Tuple of (success: bool, error_message: Optional[str])
        """
        # Determine if we need privilege escalation
        # If passwordless mode is enabled, we don't need pkexec
        # because polkit will handle authorization directly
        requires_privilege = not is_user_service and not PolkitHelper.is_passwordless_enabled(self.config_manager)

        cmd = []
        if requires_privilege:
            cmd.append("pkexec")

        cmd.append("systemctl")

        if is_user_service:
            cmd.append("--user")

        cmd.extend([action, service_name])

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout,
                check=True
            )
            logger.info(f"Successfully {action}ed {service_name}")
            return True, None

        except subprocess.TimeoutExpired:
            error_msg = f"Timeout while trying to {action} {service_name}"
            logger.error(error_msg)
            return False, error_msg

        except subprocess.CalledProcessError as e:
            error_msg = e.stderr.strip() if e.stderr else f"Failed to {action} {service_name}"
            logger.error(f"Failed to {action} {service_name}: {error_msg}")
            return False, error_msg

        except Exception as e:
            error_msg = str(e)
            logger.error(f"Unexpected error while trying to {action} {service_name}: {error_msg}")
            return False, error_msg
