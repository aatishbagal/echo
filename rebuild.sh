#!/bin/bash

# Echo - Clean Rebuild Script for Linux/macOS/Windows (Git Bash)
# Deletes all build files and rebuilds from scratch

set -e

CONFIG="${1:-Release}"

# Detect operating system
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" || "$OSTYPE" == "cygwin" ]]; then
    IS_WINDOWS=true
else
    IS_WINDOWS=false
fi

echo "==============================================="
echo "Echo - Clean Rebuild"
echo "Configuration: $CONFIG"
if [ "$IS_WINDOWS" = true ]; then
    echo "Platform: Windows (Git Bash)"
    echo "NOTE: For best results on Windows, use rebuild.bat instead"
fi
echo "==============================================="
echo

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "[ERROR] CMakeLists.txt not found"
    echo "Run this script from the Echo project root"
    exit 1
fi

# Remove build directory if it exists
if [ -d "build" ]; then
    echo "[INFO] Removing previous build directory..."
    rm -rf build
    echo "[OK] Build directory cleaned"
else
    echo "[INFO] No existing build directory found"
fi

# Create fresh build directory
mkdir -p build
echo "[OK] Created new build directory"
echo

# Navigate to build directory
cd build

# Run CMake configuration
echo "[INFO] Configuring CMake..."

if [ "$IS_WINDOWS" = true ]; then
    # Windows with vcpkg
    if [ -z "$VCPKG_ROOT" ]; then
        if [ -f "vcpkg/vcpkg.exe" ]; then
            VCPKG_ROOT="$(pwd)/vcpkg"
            export VCPKG_ROOT
        elif [ -d "../vcpkg" ]; then
            VCPKG_ROOT="$(cd ../vcpkg && pwd)"
            export VCPKG_ROOT
        else
            echo "[ERROR] vcpkg not found. Please run setup.bat first"
            cd ..
            exit 1
        fi
    fi
    
    VCPKG_TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    
    # Detect Visual Studio
    VS_GEN="Visual Studio 17 2022"
    if [ -d "/c/Program Files/Microsoft Visual Studio/2019" ]; then
        VS_GEN="Visual Studio 16 2019"
    fi
    
    cmake .. -G "$VS_GEN" -A x64 -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN"
else
    # Linux/macOS
    if [ "$CONFIG" = "Debug" ]; then
        cmake .. -DCMAKE_BUILD_TYPE=Debug
    else
        cmake .. -DCMAKE_BUILD_TYPE=Release
    fi
fi

if [ $? -ne 0 ]; then
    echo "[ERROR] CMake configuration failed"
    cd ..
    exit 1
fi

echo "[OK] Configuration complete"
echo

# Build
echo "[INFO] Building Echo ($CONFIG)..."

if [ "$IS_WINDOWS" = true ]; then
    # Windows: Use cmake --build
    cmake --build . --config $CONFIG --parallel
else
    # Linux/macOS: Use make
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

if [ $? -ne 0 ]; then
    echo "[ERROR] Build failed"
    cd ..
    exit 1
fi

cd ..

echo
echo "==============================================="
echo "[SUCCESS] Clean rebuild complete!"
echo "==============================================="
echo

if [ "$IS_WINDOWS" = true ]; then
    echo "Executable: build/$CONFIG/echo.exe"
else
    echo "Executable: build/echo"
fi
echo

# Ask if user wants to run
read -p "Run Echo now? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if [ "$IS_WINDOWS" = true ]; then
        if [ -f "run_echo.bat" ]; then
            cmd //c run_echo.bat
        else
            ./build/$CONFIG/echo.exe
        fi
    else
        if [ -f "run_echo.sh" ]; then
            ./run_echo.sh
        else
            ./build/echo
        fi
    fi
fi