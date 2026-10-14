# Build Instructions

This document provides detailed instructions for building the TouchpadTracker application.

## Prerequisites

### 1. Install Visual Studio

Download and install Visual Studio 2026 (or 2022) from:
https://visualstudio.microsoft.com/

During installation, select the **"Desktop development with C++"** workload.

### 2. Install CMake

**Option A: Visual Studio Installer**
1. Open Visual Studio Installer
2. Click "Modify" on your VS installation
3. Go to "Individual Components"
4. Search for "CMake" and check "CMake tools for Windows"
5. Click "Modify" to install

**Option B: Standalone Installer**
1. Download CMake from: https://cmake.org/download/
2. Choose "Windows x64 Installer"
3. During installation, select "Add CMake to the system PATH"

**Option C: winget (Windows Package Manager)**
```powershell
winget install Kitware.CMake
```

**Option D: Chocolatey**
```powershell
choco install cmake
```

After installing CMake, restart your terminal/PowerShell.

### 3. Verify Installation

```powershell
cmake --version
```

Should output something like: `cmake version 3.28.0`

## Building the Project

### Quick Build

```powershell
cd "d:\CS\CS_3.0\touchpad c++"
.\build.ps1
```

### Step-by-Step Build

```powershell
# Navigate to project directory
cd "d:\CS\CS_3.0\touchpad c++"

# Download dependencies (if not already done)
.\download_deps.ps1

# Create and enter build directory
mkdir build -ErrorAction SilentlyContinue
cd build

# Configure with CMake
cmake .. -G "Visual Studio 18 2026" -A x64

# Build Debug version
cmake --build . --config Debug

# OR Build Release version
cmake --build . --config Release
```

### Build Output

Executables will be in:
- Debug: `build\bin\Debug\`
- Release: `build\bin\Release\`

## Alternative: Visual Studio IDE

1. Open Visual Studio
2. Choose "Open a local folder"
3. Select the `touchpad c++` folder
4. Visual Studio will detect the CMakeLists.txt
5. Select configuration (x64-Debug or x64-Release)
6. Build > Build All (Ctrl+Shift+B)

## Alternative: Using Developer Command Prompt

If CMake is not in your regular PATH but Visual Studio is installed:

1. Open "Developer Command Prompt for VS 2026" from Start menu
2. Navigate to project directory
3. Run build commands:

```cmd
cd /d "d:\CS\CS_3.0\touchpad c++"
mkdir build
cd build
cmake .. -G "Visual Studio 18 2026" -A x64
cmake --build . --config Release
```

## Troubleshooting

### "cmake is not recognized"

1. Ensure CMake is installed
2. Close and reopen PowerShell/terminal
3. If using VS CMake, open "Developer PowerShell for VS 2026" instead

### "No generator found"

You may not have the right Visual Studio version. Try:
```powershell
cmake .. -G "Visual Studio 17 2022" -A x64  # For VS 2022
cmake .. -G "Visual Studio 16 2019" -A x64  # For VS 2019
```

### Link errors with HID/SetupAPI

Ensure Windows SDK is installed with Visual Studio.

### ImGui/GLAD files not found

Run the dependency download script:
```powershell
.\download_deps.ps1
```

## Running the Application

After successful build:

```powershell
# From project root
.\build\bin\Release\TouchpadTracker.exe

# Or for test mode
.\build\bin\Release\TouchpadTest.exe

# Or to list devices
.\build\bin\Release\ListDevices.exe
```

**Note**: You may need to run as Administrator for touchpad access on some systems.
