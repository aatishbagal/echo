#!/bin/bash

set -e

CONFIG="Release"
if [ "$1" = "debug" ]; then
    CONFIG="Debug"
fi

echo "=============================================="
echo "Echo - Clean Rebuild (macOS)"
echo "Configuration: $CONFIG"
echo "=============================================="
echo ""

if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found"
    echo "Run this script from the Echo project root"
    exit 1
fi

if [ -d "build" ]; then
    echo "Removing previous build directory..."
    rm -rf build
    echo "  [OK] Build directory cleaned"
else
    echo "No existing build directory found"
fi

mkdir build
echo ""

OPENSSL_ROOT=$(brew --prefix openssl@3 2>/dev/null || echo "/usr/local/opt/openssl@3")

if [ ! -d "$OPENSSL_ROOT" ]; then
    echo "Error: OpenSSL not found"
    echo "Run ./setup_macos.sh to install dependencies"
    exit 1
fi

echo "Configuring CMake..."

cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=$CONFIG \
    -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT" \
    -DOPENSSL_INCLUDE_DIR="$OPENSSL_ROOT/include" \
    -DOPENSSL_CRYPTO_LIBRARY="$OPENSSL_ROOT/lib/libcrypto.dylib" \
    -DOPENSSL_SSL_LIBRARY="$OPENSSL_ROOT/lib/libssl.dylib"

if [ $? -ne 0 ]; then
    echo "CMake configuration failed"
    cd ..
    exit 1
fi

echo "  [OK] Configuration complete"
echo ""

echo "Building Echo ($CONFIG)..."

cmake --build . --config $CONFIG --parallel

if [ $? -ne 0 ]; then
    echo "Build failed"
    cd ..
    exit 1
fi

cd ..

echo ""
echo "=============================================="
echo "Clean rebuild complete!"
echo "=============================================="
echo ""
echo "Executable: build/echo"
echo ""
