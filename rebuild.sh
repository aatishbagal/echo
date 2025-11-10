#!/bin/bash

# Echo - Clean Rebuild Script for Linux
# Deletes all build files and rebuilds from scratch

set -e

CONFIG="${1:-Release}"

echo "==============================================="
echo "Echo - Clean Rebuild"
echo "Configuration: $CONFIG"
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
if [ "$CONFIG" = "Debug" ]; then
    cmake .. -DCMAKE_BUILD_TYPE=Debug
else
    cmake .. -DCMAKE_BUILD_TYPE=Release
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
make -j$(nproc)

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
echo "Executable: build/echo"
echo

# Ask if user wants to run
read -p "Run Echo now? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if [ -f "run_echo.sh" ]; then
        ./run_echo.sh
    else
        ./build/echo
    fi
fi