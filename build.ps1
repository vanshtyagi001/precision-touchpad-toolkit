# Build script for TouchpadTracker

param(
    [switch]$Clean,
    [switch]$Release,
    [switch]$RunAfterBuild
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   TouchpadTracker Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$projectRoot = $PSScriptRoot
$buildDir = Join-Path $projectRoot "build"
$buildType = if ($Release) { "Release" } else { "Debug" }

# Check for CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host "CMake not found! Please install CMake and add it to PATH." -ForegroundColor Red
    exit 1
}

# Check for Visual Studio / MSVC
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    Write-Host "Visual Studio not found! Please install Visual Studio with C++ workload." -ForegroundColor Red
    exit 1
}

# Clean build directory if requested
if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

# Create build directory
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Download dependencies if needed
$imguiDir = Join-Path $projectRoot "external\imgui"
$gladDir = Join-Path $projectRoot "external\glad"

if (-not (Test-Path (Join-Path $imguiDir "imgui.h"))) {
    Write-Host "ImGui not found. Downloading dependencies..." -ForegroundColor Yellow
    & (Join-Path $projectRoot "download_deps.ps1")
}

# Make sure GLAD source is compiled
$gladSource = Join-Path $gladDir "glad.c"
if (-not (Test-Path $gladSource)) {
    Write-Host "GLAD source not found. Downloading dependencies..." -ForegroundColor Yellow
    & (Join-Path $projectRoot "download_deps.ps1")
}

# Configure with CMake
Write-Host "Configuring with CMake..." -ForegroundColor Yellow
Push-Location $buildDir

try {
    cmake .. -G "Visual Studio 18 2026" -A x64
    
    if ($LASTEXITCODE -ne 0) {
        # Try VS 2022
        cmake .. -G "Visual Studio 17 2022" -A x64
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configuration failed!" -ForegroundColor Red
        exit 1
    }
    
    # Build
    Write-Host "Building ($buildType)..." -ForegroundColor Yellow
    cmake --build . --config $buildType --parallel
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Executables are in: $buildDir\bin\$buildType" -ForegroundColor Cyan
    
    $binDir = Join-Path $buildDir "bin\$buildType"
    
    Write-Host ""
    Write-Host "Available executables:" -ForegroundColor Cyan
    Write-Host "  - TouchpadTracker.exe  : Main GUI application" -ForegroundColor White
    Write-Host "  - TouchpadTest.exe     : Console test mode" -ForegroundColor White
    Write-Host "  - ListDevices.exe      : List touch devices" -ForegroundColor White
    
    if ($RunAfterBuild) {
        Write-Host ""
        Write-Host "Starting TouchpadTracker..." -ForegroundColor Yellow
        $exe = Join-Path $binDir "TouchpadTracker.exe"
        if (Test-Path $exe) {
            Start-Process $exe
        }
    }
    
} finally {
    Pop-Location
}
