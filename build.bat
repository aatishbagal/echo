@echo off
REM Echo Build Script for Windows
REM Quick rebuild without reinstalling dependencies

echo ===============================================
echo Echo - Quick Build Script
echo ===============================================
echo.

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo [ERROR] CMakeLists.txt not found. Are you in the Echo project directory?
    pause
    exit /b 1
)

REM Check if build directory exists
if not exist "build" (
    echo [ERROR] Build directory not found. Run setup.bat first!
    pause
    exit /b 1
)

REM Navigate to build directory
cd build

REM Check if vcpkg is already set up
if not exist "vcpkg\vcpkg.exe" (
    if not defined VCPKG_ROOT (
        echo [WARNING] vcpkg not found in build directory and VCPKG_ROOT not set
        echo [WARNING] You may need to run setup.bat first
    )
)

echo [INFO] Building Echo...

REM Build the project (Release by default)
cmake --build . --config Release --parallel

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Build completed successfully!
echo.
echo Executable location: build\Release\echo.exe
echo.

REM Return to project root
cd ..

REM Ask if user wants to run the program
set /p RUN_ECHO="Run Echo now? (Y/N): "
if /i "%RUN_ECHO%"=="Y" (
    echo Starting Echo...
    start build\Release\echo.exe
)

pause