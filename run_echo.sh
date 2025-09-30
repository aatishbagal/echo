#!/bin/bash
# Run Echo with BLE advertising

echo "================================================"
echo "Echo - Starting with BLE Advertising"
echo "================================================"

# Check if echo binary exists
if [ ! -f "build/echo" ]; then
    echo "Error: build/echo not found. Run ./setup.sh first"
    exit 1
fi

# Check if identity file exists to get peer ID
if [ ! -f "echo_identity.dat" ]; then
    echo "Running Echo once to generate identity..."
    timeout 2 ./build/echo || true
fi

# Read identity (this is a placeholder - in real implementation, Echo would output peer ID)
echo ""
echo "Starting BLE advertising in background..."
echo "Note: You'll need to enter your peer ID"
echo ""

# Get peer ID from user
read -p "Enter your peer ID (from Echo startup, first 16 chars of fingerprint): " PEER_ID

if [ -z "$PEER_ID" ]; then
    echo "No peer ID provided, using demo ID"
    PEER_ID="0123456789abcdef"
fi

# Start Python advertiser in background
python3 advertise_echo.py --peer-id "$PEER_ID" &
ADVERTISER_PID=$!

echo "Advertising started (PID: $ADVERTISER_PID)"
echo ""

# Give advertising time to start
sleep 2

# Run Echo
echo "Starting Echo..."
./build/echo

# Cleanup
echo ""
echo "Stopping advertising..."
kill $ADVERTISER_PID 2>/dev/null

echo "Shutdown complete"