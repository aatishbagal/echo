#!/bin/bash

echo "================================================"
echo "Echo - Bluetooth Mesh Messaging (macOS)"
echo "================================================"
echo ""

if [ ! -f "build/echo" ]; then
    echo "Error: build/echo not found"
    echo "Run ./setup_macos.sh first to build the project"
    exit 1
fi

if [ ! -f "echo_identity.dat" ]; then
    echo "Generating identity..."
    ./build/echo &
    ECHO_PID=$!
    sleep 2
    kill $ECHO_PID 2>/dev/null
    wait $ECHO_PID 2>/dev/null
fi

if [ ! -f "echo_identity.dat" ]; then
    echo "Error: Failed to generate identity file"
    exit 1
fi

echo "IMPORTANT: macOS Limitations"
echo "- Scanning: Works"
echo "- Advertising: NOT SUPPORTED (cannot be discovered)"
echo ""

./build/echo

echo ""
echo "Echo stopped"
