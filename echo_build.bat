@echo off
REM Echo Smart Build Script - Handles setup and build intelligently
REM Usage: echo_build.bat [options]
REM Options: clean, debug, run, setup, help

setlocal EnableDelayedExpansion

REM Parse command line arguments
set "ACTION=build"
set "CONFIG=Release"
set "RUN_AFTER=0"
set "CLEAN_BUILD=0"
set "FORCE_SETUP=0"

:parse_args
if "%~1"=="" goto :end_parse
if /i "%~1"=="clean" set "CLEAN_BUILD=1"
if /i "%~1"=="debug" set "CONFIG=Debug"
if /i "%~1"=="run" set "RUN_AFTER=1"
if /i "%~1"=="setup" set "FORCE_SETUP=1"
if /i "%~1"=="help" goto :show_help
shift
goto :parse_args
:end_parse

echo ===============================================
echo Echo - Smart Build System
echo Configuration: %CONFIG%
echo ===============================================
echo.

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo [ERROR] CMakeLists.txt not found.
    echo Please run this script from the Echo project root directory.
    pause
    exit /b 1
)

REM Check if this is first run or forced setup
if not exist "build\CMakeCache.txt" set "FORCE_SETUP=1"
if not exist "external\simpleble\CMakeLists.txt" set "FORCE_SETUP=1"

if "%FORCE_SETUP%"=="1" (
    echo [INFO] Running initial setup...
    goto :run_setup
)

REM Quick build path
if "%CLEAN_BUILD%"=="1" (
    echo [INFO] Performing clean build...
    cd build
    cmake --build . --target clean 2>nul
    cd ..
)

echo [INFO] Building Echo (%CONFIG%)...
cmake --build build --config %CONFIG% --parallel

if %errorlevel% neq 0 (
    echo.
    echo [WARNING] Build failed. Checking if setup is needed...
    
    REM Check if dependencies are missing
    if not exist "build\vcpkg\installed\x64-windows" (
        echo [INFO] Dependencies missing. Running setup...
        goto :run_setup
    )
    
    echo [ERROR] Build failed. Try running: echo_build.bat setup
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Build completed!
echo Executable: build\%CONFIG%\echo.exe
goto :run_check

:run_setup
REM Full setup routine
echo.
echo Checking prerequisites...

where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Git not found. Install from: https://git-scm.com/
    pause
    exit /b 1
)

where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found. Install from: https://cmake.org/
    pause
    exit /b 1
)

REM Detect Visual Studio
set "VS_GEN="
for %%v in (2022 2019) do (
    for %%e in (Enterprise Professional Community BuildTools) do (
        if exist "%ProgramFiles%\Microsoft Visual Studio\%%v\%%e\MSBuild\Current\Bin\MSBuild.exe" (
            if "%%v"=="2022" set "VS_GEN=Visual Studio 17 2022"
            if "%%v"=="2019" set "VS_GEN=Visual Studio 16 2019"
            goto :vs_found
        )
    )
)

if not defined VS_GEN (
    echo [ERROR] Visual Studio not found
    pause
    exit /b 1
)

:vs_found
echo [OK] All prerequisites found
echo.

REM Create build directory
if not exist "build" mkdir build

REM Setup vcpkg
set "VCPKG_PATH="
if exist "build\vcpkg\vcpkg.exe" (
    echo [OK] vcpkg found in build directory
    set "VCPKG_PATH=build\vcpkg"
) else if defined VCPKG_ROOT if exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [OK] Using system vcpkg from VCPKG_ROOT
    set "VCPKG_PATH=%VCPKG_ROOT%"
) else (
    echo [INFO] Installing vcpkg...
    cd build
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    call bootstrap-vcpkg.bat
    cd ..\..
    set "VCPKG_PATH=build\vcpkg"
)

REM Install dependencies if needed
echo [INFO] Checking dependencies...
set "INSTALL_DEPS=0"
for %%d in (libsodium openssl lz4) do (
    "%VCPKG_PATH%\vcpkg" list | findstr /i "%%d:x64-windows" >nul 2>&1
    if !errorlevel! neq 0 set "INSTALL_DEPS=1"
)

if "%INSTALL_DEPS%"=="1" (
    echo [INFO] Installing dependencies...
    "%VCPKG_PATH%\vcpkg" install libsodium:x64-windows openssl:x64-windows lz4:x64-windows
)

REM Setup SimpleBLE
if not exist "external\simpleble\CMakeLists.txt" (
    echo [INFO] Setting up SimpleBLE...
    git submodule add https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble 2>nul
    git submodule update --init --recursive
)

REM Configure CMake
echo [INFO] Configuring CMake...
cd build
cmake .. -G "%VS_GEN%" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_PATH%\scripts\buildsystems\vcpkg.cmake"
cd ..

REM Build
echo [INFO] Building Echo...
cmake --build build --config %CONFIG% --parallel

if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    pause
    exit /b 1
)

echo [SUCCESS] Setup and build complete!

:run_check
if "%RUN_AFTER%"=="1" (
    echo.
    echo Starting Echo...
    start build\%CONFIG%\echo.exe
) else (
    echo.
    set /p RUN_NOW="Run Echo now? (Y/N): "
    if /i "!RUN_NOW!"=="Y" start build\%CONFIG%\echo.exe
)

echo.
pause
exit /b 0

:show_help
echo.
echo Usage: echo_build.bat [options]
echo.
echo Options:
echo   clean   - Clean before building
echo   debug   - Build debug configuration (default: Release)
echo   run     - Run Echo after building
echo   setup   - Force full setup (dependencies, cmake config)
echo   help    - Show this help message
echo.
echo Examples:
echo   echo_build.bat              - Quick rebuild
echo   echo_build.bat clean        - Clean rebuild
echo   echo_build.bat debug run    - Build debug and run
echo   echo_build.bat setup        - Full setup and build
echo.
pause
exit /b 0