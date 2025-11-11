@echo off
REM Check and enable Windows Developer Mode for BLE advertising

echo ===============================================
echo Windows Developer Mode Check
echo ===============================================
echo.

echo Checking current Developer Mode status...
reg query "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock" /v AllowDevelopmentWithoutDevLicense 2>nul | find "0x1" >nul

if %errorlevel% equ 0 (
    echo [OK] Developer Mode is ENABLED
    echo Your Echo app should be able to advertise properly
    goto :end
) else (
    echo [WARNING] Developer Mode is DISABLED
    echo.
    echo This prevents proper BLE advertising on Windows 11
    echo.
    echo To enable Developer Mode:
    echo 1. Press Win + I to open Settings
    echo 2. Go to "Privacy & Security" -^> "For developers"
    echo 3. Turn ON "Developer Mode"
    echo 4. Restart Echo
    echo.
    echo OR run Echo as Administrator
    echo.
)

:end
pause
