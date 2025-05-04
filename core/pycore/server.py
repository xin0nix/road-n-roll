import subprocess
import atexit
from importlib.resources import files, as_file
from pathlib import Path
import sys


class Server:
    def __init__(self):
        is_windows = sys.platform.startswith("win")
        is_linux = sys.platform.startswith("linux")
        resource_path: str = None
        if is_windows:
            resource_path = files("pycore").joinpath(
                "../build/win-release/app/CoreApp.exe"
            )
        elif is_linux:
            resource_path = files("pycore").joinpath("../build/clang-debug/app/CoreApp")
        else:
            raise NotImplementedError(f"Unsupported operating system: {sys.platform}")

        if not Path(resource_path).exists():
            raise FileNotFoundError(
                f"Could not find application binary at: {resource_path}\n"
                f"Please ensure the application has been built for {sys.platform}"
            )

        # Convert to absolute filesystem path
        with as_file(resource_path) as exe_path:
            self.bin_path = str(exe_path.resolve())
            self.process = None

    def start(self):
        try:
            self.process = subprocess.Popen(
                [self.bin_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            atexit.register(self.stop)
        except FileNotFoundError as e:
            raise RuntimeError(
                f"Failed to start {self.bin_path}. "
                f"File exists: {Path(self.bin_path).exists()}"
            ) from e

    def stop(self):
        if self.process:
            self.process.terminate()
            self.process.wait()
