# Echo Build Script for Windows (PowerShell)
# Quick rebuild without reinstalling dependencies

param(
    [string]$Configuration = "Release",
    [switch]$Clean,
    [switch]$Run,
    [switch]$Debug
)

Write-Host "===============================================" -ForegroundColor Green
Write-Host "Echo - Quick Build Script" -ForegroundColor Green
Write-Host "===============================================" -ForegroundColor Green
Write-Host ""

# Check if we're in the right directory
if (!(Test-Path "CMakeLists.txt")) {
    Write-Host "[ERROR] CMakeLists.txt not found. Are you in the Echo project directory?" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

# Check if build directory exists
if (!(Test-Path "build")) {
    Write-Host "[ERROR] Build directory not found. Run setup.ps1 first!" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

# Set configuration
if ($Debug) {
    $Configuration = "Debug"
}

Set-Location build

# Clean if requested
if ($Clean) {
    Write-Host "[INFO] Cleaning previous build..." -ForegroundColor Cyan
    cmake --build . --target clean
}

Write-Host "[INFO] Building Echo ($Configuration)..." -ForegroundColor Cyan

# Build the project
cmake --build . --config $Configuration --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    Set-Location ..
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""
Write-Host "[SUCCESS] Build completed successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "Executable location: build\$Configuration\echo.exe" -ForegroundColor Yellow
Write-Host ""

# Return to project root
Set-Location ..

# Run if requested
if ($Run) {
    Write-Host "Starting Echo..." -ForegroundColor Cyan
    & "build\$Configuration\echo.exe"
} else {
    $response = Read-Host "Run Echo now? (Y/N)"
    if ($response -eq 'Y' -or $response -eq 'y') {
        Write-Host "Starting Echo..." -ForegroundColor Cyan
        & "build\$Configuration\echo.exe"
    }
}

if (-not $Run) {
    Read-Host "Press Enter to exit"
}