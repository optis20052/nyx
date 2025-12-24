# Nyx Quick Start Guide

## Running the Application

```bash
python -m nyx.main
```

Or if installed:
```bash
nyx
```

## First-Time Setup

When you run the app for the first time:

1. **Look for the main tray icon** in your system tray (it uses the "preferences-system" icon)
2. **Right-click** on the main icon to open the menu
3. **Select "Add Service..."** to add your first service

## Adding a Service

In the "Add Service" dialog:

1. **Service Name**: Enter the systemd unit name (e.g., `docker`, `postgresql`, `nginx`)
   - Find available services: `systemctl --user list-units` (for user services)
   - Or: `systemctl list-units` (for system services)

2. **Display Name**: Enter a friendly name (optional, defaults to service name)

3. **Icon Name**: Enter a freedesktop icon name (optional)
   - Examples: `database`, `docker`, `network`, `mail`, `web-browser`
   - Default: `application-x-executable`

4. **Service Type**:
   - Choose `user` for services running under your account
   - Choose `system` for system-wide services (requires sudo for control)

5. **Options**:
   -  Start service when app launches (auto-start)
   -  Show tray icon (enabled by default)

## Example Services to Try

### User Services
```bash
# Check what user services you have
systemctl --user list-units --type=service
```

Common user services:
- `pulseaudio` - Sound server
- `dbus` - D-Bus session daemon

### System Services
```bash
# Check system services (common ones)
systemctl list-units --type=service | grep -E '(docker|postgresql|nginx|apache2|mysql)'
```

Common system services:
- `docker` - Docker daemon
- `postgresql` - PostgreSQL database
- `nginx` - Nginx web server
- `apache2` - Apache web server
- `mysql` - MySQL database

## Using the App

Once you've added services:

1. **Each service gets its own tray icon** with a colored status indicator:
   - 🟢 Green = Running
   - 🔴 Red = Failed
   - ⚫ Gray = Stopped
   - 🟡 Yellow = Starting/Stopping

2. **Right-click any service icon** to:
   - Start/Stop/Restart the service
   - View Logs (with auto-refresh)
   - Edit service settings
   - Remove from tray

3. **Main tray icon** (always visible) provides:
   - Show Manager (opens the full management window)
   - Settings... (configure app behavior and autostart)
   - Add Service...
   - Enable/Disable Passwordless Mode
   - About
   - Quit

## Configuration

Config file location: `~/.config/nyx/config.yaml`

You can edit settings via the Settings dialog (right-click tray icon → Settings) or manually edit the file:

```yaml
version: 1.0
services:
  - name: docker
    display_name: Docker
    icon: docker
    service_type: system
    auto_start: false
    enabled: true
settings:
  update_interval: 5         # Status check interval in seconds
  show_notifications: true   # Desktop notifications
  minimize_to_tray: false    # Start minimized on manual launch
  passwordless_mode: false   # Passwordless service control via PolicyKit
  show_main_tray: true       # Show main tray icon
```

### Settings Explained

- **Update Interval**: How often (in seconds) to check service status
- **Show Notifications**: Desktop notifications for service state changes
- **Minimize to Tray**: If enabled, app starts minimized when launched manually
- **Passwordless Mode**: Use PolicyKit to manage services without password prompts
- **Show Main Tray**: Show/hide the main application tray icon

## Troubleshooting

### No services showing?
- Check that the service name is correct
- Verify service exists: `systemctl list-units | grep <name>`
- Check config file: `~/.config/nyx/config.yaml`
- Check logs: `~/.config/nyx/app.log`

### Permission errors with system services?
- System services require root access
- The app uses `pkexec` for graphical password prompts
- Enable **Passwordless Mode** in Settings to avoid repeated password prompts
- Alternative: Use user services when possible

### App won't start?
- Check logs: `~/.config/nyx/app.log`
- Ensure you're running in the virtual environment
- Verify dependencies: `pip list | grep -E '(PyQt6|PyYAML)'`

## Auto-Start on Login

**Easy way (Recommended):**
1. Right-click the main tray icon
2. Click "Settings..."
3. Check "Start Nyx automatically on login"
4. Click "Save"

The app will now start automatically when you log in, minimized to tray.

**Manual way:**
```bash
# The autostart file is automatically managed by the app
# Location: ~/.config/autostart/nyx.desktop
```

## Logs

- Application log: `~/.config/nyx/app.log`
- Service logs: Available via "View Logs" in each service menu

## Desktop Environments

Nyx is tested and works on:
- ✅ KDE Plasma
- ✅ GNOME

It should also work on other Qt-compatible desktop environments.

## Contributing

Found a bug or want to contribute? Visit the project on GitHub!

---

Enjoy managing your systemd services with Nyx! 🌙
