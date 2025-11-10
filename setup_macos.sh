#!/bin/bash

set -e

echo "=============================================="
echo "Echo - macOS Setup Script"
echo "=============================================="
echo ""

if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found"
    echo "Run this script from the Echo project root"
    exit 1
fi

echo "Checking prerequisites..."

if ! command -v brew &> /dev/null; then
    echo "Error: Homebrew not found"
    echo "Install from: https://brew.sh"
    exit 1
fi
echo "  [OK] Homebrew found"

if ! command -v git &> /dev/null; then
    echo "Error: git not found"
    exit 1
fi
echo "  [OK] git found"

if ! command -v cmake &> /dev/null; then
    echo "Installing CMake..."
    brew install cmake
fi
echo "  [OK] CMake found"

echo ""
echo "Installing dependencies via Homebrew..."

DEPS="openssl@3 libsodium lz4"
for dep in $DEPS; do
    if brew list $dep &>/dev/null; then
        echo "  [OK] $dep already installed"
    else
        echo "  Installing $dep..."
        brew install $dep
    fi
done

echo ""
echo "Setting up SimpleBLE submodule..."

if [ ! -f "external/simpleble/CMakeLists.txt" ]; then
    echo "  Initializing SimpleBLE submodule..."
    git submodule add https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble 2>/dev/null || true
    git submodule update --init --recursive
else
    echo "  [OK] SimpleBLE already present"
    git submodule update --init --recursive
fi

echo ""
echo "Creating build directory..."

if [ -d "build" ]; then
    echo "  Build directory exists, cleaning..."
    rm -rf build
fi

mkdir -p build
cd build

echo ""
echo "Configuring CMake..."

OPENSSL_ROOT=$(brew --prefix openssl@3)

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT" \
    -DOPENSSL_INCLUDE_DIR="$OPENSSL_ROOT/include" \
    -DOPENSSL_CRYPTO_LIBRARY="$OPENSSL_ROOT/lib/libcrypto.dylib" \
    -DOPENSSL_SSL_LIBRARY="$OPENSSL_ROOT/lib/libssl.dylib"

if [ $? -ne 0 ]; then
    echo "CMake configuration failed"
    cd ..
    exit 1
fi

echo ""
echo "Building Echo..."

cmake --build . --config Release --parallel

if [ $? -ne 0 ]; then
    echo "Build failed"
    cd ..
    exit 1
fi

cd ..

echo ""
echo "=============================================="
echo "Setup complete!"
echo "=============================================="
echo ""
echo "Executable: build/echo"
echo ""
echo "To run Echo:"
echo "  ./build/echo"
echo ""
echo "REMINDER: macOS version limitations:"
echo "  - Can scan and detect other Echo devices"
echo "  - Cannot advertise (not discoverable by others)"
echo "  - Receive-only mesh node"
echo ""
echo "To rebuild later:"
echo "  cd build && cmake --build . --config Release"
echo ""
