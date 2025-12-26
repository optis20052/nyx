#!/usr/bin/env python3
"""Setup script for NyxApp."""

from setuptools import setup, find_packages
from pathlib import Path

# Read the long description from README
README = Path("README.md")
long_description = README.read_text() if README.exists() else ""

# Read version from package
version = "1.0.0"

setup(
    name="nyxapp",
    version=version,
    description="NyxApp - Manage systemd services from the system tray",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="Ali",
    author_email="",
    url="https://github.com/yourusername/nyxapp",
    license="MIT",
    packages=find_packages(),
    package_data={
        "nyxapp": [
            "resources/icons/*.svg",
            "resources/icons/*.png",
        ],
    },
    include_package_data=True,
    install_requires=[
        "PyQt6>=6.4.0",
        "PyYAML>=6.0",
    ],
    entry_points={
        "console_scripts": [
            "nyxapp=nyxapp.main:main",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: End Users/Desktop",
        "License :: OSI Approved :: MIT License",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Topic :: System :: Monitoring",
        "Environment :: X11 Applications :: Qt",
    ],
    python_requires=">=3.10",
    data_files=[
        ("share/applications", ["nyxapp.desktop"]),
        ("share/icons/hicolor/scalable/apps", [
            "nyxapp/resources/icons/nyxapp.svg",
            "nyxapp/resources/icons/nyxapp-symbolic.svg"
        ]),
    ],
)
