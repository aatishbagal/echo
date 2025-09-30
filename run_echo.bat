@echo off
REM Run Echo on Windows
REM Note: Windows BLE advertising requires native implementation
REM This script runs Echo without advertising for now

echo ================================================
echo Echo - Bluetooth Mesh Messaging
echo ================================================
echo.

if not exist "build\Release\echo.exe" (
    if not exist "build\Debug\echo.exe" (
        echo Error: echo.exe not found in build\Release or build\Debug
        echo Run setup.bat first to build the project
        pause
        exit /b 1
    )
    set ECHO_EXE=build\Debug\echo.exe
) else (
    set ECHO_EXE=build\Release\echo.exe
)

echo Starting Echo...
echo.
echo Note: Windows BLE advertising not yet implemented
echo Echo can scan for devices but cannot advertise itself
echo.
echo To test Echo-to-Echo communication:
echo 1. Run Echo on two Windows PCs or one Windows + one Linux
echo 2. On Linux, use: ./run_echo.sh to enable advertising
echo 3. Windows Echo will be able to see Linux Echo
echo.

%ECHO_EXE%

echo.
echo Echo stopped
pause