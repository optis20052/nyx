"""PolicyKit helper for passwordless systemctl operations."""

import logging
import subprocess
from pathlib import Path

logger = logging.getLogger(__name__)


class PolkitHelper:
    """Helper for managing PolicyKit rules for passwordless systemctl."""

    POLKIT_RULE_FILE = "/etc/polkit-1/rules.d/50-nyxapp-systemctl.rules"

    # PolicyKit rule that allows passwordless systemctl for the current user
    POLKIT_RULE_TEMPLATE = """/* Allow {username} to manage systemd services without password */
polkit.addRule(function(action, subject) {{
    if ((action.id == "org.freedesktop.systemd1.manage-units" ||
         action.id == "org.freedesktop.systemd1.manage-unit-files" ||
         action.id == "org.freedesktop.systemd1.reload-daemon") &&
        subject.user == "{username}") {{
        return polkit.Result.YES;
    }}
}});
"""

    @staticmethod
    def is_passwordless_enabled(config_manager=None) -> bool:
        """Check if passwordless mode is enabled.

        Args:
            config_manager: Optional ConfigManager instance to check settings

        Returns:
            True if polkit rule exists or config says it's enabled, False otherwise
        """
        # First check config if available
        if config_manager is not None:
            return config_manager.get_setting("passwordless_mode", False)

        # Fallback to checking file (may fail due to permissions)
        try:
            return Path(PolkitHelper.POLKIT_RULE_FILE).exists()
        except (PermissionError, OSError):
            # If we can't access the directory, assume the rule doesn't exist
            return False

    @staticmethod
    def enable_passwordless_mode(username: str, config_manager=None) -> tuple[bool, str]:
        """Enable passwordless mode by creating a polkit rule.

        Args:
            username: Current username
            config_manager: Optional ConfigManager instance to save settings

        Returns:
            Tuple of (success, message)
        """
        try:
            # Generate the polkit rule
            rule_content = PolkitHelper.POLKIT_RULE_TEMPLATE.format(username=username)

            # Write rule to a temporary file
            temp_file = Path("/tmp/systemd-tray-polkit.rules")
            temp_file.write_text(rule_content)

            # Use pkexec to copy the file to the polkit directory
            # This will ask for password once
            result = subprocess.run(
                [
                    "pkexec", "sh", "-c",
                    f"cp {temp_file} {PolkitHelper.POLKIT_RULE_FILE} && "
                    f"chmod 644 {PolkitHelper.POLKIT_RULE_FILE}"
                ],
                capture_output=True,
                text=True,
                timeout=30
            )

            # Clean up temp file
            temp_file.unlink(missing_ok=True)

            if result.returncode == 0:
                # Reload polkit to apply the new rule
                try:
                    subprocess.run(
                        ["pkexec", "systemctl", "restart", "polkit"],
                        capture_output=True,
                        text=True,
                        timeout=10
                    )
                except Exception as e:
                    logger.warning(f"Could not restart polkit service: {e}")
                    # Continue anyway, the rule might still work

                # Save to config if provided
                if config_manager is not None:
                    config_manager.set_setting("passwordless_mode", True)

                logger.info("Passwordless mode enabled successfully")
                return True, "Passwordless mode enabled! You won't be asked for password anymore."
            else:
                error_msg = result.stderr or "Failed to create polkit rule"
                logger.error(f"Failed to enable passwordless mode: {error_msg}")
                return False, f"Failed to enable passwordless mode: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Operation timed out"
        except Exception as e:
            logger.error(f"Error enabling passwordless mode: {e}")
            return False, f"Error: {str(e)}"

    @staticmethod
    def disable_passwordless_mode(config_manager=None) -> tuple[bool, str]:
        """Disable passwordless mode by removing the polkit rule.

        Args:
            config_manager: Optional ConfigManager instance to save settings

        Returns:
            Tuple of (success, message)
        """
        try:
            # Use pkexec to remove the file
            result = subprocess.run(
                ["pkexec", "rm", "-f", PolkitHelper.POLKIT_RULE_FILE],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode == 0:
                # Save to config if provided
                if config_manager is not None:
                    config_manager.set_setting("passwordless_mode", False)

                logger.info("Passwordless mode disabled successfully")
                return True, "Passwordless mode disabled. You will be asked for password again."
            else:
                error_msg = result.stderr or "Failed to remove polkit rule"
                logger.error(f"Failed to disable passwordless mode: {error_msg}")
                return False, f"Failed to disable passwordless mode: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Operation timed out"
        except Exception as e:
            logger.error(f"Error disabling passwordless mode: {e}")
            return False, f"Error: {str(e)}"

    @staticmethod
    def get_current_username() -> str:
        """Get the current username.

        Returns:
            Current username
        """
        import os
        return os.getenv("USER") or os.getenv("USERNAME") or "unknown"
