@echo off
REM Echo Setup Script for Windows - Single Click Install
REM Equivalent to setup.sh for Linux

setlocal EnableDelayedExpansion

echo ===============================================
echo Echo - BitChat Compatible Desktop Messaging
echo Setup Script for Windows
echo ===============================================
echo Starting Echo setup...
echo.

REM Check CMakeLists.txt
if not exist "CMakeLists.txt" (
    echo Error: CMakeLists.txt not found. Are you in the Echo project directory?
    pause
    exit /b 1
)

REM Check Git
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Git not found
    echo Please install Git from https://git-scm.com/
    pause
    exit /b 1
)
echo [OK] Git found

REM Check CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: CMake not found
    echo Please install CMake from https://cmake.org/
    pause
    exit /b 1
)
echo [OK] CMake found

REM Check Visual Studio
set "VS_FOUND="
set "VS_GEN=Visual Studio 17 2022"
for %%v in (2022 2019) do (
    for %%e in (Enterprise Professional Community) do (
        if exist "%ProgramFiles%\Microsoft Visual Studio\%%v\%%e\MSBuild\Current\Bin\MSBuild.exe" (
            set "VS_FOUND=%%v %%e"
            if "%%v"=="2019" set "VS_GEN=Visual Studio 16 2019"
            goto :vs_found
        )
    )
)

if not defined VS_FOUND (
    echo Error: Visual Studio 2019/2022 not found
    echo Please install Visual Studio with C++ development tools
    pause
    exit /b 1
)

:vs_found
echo [OK] Visual Studio !VS_FOUND! found
echo.

REM Initialize git if needed
if not exist ".git" (
    echo Initializing git repository...
    git init
    git add .
    git commit -m "Initial Echo project setup"
    echo [OK] Git repository initialized
    echo.
)

REM Setup vcpkg
echo Setting up vcpkg and dependencies...

if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\vcpkg.exe" (
        echo [OK] Using existing vcpkg at: %VCPKG_ROOT%
        set "VCPKG_EXE=%VCPKG_ROOT%\vcpkg.exe"
        goto :vcpkg_ready
    )
)

if exist "vcpkg\vcpkg.exe" (
    echo [OK] Using local vcpkg installation
    set "VCPKG_EXE=vcpkg\vcpkg.exe"
    goto :vcpkg_ready
)

REM Install vcpkg
echo Installing vcpkg...
if exist "vcpkg" rmdir /s /q "vcpkg"
git clone https://github.com/Microsoft/vcpkg.git
if %errorlevel% neq 0 (
    echo Error: Failed to clone vcpkg
    pause
    exit /b 1
)

call vcpkg\bootstrap-vcpkg.bat
if %errorlevel% neq 0 (
    echo Error: Failed to bootstrap vcpkg
    pause
    exit /b 1
)

set "VCPKG_EXE=vcpkg\vcpkg.exe"
echo [OK] vcpkg installed

:vcpkg_ready
echo.

REM Install dependencies
echo Installing dependencies (libsodium, openssl, lz4)...
echo This may take several minutes...

for %%p in (libsodium:x64-windows openssl:x64-windows lz4:x64-windows) do (
    echo   Installing %%p...
    "%VCPKG_EXE%" install %%p
)
echo [OK] Dependencies installed
echo.

REM Create directories
echo Creating directory structure...
for %%d in (
    "src\core\bluetooth" "src\core\protocol" "src\core\crypto"
    "src\core\mesh" "src\core\commands" "src\ui" "src\utils"
    "external" "tests" "docs" "build"
) do (
    if not exist "%%d" mkdir "%%d"
)
echo [OK] Directory structure created
echo.

REM Create .gitignore
if not exist ".gitignore" (
    echo Creating .gitignore...
    (
        echo # Build directories
        echo build/
        echo *build*/
        echo out/
        echo.
        echo # Visual Studio
        echo .vs/
        echo *.vcxproj.user
        echo.
        echo # vcpkg
        echo vcpkg/
        echo.
        echo # Binaries
        echo *.exe
        echo *.dll
        echo *.lib
        echo *.pdb
        echo *.obj
        echo.
        echo # Temporary
        echo *.tmp
        echo *.log
    ) > .gitignore
    echo [OK] Created .gitignore
)

REM Setup SimpleBLE
echo.
echo Setting up SimpleBLE...

git rm --cached external/simpleble >nul 2>&1
if exist "external\simpleble" rmdir /s /q "external\simpleble"
if exist ".gitmodules" del ".gitmodules"

git clone --recursive https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble
if %errorlevel% neq 0 (
    echo Error: Failed to clone SimpleBLE
    pause
    exit /b 1
)

if exist "external\simpleble\simpleble\CMakeLists.txt" (
    echo [OK] SimpleBLE setup complete!
) else if exist "external\simpleble\CMakeLists.txt" (
    echo [OK] SimpleBLE setup complete!
) else (
    echo Error: SimpleBLE setup failed
    pause
    exit /b 1
)

echo.
echo Building Echo...

cd build

REM Clean
for /d %%i in (*) do rmdir /s /q "%%i" >nul 2>&1
del /q * >nul 2>&1

REM Get vcpkg toolchain path
for %%i in ("%VCPKG_EXE%") do set "VCPKG_DIR=%%~dpi"
set "VCPKG_TOOLCHAIN=%VCPKG_DIR%scripts\buildsystems\vcpkg.cmake"

REM Configure
echo Running CMake configuration...
cmake .. -G "%VS_GEN%" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%"

if %errorlevel% neq 0 (
    echo Error: CMake configuration failed
    cd ..
    pause
    exit /b 1
)
echo [OK] CMake configuration successful

REM Build
echo Building project (this may take a few minutes)...
cmake --build . --config Release --parallel

if %errorlevel% neq 0 (
    echo Error: Build failed
    cd ..
    pause
    exit /b 1
)

echo [OK] Build successful!
cd ..

echo.
echo ===============================================
echo Setup complete! [Success]
echo ===============================================
echo.
echo Next steps:
echo 1. Run Echo: .\build\Release\echo.exe
echo 2. Test Bluetooth scanning: Type 'scan' in Echo
echo 3. Check docs\ for development guides
echo.
echo Happy coding!
echo.
pause