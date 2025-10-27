@echo off
REM Echo Setup Script for Windows - Optimized Version
REM Only installs dependencies if not already present

setlocal EnableDelayedExpansion

echo ===============================================
echo Echo - Optimized Setup Script
echo ===============================================
echo.

REM Check prerequisites
echo Checking prerequisites...

if not exist "CMakeLists.txt" (
    echo [ERROR] CMakeLists.txt not found. Are you in the Echo project directory?
    pause
    exit /b 1
)

where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Git not found. Please install from https://git-scm.com/
    pause
    exit /b 1
)
echo [OK] Git found

where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found. Please install from https://cmake.org/
    pause
    exit /b 1
)
echo [OK] CMake found

REM Check Visual Studio
set "VS_FOUND="
set "VS_GEN=Visual Studio 17 2022"
for %%v in (2022 2019) do (
    for %%e in (Enterprise Professional Community BuildTools) do (
        if exist "%ProgramFiles%\Microsoft Visual Studio\%%v\%%e\MSBuild\Current\Bin\MSBuild.exe" (
            set "VS_FOUND=%%v %%e"
            if "%%v"=="2019" set "VS_GEN=Visual Studio 16 2019"
            goto :vs_found
        )
    )
)

if not defined VS_FOUND (
    echo [ERROR] Visual Studio 2019/2022 not found
    echo Please install Visual Studio with C++ development tools
    pause
    exit /b 1
)

:vs_found
echo [OK] Visual Studio !VS_FOUND! found
echo.

REM Create build directory
if not exist "build" mkdir build

REM Check if vcpkg is already set up
set "VCPKG_CONFIGURED=0"
set "VCPKG_PATH="
set "VCPKG_ROOT_PATH="

REM Check in build directory first
if exist "build\vcpkg\vcpkg.exe" (
    echo [OK] Found vcpkg in build directory
    set "VCPKG_PATH=build\vcpkg\vcpkg.exe"
    set "VCPKG_ROOT_PATH=build\vcpkg"
    set "VCPKG_CONFIGURED=1"
    goto :check_deps
)

REM Check environment variable
if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\vcpkg.exe" (
        echo [OK] Using vcpkg from VCPKG_ROOT: %VCPKG_ROOT%
        set "VCPKG_PATH=%VCPKG_ROOT%\vcpkg.exe"
        set "VCPKG_ROOT_PATH=%VCPKG_ROOT%"
        set "VCPKG_CONFIGURED=1"
        goto :check_deps
    )
)

REM Install vcpkg if not found
echo [INFO] vcpkg not found. Installing...
cd build
if exist "vcpkg" rmdir /s /q "vcpkg"
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
call bootstrap-vcpkg.bat
cd ..\..
set "VCPKG_PATH=build\vcpkg\vcpkg.exe"
set "VCPKG_ROOT_PATH=build\vcpkg"
echo [OK] vcpkg installed

:check_deps
REM Check if dependencies are already installed
echo.
echo Checking dependencies...

set "DEPS_NEEDED=0"
for %%d in (libsodium openssl lz4) do (
    "%VCPKG_PATH%" list | findstr /i "%%d:x64-windows" >nul 2>&1
    if !errorlevel! neq 0 (
        echo [INFO] Missing dependency: %%d
        set "DEPS_NEEDED=1"
    ) else (
        echo [OK] Found: %%d
    )
)

if "%DEPS_NEEDED%"=="1" (
    echo.
    echo Installing missing dependencies...
    "%VCPKG_PATH%" install libsodium:x64-windows openssl:x64-windows lz4:x64-windows
    echo [OK] Dependencies installed
) else (
    echo [OK] All dependencies already installed
)

REM Check SimpleBLE
echo.
echo Checking SimpleBLE...
if exist "external\simpleble\CMakeLists.txt" (
    echo [OK] SimpleBLE already set up
) else (
    echo [INFO] Setting up SimpleBLE...
    if not exist "external" mkdir external
    git submodule add https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble 2>nul
    git submodule update --init --recursive
    echo [OK] SimpleBLE configured
)

REM Configure CMake if needed
echo.
cd build

if exist "CMakeCache.txt" (
    echo [OK] CMake already configured
    set /p RECONFIGURE="Reconfigure CMake? (Y/N): "
    if /i "!RECONFIGURE!"=="Y" (
        del CMakeCache.txt
        goto :configure
    )
) else (
    goto :configure
)
goto :build

:configure
echo [INFO] Configuring CMake...
cmake .. -G "%VS_GEN%" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT_PATH%\scripts\buildsystems\vcpkg.cmake"
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    cd ..
    pause
    exit /b 1
)
echo [OK] CMake configured

:build
echo.
set /p BUILD_NOW="Build Echo now? (Y/N): "
if /i "%BUILD_NOW%"=="Y" (
    echo [INFO] Building Echo...
    cmake --build . --config Release --parallel
    if !errorlevel! neq 0 (
        echo [ERROR] Build failed
        cd ..
        pause
        exit /b 1
    )
    echo [OK] Build successful!
)

cd ..

echo.
echo ===============================================
echo Setup complete!
echo ===============================================
echo.
echo To build Echo:
echo   - Quick build: run build.bat
echo   - Full rebuild: run "cmake --build build --config Release" 
echo   - Debug build: run "cmake --build build --config Debug"
echo.
echo Executable location: build\Release\echo.exe
echo.
pause