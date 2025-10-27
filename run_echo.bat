@echo off
setlocal EnableDelayedExpansion

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

if not exist "echo_identity.dat" (
    echo Generating identity...
    timeout /t 2 /nobreak ^>nul
    %ECHO_EXE%
    timeout /t 1 /nobreak ^>nul
    taskkill /f /im echo.exe ^>nul 2^>^&1
)

if not exist "echo_identity.dat" (
    echo Error: Failed to generate identity file
    pause
    exit /b 1
)

echo Reading identity from echo_identity.dat...

for /f "tokens=1,2 delims=|" %%a in ('python read_identity.py 2^>nul') do (
    set USERNAME=%%a
    set PEER_ID=%%b
)

if not defined USERNAME (
    echo Error: Failed to read identity file
    echo Make sure Python 3 is installed
    pause
    exit /b 1
)

echo   Username: !USERNAME!
echo   Peer ID: !PEER_ID!
echo.

echo Note: Windows BLE advertising not yet implemented
echo Echo can scan for devices but cannot advertise itself
echo.

%ECHO_EXE%

echo.
echo Echo stopped
pause