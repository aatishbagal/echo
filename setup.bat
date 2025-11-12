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
    echo [ERROR] CMakeLists.txt not found. Are you in the Echo project directory?
    pause
    exit /b 1
)

REM Check Git
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Git not found
    echo Please install Git from https://git-scm.com/
    pause
    exit /b 1
)
echo [OK] Git found

REM Check CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found
    echo Please install CMake from https://cmake.org/
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
            set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\%%v\%%e"
            if "%%v"=="2019" set "VS_GEN=Visual Studio 16 2019"
            goto :vs_found
        )
        if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\%%v\%%e\MSBuild\Current\Bin\MSBuild.exe" (
            set "VS_FOUND=%%v %%e"
            set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\%%v\%%e"
            if "%%v"=="2019" set "VS_GEN=Visual Studio 16 2019"
            goto :vs_found
        )
    )
)

if not defined VS_FOUND (
    echo [ERROR] Visual Studio 2019/2022 not found
    echo.
    echo Please make sure Visual Studio 2022 is installed with:
    echo   - Desktop development with C++
    echo   - MSVC build tools
    echo   - Windows SDK
    echo.
    echo If installed, try:
    echo   1. Restart your command prompt as Administrator
    echo   2. Repair Visual Studio installation
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

REM Create build directory if it doesn't exist
if not exist "build" mkdir "build"

REM Setup vcpkg - check git submodule first, then build directory
echo Setting up vcpkg and dependencies...

REM Check for vcpkg as git submodule at project root
if exist "vcpkg\vcpkg.exe" (
    echo [OK] Using vcpkg from git submodule
    set "VCPKG_EXE=vcpkg\vcpkg.exe"
    set "VCPKG_ROOT=%cd%\vcpkg"
    goto :vcpkg_ready
)

REM Check if vcpkg submodule exists but not bootstrapped
if exist "vcpkg\.git" (
    if not exist "vcpkg\vcpkg.exe" (
        echo [INFO] vcpkg submodule found but not bootstrapped
        echo Bootstrapping vcpkg...
        cd vcpkg
        call bootstrap-vcpkg.bat
        if %errorlevel% neq 0 (
            echo [ERROR] Failed to bootstrap vcpkg
            cd ..
            pause
            exit /b 1
        )
        cd ..
        set "VCPKG_EXE=vcpkg\vcpkg.exe"
        set "VCPKG_ROOT=%cd%\vcpkg"
        goto :vcpkg_ready
    )
)

REM Check for vcpkg in build directory (legacy location)
if exist "build\vcpkg\vcpkg.exe" (
    echo [OK] Using local vcpkg installation in build directory
    set "VCPKG_EXE=build\vcpkg\vcpkg.exe"
    set "VCPKG_ROOT=%cd%\build\vcpkg"
    goto :vcpkg_ready
)

REM Check environment variable
if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\vcpkg.exe" (
        echo [OK] Using existing vcpkg at: %VCPKG_ROOT%
        set "VCPKG_EXE=%VCPKG_ROOT%\vcpkg.exe"
        goto :vcpkg_ready
    )
)

REM Install vcpkg in build directory (fallback)
echo Installing vcpkg in build directory...
if exist "build\vcpkg" rmdir /s /q "build\vcpkg"

cd build
git clone https://github.com/Microsoft/vcpkg.git
if %errorlevel% neq 0 (
    echo [ERROR] Failed to clone vcpkg
    cd ..
    pause
    exit /b 1
)

cd vcpkg
call bootstrap-vcpkg.bat
if %errorlevel% neq 0 (
    echo [ERROR] Failed to bootstrap vcpkg
    cd ..\..
    pause
    exit /b 1
)

cd ..\..
set "VCPKG_EXE=build\vcpkg\vcpkg.exe"
set "VCPKG_ROOT=%cd%\build\vcpkg"
echo [OK] vcpkg installed at: %VCPKG_ROOT%

:vcpkg_ready
echo.

REM Install dependencies
echo Installing dependencies (libsodium, openssl, lz4)...
echo This may take several minutes...

for %%p in (libsodium:x64-windows openssl:x64-windows lz4:x64-windows python3:x64-windows) do (
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
    "external" "tests" "docs"
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
    echo [ERROR] Failed to clone SimpleBLE
    pause
    exit /b 1
)

if exist "external\simpleble\simpleble\CMakeLists.txt" (
    echo [OK] SimpleBLE setup complete!
) else if exist "external\simpleble\CMakeLists.txt" (
    echo [OK] SimpleBLE setup complete!
) else (
    echo [ERROR] SimpleBLE setup failed
    pause
    exit /b 1
)

echo.
echo Building Echo...

cd build

REM Clean build files but keep vcpkg
echo Cleaning previous build (preserving vcpkg)...
for /d %%i in (*) do (
    if not "%%i"=="vcpkg" rmdir /s /q "%%i" >nul 2>&1
)
for %%i in (*) do (
    if not "%%~nxi"=="vcpkg" del /q "%%i" >nul 2>&1
)

REM Get vcpkg toolchain path
set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

if not exist "%VCPKG_TOOLCHAIN%" (
    echo [ERROR] vcpkg toolchain not found at: %VCPKG_TOOLCHAIN%
    cd ..
    pause
    exit /b 1
)

echo [OK] vcpkg toolchain found at: %VCPKG_TOOLCHAIN%
echo.

REM Configure
echo Running CMake configuration...
echo Configuration details:
echo   vcpkg root: %VCPKG_ROOT%
echo   vcpkg toolchain: %VCPKG_TOOLCHAIN%
echo   Generator: %VS_GEN%
echo.

cmake .. -G "%VS_GEN%" -A x64 -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%"

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    cd ..
    pause
    exit /b 1
)
echo [OK] CMake configuration successful

REM Build
echo Building project (this may take a few minutes)...
cmake --build . --config Release --parallel

if %errorlevel% neq 0 (
    echo [ERROR] Build failed
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