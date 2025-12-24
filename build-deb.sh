#!/bin/bash
# Script to build the Debian package

set -e

echo "Building Systemd Tray .deb package..."

# Clean previous builds
rm -rf debian/systemd-tray
rm -f ../*.deb ../*.buildinfo ../*.changes

# Build the package
dpkg-buildpackage -us -uc -b

echo ""
echo "Build complete!"
echo "Package file: ../systemd-tray_1.0.0_all.deb"
echo ""
echo "To install:"
echo "  sudo dpkg -i ../systemd-tray_1.0.0_all.deb"
echo "  sudo apt-get install -f  # If there are dependency issues"
