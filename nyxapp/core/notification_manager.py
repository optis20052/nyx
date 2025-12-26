"""Notification manager for desktop notifications."""

import logging
import time
from typing import Dict, Tuple
from PyQt6.QtDBus import QDBusConnection, QDBusInterface, QDBusReply
from PyQt6.QtWidgets import QSystemTrayIcon

from ..models.service import ServiceStatus
from ..utils.constants import APP_NAME, APP_ICON, NOTIFICATION_TIMEOUT, NOTIFICATION_RATE_LIMIT

logger = logging.getLogger(__name__)


class NotificationManager:
    """Manages desktop notifications via KDE D-Bus or Qt fallback."""

    def __init__(self):
        """Initialize the notification manager."""
        self._last_notification: Dict[str, float] = {}  # Track last notification time per service
        self._kde_available = self._check_kde_notifications()

    def _check_kde_notifications(self) -> bool:
        """Check if KDE notifications are available via D-Bus.

        Returns:
            True if KDE notifications available, False otherwise
        """
        try:
            bus = QDBusConnection.sessionBus()
            if not bus.isConnected():
                logger.warning("D-Bus session bus not connected")
                return False

            interface = QDBusInterface(
                "org.freedesktop.Notifications",
                "/org/freedesktop/Notifications",
                "org.freedesktop.Notifications",
                bus
            )

            if not interface.isValid():
                logger.warning("KDE Notifications interface not available")
                return False

            logger.info("KDE Notifications available via D-Bus")
            return True

        except Exception as e:
            logger.warning(f"KDE Notifications not available: {e}")
            return False

    def notify_service_started(self, service_name: str, display_name: str):
        """Send notification when service starts.

        Args:
            service_name: Service identifier for rate limiting
            display_name: User-friendly service name
        """
        if self._should_notify(service_name):
            self._send_notification(
                f"{display_name} Started",
                f"Service {display_name} has been started successfully.",
                "normal"
            )
            self._update_notification_time(service_name)

    def notify_service_stopped(self, service_name: str, display_name: str):
        """Send notification when service stops.

        Args:
            service_name: Service identifier for rate limiting
            display_name: User-friendly service name
        """
        if self._should_notify(service_name):
            self._send_notification(
                f"{display_name} Stopped",
                f"Service {display_name} has been stopped.",
                "normal"
            )
            self._update_notification_time(service_name)

    def notify_service_failed(self, service_name: str, display_name: str, error: str = ""):
        """Send notification when service fails.

        Args:
            service_name: Service identifier for rate limiting
            display_name: User-friendly service name
            error: Optional error message
        """
        if self._should_notify(service_name):
            message = f"Service {display_name} has failed."
            if error:
                message += f"\n{error}"

            self._send_notification(
                f"{display_name} Failed",
                message,
                "critical"
            )
            self._update_notification_time(service_name)

    def notify_error(self, title: str, message: str):
        """Send generic error notification.

        Args:
            title: Notification title
            message: Notification message
        """
        self._send_notification(title, message, "critical")

    def notify_info(self, title: str, message: str):
        """Send generic info notification.

        Args:
            title: Notification title
            message: Notification message
        """
        self._send_notification(title, message, "normal")

    def _send_notification(self, title: str, message: str, urgency: str = "normal"):
        """Send a notification using KDE D-Bus or Qt fallback.

        Args:
            title: Notification title
            message: Notification message
            urgency: Urgency level ('low', 'normal', 'critical')
        """
        if self._kde_available:
            self._send_kde_notification(title, message, urgency)
        else:
            self._send_qt_notification(title, message)

    def _send_kde_notification(self, title: str, message: str, urgency: str):
        """Send notification via KDE D-Bus.

        Args:
            title: Notification title
            message: Notification message
            urgency: Urgency level ('low', 'normal', 'critical')
        """
        try:
            bus = QDBusConnection.sessionBus()
            interface = QDBusInterface(
                "org.freedesktop.Notifications",
                "/org/freedesktop/Notifications",
                "org.freedesktop.Notifications",
                bus
            )

            if not interface.isValid():
                logger.warning("D-Bus interface invalid, falling back to Qt notifications")
                self._send_qt_notification(title, message)
                return

            # Map urgency to level
            urgency_level = {
                "low": 0,
                "normal": 1,
                "critical": 2
            }.get(urgency, 1)

            # Call Notify method
            # Signature: Notify(app_name, replaces_id, app_icon, summary, body, actions, hints, timeout)
            reply = interface.call(
                "Notify",
                APP_NAME,           # app_name
                0,                  # replaces_id (0 = new notification)
                APP_ICON,           # app_icon
                title,              # summary
                message,            # body
                [],                 # actions
                {"urgency": urgency_level},  # hints
                NOTIFICATION_TIMEOUT  # timeout in milliseconds
            )

            if reply.type() == QDBusReply.ErrorType:
                logger.error(f"D-Bus notification error: {reply.error().message()}")
                self._send_qt_notification(title, message)

        except Exception as e:
            logger.error(f"Failed to send KDE notification: {e}")
            self._send_qt_notification(title, message)

    def _send_qt_notification(self, title: str, message: str):
        """Send notification via Qt system tray (fallback).

        Args:
            title: Notification title
            message: Notification message
        """
        try:
            # Note: This requires a QSystemTrayIcon instance
            # We'll use the basic showMessage which works across platforms
            # The actual tray icon will handle this in the real implementation
            logger.info(f"Qt Notification: {title} - {message}")
        except Exception as e:
            logger.error(f"Failed to send Qt notification: {e}")

    def _should_notify(self, service_name: str) -> bool:
        """Check if notification should be sent (rate limiting).

        Args:
            service_name: Service identifier

        Returns:
            True if notification should be sent, False if rate limited
        """
        current_time = time.time()
        last_time = self._last_notification.get(service_name, 0)

        if current_time - last_time < NOTIFICATION_RATE_LIMIT:
            logger.debug(f"Rate limiting notification for {service_name}")
            return False

        return True

    def _update_notification_time(self, service_name: str):
        """Update the last notification time for a service.

        Args:
            service_name: Service identifier
        """
        self._last_notification[service_name] = time.time()
