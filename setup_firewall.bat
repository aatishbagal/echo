@echo off
echo ================================================
echo Echo WiFi - Windows Firewall Configuration
echo ================================================
echo.
echo This script will add firewall rules for Echo WiFi messaging
echo You need to run this as Administrator
echo.
pause

echo.
echo Adding firewall rules...
echo.

REM Inbound UDP 48270 (Discovery)
netsh advfirewall firewall add rule name="Echo WiFi Discovery IN" dir=in action=allow protocol=UDP localport=48270 profile=private,public
if %errorlevel% equ 0 (
    echo [OK] Inbound UDP 48270 - Discovery
) else (
    echo [FAIL] Inbound UDP 48270
)

REM Outbound UDP 48270 (Discovery)
netsh advfirewall firewall add rule name="Echo WiFi Discovery OUT" dir=out action=allow protocol=UDP localport=48270 profile=private,public
if %errorlevel% equ 0 (
    echo [OK] Outbound UDP 48270 - Discovery
) else (
    echo [FAIL] Outbound UDP 48270
)

REM Inbound TCP 48271 (Messaging)
netsh advfirewall firewall add rule name="Echo WiFi Messaging IN" dir=in action=allow protocol=TCP localport=48271 profile=private,public
if %errorlevel% equ 0 (
    echo [OK] Inbound TCP 48271 - Messaging
) else (
    echo [FAIL] Inbound TCP 48271
)

REM Outbound TCP 48271 (Messaging)
netsh advfirewall firewall add rule name="Echo WiFi Messaging OUT" dir=out action=allow protocol=TCP localport=48271 profile=private,public
if %errorlevel% equ 0 (
    echo [OK] Outbound TCP 48271 - Messaging
) else (
    echo [FAIL] Outbound TCP 48271
)

echo.
echo ================================================
echo Configuration complete!
echo ================================================
echo.
echo Firewall rules added:
netsh advfirewall firewall show rule name="Echo WiFi Discovery IN"
netsh advfirewall firewall show rule name="Echo WiFi Discovery OUT"
netsh advfirewall firewall show rule name="Echo WiFi Messaging IN"
netsh advfirewall firewall show rule name="Echo WiFi Messaging OUT"

echo.
echo You can now run Echo and use WiFi messaging
echo.
pause