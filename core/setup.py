"""Setup script providing CoreApp binary package."""

import sys
from setuptools import setup


def get_binary_relative_path():
    """Binary should be copied into pycore/bin/CoreApp or CoreApp.exe"""
    if sys.platform.startswith("win"):
        return "bin/CoreApp.exe"
    elif sys.platform.startswith("linux"):
        return "bin/CoreApp"
    else:
        raise NotImplementedError(f"Unsupported OS: {sys.platform}")


setup(
    name="pycore",
    version="0.0.1",
    packages=["pycore"],
    package_data={
        "pycore": [get_binary_relative_path()],
    },
    include_package_data=True,
    has_ext_modules=lambda: True,
    options={
        "bdist_wheel": {
            "plat_name": {
                "win32": "win_amd64",
                "linux": "manylinux_2_39_x86_64",
            }.get(sys.platform, "any")
        }
    },
    python_requires=">=3.12",
)
