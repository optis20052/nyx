# Packaging Guide for Nyx

## What We've Set Up

### 1. Application Icon
- Created: `nyx/resources/icons/nyx.svg`
- Application icon for both tray and window
- Used throughout the application

### 2. Desktop Entry
- File: `nyx.desktop`
- Allows launching from application menu
- Categories: System, Monitor, Qt
- Supports autostart when enabled in settings

### 3. Python Package Setup
- File: `setup.py`
- Defines package metadata and dependencies
- Sets up console script entry point: `nyx`

### 4. Debian Package Files
- `debian/control` - Package metadata and dependencies
- `debian/rules` - Build instructions
- `debian/changelog` - Version history
- `debian/copyright` - License information
- `debian/compat` - Debian compatibility level

## Building the .deb Package

### Prerequisites

Install build dependencies:
```bash
sudo apt-get install debhelper dh-python python3-all python3-setuptools \
                     python3-pyqt6 python3-yaml
```

**Note:** The build system includes a fix for Python 3.13 setuptools compatibility. The `debian/rules` file exports `PYTHONPATH` to include `/usr/lib/python3/dist-packages` so setuptools can be found during the build process.

### Build Process

1. **Navigate to project directory:**
   ```bash
   cd /home/ali/Projects/KDE
   ```

2. **Run the build script:**
   ```bash
   ./build-deb.sh
   ```

3. **The .deb file will be created in the parent directory:**
   ```bash
   ls -l ../nyx_1.0.0_all.deb
   ```

## Installing the Package

### Install with dpkg:
```bash
sudo dpkg -i ../nyx_1.0.0_all.deb
```

### Fix dependencies if needed:
```bash
sudo apt-get install -f
```

### Verify installation:
```bash
which nyx
nyx --help
```

## Using the Application

### From Application Menu:
Look for "Nyx" in System → Monitor

### From Command Line:
```bash
# Default (show window on manual launch)
nyx

# Show window (explicit)
nyx --show-window

# Hide main tray icon (only show service icons)
nyx --no-tray

# Started from autostart (internal use)
nyx --startup

# Window only (no tray icons)
nyx --show-window --no-tray
```

## File Locations After Installation

- **Binary:** `/usr/bin/nyx`
- **Python package:** `/usr/lib/python3/dist-packages/nyx/`
- **Desktop file:** `/usr/share/applications/nyx.desktop`
- **Icon:** `/usr/share/icons/hicolor/scalable/apps/nyx.svg`
- **User config:** `~/.config/nyx/config.yaml`
- **User icons:** `~/.config/nyx/icons/`
- **Autostart file:** `~/.config/autostart/nyx.desktop` (when enabled)

## Uninstalling

```bash
sudo apt-get remove nyx
```

## Distribution

To share the .deb package:
1. Copy `../nyx_1.0.0_all.deb` to distribution location
2. Users can install with: `sudo dpkg -i nyx_1.0.0_all.deb`

## Updating Version

1. Update version in `setup.py`
2. Update version in `debian/changelog` with new entry
3. Rebuild package

## Notes

- Package is architecture-independent (all)
- Requires Python 3.10+
- Requires Qt 6 (PyQt6)
- Works on any Debian-based distribution (Ubuntu, Mint, etc.)
- Tested on KDE Plasma and GNOME desktop environments
