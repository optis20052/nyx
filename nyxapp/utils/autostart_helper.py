"""Autostart helper for managing XDG autostart entries."""

import logging
import shutil
from pathlib import Path

logger = logging.getLogger(__name__)


class AutostartHelper:
    """Helper for managing XDG autostart desktop entries."""

    # XDG autostart directory
    AUTOSTART_DIR = Path.home() / ".config" / "autostart"
    AUTOSTART_FILE = AUTOSTART_DIR / "nyxapp.desktop"

    # System desktop file locations to search
    DESKTOP_FILE_LOCATIONS = [
        Path("/usr/share/applications/nyxapp.desktop"),
        Path("/usr/local/share/applications/nyxapp.desktop"),
        Path.home() / ".local" / "share" / "applications" / "nyxapp.desktop",
    ]

    @staticmethod
    def is_autostart_enabled() -> bool:
        """Check if autostart is enabled.

        Returns:
            True if autostart desktop file exists, False otherwise
        """
        return AutostartHelper.AUTOSTART_FILE.exists()

    @staticmethod
    def find_desktop_file() -> Path | None:
        """Find the installed nyxapp.desktop file.

        Returns:
            Path to desktop file if found, None otherwise
        """
        # Check if running from source (development)
        # Look for nyxapp.desktop in current directory or parent
        dev_desktop_file = Path(__file__).parent.parent.parent / "nyxapp.desktop"
        if dev_desktop_file.exists():
            return dev_desktop_file

        # Check system locations
        for location in AutostartHelper.DESKTOP_FILE_LOCATIONS:
            if location.exists():
                return location

        return None

    @staticmethod
    def enable_autostart() -> tuple[bool, str]:
        """Enable autostart by creating/copying desktop file to autostart directory.

        Returns:
            Tuple of (success, message)
        """
        try:
            # Ensure autostart directory exists
            AutostartHelper.AUTOSTART_DIR.mkdir(parents=True, exist_ok=True)

            # Find the desktop file
            desktop_file = AutostartHelper.find_desktop_file()
            if not desktop_file:
                error_msg = (
                    "Could not find nyxapp.desktop file in any standard location.\n"
                    "Please ensure the application is properly installed."
                )
                logger.error(error_msg)
                return False, error_msg

            # Read the desktop file and modify it for autostart
            content = desktop_file.read_text()

            # Modify Exec line to add --startup flag
            modified_content = []
            for line in content.splitlines():
                if line.startswith("Exec="):
                    # Add --startup flag to the command
                    if "--startup" not in line:
                        line = line.rstrip() + " --startup"
                elif line.startswith("StartupNotify="):
                    # Disable startup notification to prevent loading indicator
                    line = "StartupNotify=false"
                modified_content.append(line)

            # Write the modified content to autostart directory
            AutostartHelper.AUTOSTART_FILE.write_text("\n".join(modified_content) + "\n")

            # Make sure it's readable
            AutostartHelper.AUTOSTART_FILE.chmod(0o644)

            logger.info(f"Autostart enabled: created modified desktop file at {AutostartHelper.AUTOSTART_FILE}")
            return True, "Autostart enabled. NyxApp will start automatically on login."

        except PermissionError as e:
            error_msg = f"Permission denied: {e}"
            logger.error(f"Failed to enable autostart: {error_msg}")
            return False, f"Failed to enable autostart:\n{error_msg}"
        except Exception as e:
            error_msg = str(e)
            logger.error(f"Error enabling autostart: {error_msg}")
            return False, f"Error enabling autostart:\n{error_msg}"

    @staticmethod
    def disable_autostart() -> tuple[bool, str]:
        """Disable autostart by removing the desktop file from autostart directory.

        Returns:
            Tuple of (success, message)
        """
        try:
            if AutostartHelper.AUTOSTART_FILE.exists():
                AutostartHelper.AUTOSTART_FILE.unlink()
                logger.info(f"Autostart disabled: removed {AutostartHelper.AUTOSTART_FILE}")
                return True, "Autostart disabled. NyxApp will not start automatically on login."
            else:
                # Already disabled
                logger.info("Autostart was already disabled")
                return True, "Autostart is already disabled."

        except PermissionError as e:
            error_msg = f"Permission denied: {e}"
            logger.error(f"Failed to disable autostart: {error_msg}")
            return False, f"Failed to disable autostart:\n{error_msg}"
        except Exception as e:
            error_msg = str(e)
            logger.error(f"Error disabling autostart: {error_msg}")
            return False, f"Error disabling autostart:\n{error_msg}"
