@echo off
REM Build script for TouchpadTracker (Command Prompt version)
REM Run this from "Developer Command Prompt for VS 2026"

echo ========================================
echo    TouchpadTracker Build Script
echo ========================================
echo.

cd /d "%~dp0"

REM Check for CMake
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo CMake not found!
    echo Please install CMake or run from Developer Command Prompt
    pause
    exit /b 1
)

REM Create build directory
if not exist build mkdir build
cd build

REM Try VS 2026 first, then 2022
echo Configuring with CMake...
cmake .. -G "Visual Studio 18 2026" -A x64
if %ERRORLEVEL% neq 0 (
    cmake .. -G "Visual Studio 17 2022" -A x64
)

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build
echo.
echo Building Release configuration...
cmake --build . --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build successful!
echo.
echo Executables are in: %cd%\bin\Release
echo.
echo - TouchpadTracker.exe : Main GUI application
echo - TouchpadTest.exe    : Console test mode
echo - ListDevices.exe     : List touch devices
echo ========================================
echo.

pause
