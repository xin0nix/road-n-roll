import subprocess
import atexit
from pathlib import Path
import platform


class Server:
    def __init__(self, host="127.0.0.1", port=8080):
        # Get directory of the current script file
        script_dir = Path(__file__).parent.resolve()

        # Construct path to the binary inside 'bin' relative to script_dir
        build_dir = script_dir / "bin"

        # Determine OS and set binary name accordingly
        system = platform.system()
        if system == "Windows":
            binary_name = "CoreApp.exe"
        elif system == "Linux":
            binary_name = "CoreApp"
        else:
            raise NotImplementedError(f"Unsupported operating system: {system}")

        resource_path = build_dir / binary_name

        # Check if the binary exists
        if not resource_path.exists():
            raise FileNotFoundError(
                f"Could not find application binary at: {resource_path}\n"
            )

        # Store absolute path to binary
        self.bin_path = str(resource_path)
        self.process = None
        self.host = host
        self.port = port

        # Register stop to be called at exit only once
        atexit.register(self.stop)

    def get_path(self):
        return self.bin_path

    def start(self):
        if self.process is not None:
            print(f"Process already running with PID {self.process.pid}")
            return

        try:
            self.process = subprocess.Popen(
                [self.bin_path, f"--host={self.host}", f"--port={self.port}"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            print(f"Started process {self.bin_path} with PID {self.process.pid}")
        except FileNotFoundError as e:
            raise RuntimeError(
                f"Failed to start {self.bin_path}. "
                f"File exists: {Path(self.bin_path).exists()}"
            ) from e

    def stop(self):
        if self.process:
            if self.process.poll() is None:  # Process is still running
                print(f"Stopping process PID {self.process.pid}")
                self.process.terminate()
                try:
                    stdout, stderr = self.process.communicate(timeout=10)
                except subprocess.TimeoutExpired:
                    print("Process did not terminate in time; killing it.")
                    self.process.kill()
                    stdout, stderr = self.process.communicate()
            else:
                # Process already exited
                stdout, stderr = self.process.communicate()

            self.process = None

            # Decode output safely
            stdout_decoded = stdout.decode(errors="ignore").strip() if stdout else ""
            stderr_decoded = stderr.decode(errors="ignore").strip() if stderr else ""

            # Format output with labels
            formatted_output = ""
            if stdout_decoded:
                formatted_output += f"STDOUT:\n{stdout_decoded}\n"
            if stderr_decoded:
                formatted_output += f"STDERR:\n{stderr_decoded}\n"

            return formatted_output.strip()  # remove trailing newline

        return "..."
