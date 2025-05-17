## Build and Installation

### Prerequisites
- CMake 3.28.3 or later
- Python 3.12
- C++23 compatible compiler (Clang recommended for Linux, MSVC for Windows)
- Boost 1.85.0 (automatically downloaded during build)

### Build Process
Prerequisites:
```sh
sudo snap install ruff
```

1. Configure the project:
```sh
cmake --preset=clang-debug
```

2. Build the project and generate Python wheel:
```sh
cmake --build build/clang-debug
```

3. The generated wheel will be located at (example path):
```sh
dist/pycore-0.0.1-cp312-cp312-manylinux_2_28.whl
```

### Installation
Install the wheel in your Python environment:
```sh
pip install dist/pycore-0.0.1-cp312-cp312-manylinux_2_28.whl
```

### Verification
Test the installation:
```python
import pycore
server = pycore.Server()
server.start()
print("Server running successfully!")
```

### Key Features
- Automatic virtual environment creation
- Dependency installation from requirements.txt
- manylinux_2_28 compatible wheel generation
- Integration with CoreApp build process

### Notes
- The build process automatically handles:
  - Python virtual environment creation
  - Required dependency installation
  - CoreApp executable building
  - Python wheel package generation
- Windows users should replace `clang` with their preferred MSVC compiler

### Windows Build Instructions
1. Install MSVC
2. Activate the development environment in PowerShell:
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
```
To avoid activating the environment each time, you can add this command to your PowerShell profile (with slower PowerShell startup as a trade-off):
```powershell
if (!(Test-Path $PROFILE)) { New-Item -ItemType File -Path $PROFILE -Force }
notepad $PROFILE
```
3. Configure the project using the Windows preset:
```sh
cmake --preset=win-release
```
4. Build the project:
```sh
cmake --build build/win-release
```