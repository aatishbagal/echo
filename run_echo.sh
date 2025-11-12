#!/bin/bash

echo "================================================"
echo "Echo - Starting with BLE Advertising"
echo "================================================"

if [ ! -f "build/echo" ]; then
    echo "Error: build/echo not found. Run ./setup.sh first"
    exit 1
fi

if [ ! -f "echo_identity.dat" ]; then
    echo "Running Echo once to generate identity..."
    timeout 2 ./build/echo || true
fi

if [ ! -f "echo_identity.dat" ]; then
    echo "Error: Failed to generate identity file"
    exit 1
fi

echo "Reading identity from echo_identity.dat..."

USERNAME=$(hexdump -e '4/1 "%02x"' -n 4 echo_identity.dat | xxd -r -p | hexdump -e '"%d"')
USERNAME=$(dd if=echo_identity.dat bs=1 skip=4 count=$USERNAME 2>/dev/null)

FINGERPRINT=$(hexdump -e '16/1 "%02x"' -s 36 -n 16 echo_identity.dat)

echo "  Username: $USERNAME"
echo "  Peer ID: $FINGERPRINT"
echo ""

python3 advertise_echo.py --peer-id "$FINGERPRINT" --username "$USERNAME" &
ADVERTISER_PID=$!

echo "Advertising started (PID: $ADVERTISER_PID)"
echo ""

sleep 2

echo "Starting Echo..."
./build/echo

echo ""
echo "Stopping advertising..."
kill $ADVERTISER_PID 2>/dev/null

echo "Shutdown complete"