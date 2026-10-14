<div align="center">

# Precision Touchpad Toolkit

<p align="center">
  <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/windows8/windows8-original.svg" alt="Windows" width="48" height="48"/>
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/cplusplus/cplusplus-original.svg" alt="C++" width="48" height="48"/>
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/cmake/cmake-original.svg" alt="CMake" width="48" height="48"/>
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/opengl/opengl-original.svg" alt="OpenGL" width="48" height="48"/>
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/visualstudio/visualstudio-plain.svg" alt="Visual Studio" width="48" height="48"/>
</p>

A low-level C++17 utility suite for Windows Precision Touchpads. Directly decodes raw HID input packets for multi-touch tracking, gesture recognition, real-time GUI visualization, and background productivity shortcuts.

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6?style=for-the-badge&logo=windows&logoColor=white" alt="Windows"/>
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++17"/>
  <img src="https://img.shields.io/badge/Build-CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white" alt="CMake"/>
  <img src="https://img.shields.io/badge/Graphics-OpenGL-5586A4?style=for-the-badge&logo=opengl&logoColor=white" alt="OpenGL"/>
  <img src="https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge" alt="License: MIT"/>
</p>

</div>

---

## Features

### Raw Input Capture
- Direct raw finger data decoding from precision touchpads via Windows Raw Input and HID APIs.
- Per-finger coordinate tracking (normalized X/Y) up to 10 simultaneous contacts.
- Contact state machine tracking (`Down`, `Move`, `Up`), contact ID, and pressure strength.
- Physical click detection and polling frequencies matching hardware rates (100–300 Hz).

### Multi-Finger Gesture Engine
- **Single-finger**: Tap, double-tap, long press, drag, flick.
- **Two-finger**: Pinch-in / pinch-out, rotation, two-finger tap, horizontal/vertical scroll.
- **Three-finger**: Directional swipes (left/right/up/down) and taps.

### Real-Time GUI Visualizer
- Hardware-accelerated touchpad canvas built with Dear ImGui and OpenGL.
- Colored contact points, coordinate readouts, touch trails, and contact heatmaps.
- Live frame-rate and input-rate telemetry.

### Background Shortcut Daemon (`TouchpadShortcuts`)
- Lightweight system tray utility running without a visible console.
- Configurable corner tap actions:
  - Top-Left: Brightness Down
  - Top-Right: Brightness Up
  - Bottom-Left: Volume Down
  - Bottom-Right: Volume Up
- Right-edge vertical scroll zone with velocity smoothing and automatic edge scrolling.

---

## Tech Stack & Architecture

| Technology | Logo | Role |
|---|:---:|---|
| **Windows APIs** | <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/windows8/windows8-original.svg" width="32" height="32" alt="Windows"/> | Hardware digitizer discovery (`setupapi.lib`) and packet decoding (`hid.lib`, `user32.lib`) |
| **C++17** | <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/cplusplus/cplusplus-original.svg" width="32" height="32" alt="C++"/> | Low-latency state machine, gesture analysis, and event dispatch |
| **CMake** | <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/cmake/cmake-original.svg" width="32" height="32" alt="CMake"/> | Cross-generator build configuration and compilation workflow |
| **OpenGL & GLAD** | <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/opengl/opengl-original.svg" width="32" height="32" alt="OpenGL"/> | Hardware-accelerated canvas rendering at 60+ FPS |
| **Visual Studio** | <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/visualstudio/visualstudio-plain.svg" width="32" height="32" alt="Visual Studio"/> | MSVC compiler toolchain and native debugging |

---

## Repository Structure

```
├── CMakeLists.txt          # Root CMake configuration (TouchpadTracker suite)
├── build.ps1               # Automated build script (PowerShell)
├── build.bat               # Batch wrapper for build.ps1
├── download_deps.ps1       # Dependency downloader (ImGui, GLAD)
├── src/                    # Tracker & GUI source code
│   ├── DeviceEnumerator.*  # Windows HID & SetupAPI discovery
│   ├── TouchpadInput.*     # Raw Input registration & HID report parser
│   ├── GestureRecognizer.* # Multi-finger gesture detection
│   ├── TouchpadGUI.*       # ImGui/OpenGL visualization interface
│   ├── Logger.*            # CSV event and contact logger
│   ├── main.cpp            # Main visualizer entry point
│   ├── test_mode.cpp       # Headless CLI input test
│   └── list_devices.cpp    # Device discovery CLI
├── TouchpadShortcuts/      # Background shortcuts utility
│   ├── CMakeLists.txt      # Build configuration for shortcuts tool
│   ├── scripts/            # PowerShell helper scripts (brightness control)
│   └── src/                # Tray icon, zone detection, and system action handlers
└── logs/                   # Directory for CSV data dumps
```

---

## Requirements

- **Operating System**: Windows 10 or Windows 11
- **Hardware**: Laptop with a Windows Precision Touchpad
- **Compiler**: Visual Studio 2022 / 2026 with "Desktop development with C++"
- **Build Tools**: CMake 3.16 or higher, PowerShell 5.1+

---

## Building

### Quick Build

Run the build script from PowerShell:

```powershell
.\build.ps1
```

Common build options:

```powershell
# Clean build directory before compilation
.\build.ps1 -Clean

# Compile optimized Release binary
.\build.ps1 -Release

# Compile and launch immediately
.\build.ps1 -RunAfterBuild
```

### Manual Build

```powershell
# 1. Download vendor dependencies (ImGui and GLAD)
.\download_deps.ps1

# 2. Configure and generate build files
cmake -B build -A x64

# 3. Build Release configuration
cmake --build build --config Release
```

Build outputs are placed in `build/bin/Release/` (or `Debug/`).

---

## Applications

| Target | Type | Description |
|---|---|---|
| `TouchpadTracker.exe` | GUI (OpenGL/ImGui) | Full visualizer displaying active touch points, trails, and gestures. |
| `TouchpadShortcuts.exe` | System Tray | Silent background utility handling corner taps and edge scrolling. |
| `TouchpadTest.exe` | Console | Console stream dumping raw contact packets for calibration. |
| `ListDevices.exe` | Console | Diagnostic utility enumerating attached HID precision touch devices. |

---

## Technical Overview

### Input Pipeline
1. **Device Enumeration**: `DeviceEnumerator` queries Windows `SetupAPI` for HID devices matching the multi-touch digitizer collection (`UsagePage = 0x0D`, `Usage = 0x05`).
2. **Packet Retrieval**: Registers the window handle for `WM_INPUT` via `RegisterRawInputDevices`.
3. **HID Decoding**: Uses `HidP_GetCaps`, `HidP_GetValueCaps`, and `HidP_GetUsageValue` to extract per-contact coordinates, finger identifiers, and contact states directly from the raw byte report.
4. **Gesture Analysis**: `GestureRecognizer` maintains temporal windows of contact points to detect vector movement, velocity, and multi-contact distance changes.

---

## Dependencies

- [Dear ImGui](https://github.com/ocornut/imgui) (Docking/Viewport support via Win32 + OpenGL3)
- [GLAD](https://glad.dav1d.de/) (OpenGL 3.3 Core loader)
- Windows SDK (`hid.lib`, `setupapi.lib`, `user32.lib`, `dwmapi.lib`)

---

## License

This project is licensed under the MIT License.
