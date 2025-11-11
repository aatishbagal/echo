@echo off
REM Quick build without vcpkg OpenSSL dependency
REM Uses system OpenSSL if available

setlocal EnableDelayedExpansion

echo ===============================================
echo Echo - Quick Build (No vcpkg OpenSSL)
echo ===============================================
echo.

if not exist "build" mkdir "build"
cd build

REM Try to find OpenSSL manually
set "OPENSSL_FOUND=0"

REM Check common OpenSSL installation locations
for %%p in (
    "C:\Program Files\OpenSSL-Win64"
    "C:\Program Files\OpenSSL"
    "C:\OpenSSL-Win64"
    "%ProgramFiles%\OpenSSL-Win64"
) do (
    if exist "%%~p\include\openssl\ssl.h" (
        set "OPENSSL_ROOT_DIR=%%~p"
        set "OPENSSL_FOUND=1"
        echo [OK] Found OpenSSL at: %%~p
        goto :openssl_found
    )
)

if "!OPENSSL_FOUND!"=="0" (
    echo [WARNING] OpenSSL not found in common locations
    echo.
    echo Please install OpenSSL:
    echo 1. Download from: https://slproweb.com/products/Win32OpenSSL.html
    echo 2. Install "Win64 OpenSSL v3.x.x" (NOT Light version)
    echo 3. Run this script again
    echo.
    echo Or install PowerShell 7:
    echo    winget install Microsoft.PowerShell
    echo Then run setup.bat
    pause
    cd ..
    exit /b 1
)

:openssl_found

echo.
echo Configuring without vcpkg...
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DOPENSSL_ROOT_DIR="!OPENSSL_ROOT_DIR!"

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    cd ..
    pause
    exit /b 1
)

echo.
echo Building...
cmake --build . --config Release --parallel

if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ===============================================
echo Build complete!
echo ===============================================
echo.
echo Executable: build\Release\echo.exe
echo.

pause
