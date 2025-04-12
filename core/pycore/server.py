import os
import subprocess
import atexit
import sys
from importlib.resources import files, as_file
from pathlib import Path


class Server:
    def __init__(self):
        # Get resource path (works in both development and installed package)
        resource_path = files("pycore").joinpath("../build/win-release/app/CoreApp.exe")

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
