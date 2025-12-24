#!/usr/bin/env python3
"""Entry point for Systemd Tray application."""

import sys
import logging
from pathlib import Path
from PyQt6.QtWidgets import QApplication, QMenu, QMessageBox
from PyQt6.QtGui import QAction, QIcon
from PyQt6.QtCore import QSharedMemory
from PyQt6.QtNetwork import QLocalServer, QLocalSocket

from .app import NyxApp
from .utils.constants import APP_NAME, APP_ICON, APP_ICON_PATH, CONFIG_DIR, LOG_FILE, ORGANIZATION_NAME

# Server name for IPC
IPC_SERVER_NAME = "nyx_ipc_server"


def setup_logging():
    """Set up application logging."""
    # Ensure config directory exists
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)

    # Configure logging
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        handlers=[
            logging.FileHandler(LOG_FILE),
            logging.StreamHandler()
        ]
    )

    logger = logging.getLogger(__name__)
    logger.info("=" * 60)
    logger.info(f"Starting {APP_NAME}")
    logger.info("=" * 60)


def check_single_instance() -> QSharedMemory:
    """Check if another instance is already running.

    Returns:
        QSharedMemory instance if this is the only instance, None otherwise
    """
    shared_memory = QSharedMemory("NyxUniqueKey")

    if not shared_memory.create(1):
        # Another instance is already running - send message to show window
        logging.info("Another instance is already running - sending show window signal")
        send_show_window_signal()
        return None

    return shared_memory


def send_show_window_signal():
    """Send a signal to the running instance to show its main window.

    Uses retry logic to handle cases where the first instance is still starting up.
    """
    import time

    max_retries = 5
    retry_delays = [100, 200, 500, 1000, 2000]  # milliseconds, total ~3.8 seconds

    for attempt in range(max_retries):
        socket = QLocalSocket()
        socket.connectToServer(IPC_SERVER_NAME)

        if socket.waitForConnected(1000):
            socket.write(b"SHOW_WINDOW")
            socket.flush()
            socket.waitForBytesWritten(1000)
            socket.disconnectFromServer()
            logging.info(f"Successfully sent show window signal to running instance (attempt {attempt + 1})")
            return True

        socket.abort()

        if attempt < max_retries - 1:
            delay_ms = retry_delays[attempt]
            logging.debug(f"Connection attempt {attempt + 1} failed, retrying in {delay_ms}ms...")
            time.sleep(delay_ms / 1000.0)

    logging.warning("Could not connect to running instance after multiple retries - it may have crashed during startup")
    return False


def setup_ipc_server(app: NyxApp) -> QLocalServer:
    """Set up IPC server to receive commands from other instances.

    Args:
        app: NyxApp instance

    Returns:
        QLocalServer instance or None on failure
    """
    # Remove any existing server with the same name
    QLocalServer.removeServer(IPC_SERVER_NAME)

    server = QLocalServer()

    def on_new_connection():
        """Handle new connection from another instance."""
        client = server.nextPendingConnection()
        if client:
            client.waitForReadyRead(1000)
            data = client.readAll().data()

            if data == b"SHOW_WINDOW":
                logging.info("Received show window command")
                app.main_window.show()
                app.main_window.raise_()
                app.main_window.activateWindow()

            client.disconnectFromServer()

    server.newConnection.connect(on_new_connection)

    if not server.listen(IPC_SERVER_NAME):
        logging.error(f"Failed to start IPC server: {server.errorString()}")
        return None

    logging.info("IPC server started successfully")
    return server


def create_main_menu(app: NyxApp) -> QMenu:
    """Create the main application menu.

    Args:
        app: NyxApp instance

    Returns:
        QMenu for the main application
    """
    menu = QMenu()

    # Add service action
    add_action = QAction("Add Service...", menu)
    add_action.triggered.connect(lambda: show_add_service_dialog(app))
    menu.addAction(add_action)

    menu.addSeparator()

    # Settings action (placeholder for future)
    # settings_action = QAction("Settings...", menu)
    # menu.addAction(settings_action)

    # About action
    about_action = QAction("About", menu)
    about_action.triggered.connect(show_about_dialog)
    menu.addAction(about_action)

    menu.addSeparator()

    # Quit action
    quit_action = QAction("Quit", menu)
    quit_action.triggered.connect(lambda: quit_application(app))
    menu.addAction(quit_action)

    return menu


def show_add_service_dialog(app: NyxApp):
    """Show the add service dialog.

    Args:
        app: NyxApp instance
    """
    from .ui.dialogs.add_service import AddServiceDialog

    dialog = AddServiceDialog()
    if dialog.exec():
        service_config = dialog.get_service_config()
        app.add_service(service_config)


def show_about_dialog():
    """Show the about dialog."""
    from . import __version__

    QMessageBox.about(
        None,
        f"About {APP_NAME}",
        f"""<h3>{APP_NAME}</h3>
        <p>Version {__version__}</p>
        <p>A system tray application for managing systemd services.</p>
        <p>Features:</p>
        <ul>
        <li>Start/Stop/Restart services from system tray</li>
        <li>Real-time status monitoring</li>
        <li>View systemd logs</li>
        <li>Desktop notifications</li>
        </ul>
        <p>Built with PyQt6. Tested on KDE Plasma and GNOME.</p>
        <p>GitHub: <a href="https://github.com/optis20052">optis20052</a></p>
        """
    )


def quit_application(app: NyxApp):
    """Quit the application.

    Args:
        app: NyxApp instance
    """
    logging.info("Quitting application")
    app.cleanup()
    QApplication.quit()


def main():
    """Main entry point."""
    # Set up logging
    setup_logging()
    logger = logging.getLogger(__name__)

    # Parse command line arguments
    import argparse
    parser = argparse.ArgumentParser(description=f"{APP_NAME} - Manage systemd services from system tray")
    parser.add_argument('--show-window', action='store_true',
                        help='Show the management window on startup')
    parser.add_argument('--no-tray', action='store_true',
                        help='Hide the main tray icon (only show service icons)')
    parser.add_argument('--startup', action='store_true',
                        help='Started from autostart (hides window, internal use)')
    args = parser.parse_args()

    # Create Qt application - set properties BEFORE creating any widgets
    qt_app = QApplication(sys.argv)

    # Set these properties to avoid showing "python3" in KDE
    qt_app.setApplicationName(APP_NAME)  # This sets WM_CLASS
    qt_app.setApplicationDisplayName(APP_NAME)
    qt_app.setOrganizationName(ORGANIZATION_NAME)
    qt_app.setDesktopFileName("nyx")  # Must match the .desktop file name

    # Check for single instance BEFORE creating any GUI elements
    shared_memory = check_single_instance()
    if shared_memory is None:
        # Another instance is running, signal sent, exit immediately without showing anything
        sys.exit(0)

    qt_app.setQuitOnLastWindowClosed(args.no_tray)  # Quit when window closes if no tray

    # Set application icon - use custom icon if available, otherwise theme icon
    if APP_ICON_PATH.exists():
        app_icon = QIcon(str(APP_ICON_PATH))
    else:
        app_icon = QIcon.fromTheme(APP_ICON)
    qt_app.setWindowIcon(app_icon)

    try:
        # Create main application
        tray_app = NyxApp()

        # Set up IPC server to listen for show window commands
        ipc_server = setup_ipc_server(tray_app)
        if not ipc_server:
            logger.error("Failed to set up IPC server")

        # Hide main tray icon if requested
        if args.no_tray and tray_app.main_tray_icon:
            tray_app.main_tray_icon.hide()
            logger.info("Main tray icon hidden as requested")

        # Show main window based on launch context
        # Priority: --show-window > --startup > user preference

        if args.show_window:
            # Explicit request to show window (highest priority)
            tray_app.main_window.show()
            logger.info("Main window shown as requested (--show-window)")
        elif args.startup:
            # Launched from autostart - always hide window
            logger.info("Main window hidden (launched from autostart)")
        else:
            # Manual launch - check user preference
            minimize_to_tray = tray_app.config_manager.get_setting("minimize_to_tray", False)
            if minimize_to_tray:
                logger.info("Main window hidden (minimize_to_tray is enabled)")
            else:
                # Default for manual launch: show window
                tray_app.main_window.show()
                logger.info("Main window shown (manual launch)")

        logger.info("Application started successfully")

        # Run the application
        exit_code = qt_app.exec()

        # Cleanup
        tray_app.cleanup()
        if ipc_server:
            ipc_server.close()

        logger.info(f"Application exited with code {exit_code}")
        return exit_code

    except Exception as e:
        logger.exception(f"Fatal error: {e}")
        QMessageBox.critical(
            None,
            f"{APP_NAME} Error",
            f"A fatal error occurred:\n\n{str(e)}\n\nCheck the log file at:\n{LOG_FILE}"
        )
        return 1

    finally:
        # Release shared memory
        if shared_memory:
            shared_memory.detach()


if __name__ == "__main__":
    sys.exit(main())
