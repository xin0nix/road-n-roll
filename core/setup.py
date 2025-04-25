from setuptools import setup, find_packages
import sys
import os

# Platform-specific configuration
PLATFORM_DATA = {
    "win32": {"build_dir": "win-release", "binary_name": "CoreApp.exe"},
    "linux": {"build_dir": "clang-debug", "binary_name": "CoreApp"},
}


def get_binary_path():
    """Get the correct binary path for the current platform"""
    platform_config = PLATFORM_DATA.get(sys.platform)
    if not platform_config:
        raise RuntimeError(f"Unsupported platform: {sys.platform}")

    return os.path.join(
        "..",
        "build",
        platform_config["build_dir"],
        "app",
        platform_config["binary_name"],
    )


setup(
    name="pycore",
    version="0.0.1",
    packages=find_packages(),
    package_data={
        "pycore": [
            get_binary_path(),  # Platform-specific binary
        ],
    },
    include_package_data=True,
    # Force platform-specific wheels when binary exists
    has_ext_modules=lambda: True if PLATFORM_DATA.get(sys.platform) else False,
    options={
        "bdist_wheel": {
            "plat_name": {"win32": "win_amd64", "linux": "manylinux_2_28"}.get(
                sys.platform, "any"
            )
        }
    },
    python_requires=">=3.12",
)
