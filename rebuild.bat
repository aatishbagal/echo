@echo off
REM Echo - Clean Rebuild Script
REM Deletes all build files and rebuilds from scratch

setlocal EnableDelayedExpansion

set "CONFIG=Release"
if /i "%~1"=="debug" set "CONFIG=Debug"

echo ===============================================
echo Echo - Clean Rebuild
echo Configuration: %CONFIG%
echo ===============================================
echo.

if not exist "CMakeLists.txt" (
    echo [ERROR] CMakeLists.txt not found
    echo Run this script from the Echo project root
    pause
    exit /b 1
)

REM Check if build directory exists
if exist "build" (
    echo [INFO] Removing previous build directory...
    
    REM Save vcpkg if it exists
    set "VCPKG_EXISTS=0"
    if exist "build\vcpkg" (
        echo [INFO] Preserving vcpkg installation...
        set "VCPKG_EXISTS=1"
        move "build\vcpkg" "vcpkg_temp" >nul 2>&1
    )
    
    REM Delete build directory
    rmdir /s /q "build" >nul 2>&1
    if exist "build" (
        echo [WARNING] Could not delete build directory, trying harder...
        timeout /t 2 /nobreak >nul
        rd /s /q "build" 2>nul
    )
    
    REM Recreate build directory
    mkdir "build"
    
    REM Restore vcpkg
    if "!VCPKG_EXISTS!"=="1" (
        move "vcpkg_temp" "build\vcpkg" >nul 2>&1
        echo [OK] vcpkg preserved
    )
    
    echo [OK] Build directory cleaned
) else (
    echo [INFO] No existing build directory found
    mkdir "build"
)

echo.

REM Get vcpkg path
set "VCPKG_ROOT="
if exist "vcpkg\vcpkg.exe" (
    set "VCPKG_ROOT=%cd%\vcpkg"
    echo [OK] Using project vcpkg
) else if exist "build\vcpkg\vcpkg.exe" (
    set "VCPKG_ROOT=%cd%\build\vcpkg"
    echo [OK] Using local vcpkg
) else if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\vcpkg.exe" (
        echo [OK] Using system vcpkg from %VCPKG_ROOT%
    ) else (
        echo [ERROR] VCPKG_ROOT set but vcpkg.exe not found at: %VCPKG_ROOT%
        echo Run setup.bat to install dependencies
        pause
        exit /b 1
    )
) else (
    echo [ERROR] vcpkg not found
    echo Expected locations:
    echo   - %cd%\vcpkg\vcpkg.exe
    echo   - %cd%\build\vcpkg\vcpkg.exe
    echo   - VCPKG_ROOT environment variable
    echo.
    echo Run setup.bat first to install dependencies
    pause
    exit /b 1
)

REM Detect Visual Studio
set "VS_GEN=Visual Studio 17 2022"
for %%v in (2022 2019) do (
    for %%e in (Enterprise Professional Community BuildTools) do (
        if exist "%ProgramFiles%\Microsoft Visual Studio\%%v\%%e\MSBuild\Current\Bin\MSBuild.exe" (
            if "%%v"=="2019" set "VS_GEN=Visual Studio 16 2019"
            goto :vs_found
        )
    )
)

echo [ERROR] Visual Studio not found
pause
exit /b 1

:vs_found
echo [OK] Visual Studio found
echo.

REM Configure CMake
echo [INFO] Configuring CMake...
cd build

set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
cmake .. -G "%VS_GEN%" -A x64 -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%"

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    cd ..
    pause
    exit /b 1
)

echo [OK] Configuration complete
echo.

REM Build
echo [INFO] Building Echo (%CONFIG%)...
cmake --build . --config %CONFIG% --parallel

if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ===============================================
echo [SUCCESS] Clean rebuild complete!
echo ===============================================
echo.
echo Executable: build\%CONFIG%\echo.exe
echo.

set /p RUN_NOW="Run Echo now? (Y/N): "
if /i "%RUN_NOW%"=="Y" (
    start build\%CONFIG%\echo.exe
)

pause