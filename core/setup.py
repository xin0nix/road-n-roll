from setuptools import setup, find_packages
from setuptools.command.install import install
import sys
import os
import subprocess

PLATFORM_DATA = {
    "win32": {"build_dir": "win-release", "binary_name": "CoreApp.exe"},
    "linux": {"build_dir": "clang-debug", "binary_name": "CoreApp"},
}


def generate_api_client():
    """Generate OpenAPI client before installation"""
    try:
        subprocess.run(
            [
                "openapi-python-client",
                "generate",
                "--path",
                "specs/openapi.yaml",
                "--output-path",
                "pycore/api",
                "--overwrite",
            ],
            check=True,
        )
    except subprocess.CalledProcessError as e:
        print(f"Failed to generate API client: {e}")
        raise


class CustomInstall(install):
    def run(self):
        generate_api_client()
        super().run()


def get_binary_path():
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
            get_binary_path(),
            "api/pyproject.toml",
            "api/.gitignore",
            "api/game_api_client/**/*.py",
            "api/game_api_client/py.typed",
        ],
    },
    include_package_data=True,
    install_requires=[
        "httpx==0.28.1",
        "pydantic==2.11.4",
    ],
    cmdclass={
        "install": CustomInstall,
    },
    setup_requires=["openapi-python-client==0.24.3"],
    has_ext_modules=lambda: True if PLATFORM_DATA.get(sys.platform) else False,
    options={
        "bdist_wheel": {
            "plat_name": {"win32": "win_amd64", "linux": "manylinux_2_39_x86_64"}.get(
                sys.platform, "any"
            )
        }
    },
    python_requires=">=3.12",
)
