# GATT-Only Mode - Demo Instructions

## Overview
This mode enables direct device-to-device connections using GATT without relying on advertisement-based discovery.

## How It Works

### Device 1 (Server)
1. Starts GATT server and advertises
2. Shows its Bluetooth address in the output
3. Waits for connections

### Device 2 (Client)
1. Scans to see nearby BLE devices
2. Manually connects to Device 1 using its Bluetooth address
3. Establishes GATT connection

## Step-by-Step Demo

### On Both Devices:
```powershell
cd C:\Code-Proj\echo
.\rebuild.bat
```

### Device 1 (First Computer):
```
.\build\Release\echo.exe
```

The app will show something like:
```
Using Bluetooth adapter: Bluetooth [AA:BB:CC:DD:EE:FF]
```

**Copy this address!** You'll need it for Device 2.

### Device 2 (Second Computer):
```
.\build\Release\echo.exe
```

Then type:
```
scan
```

You'll see devices appear like:
```
[SCAN] Unknown [XX:YY:ZZ:11:22:33] (-65 dBm)
[SCAN] Device Name [AA:BB:CC:DD:EE:FF] (-45 dBm)
```

Now connect to Device 1 using its address:
```
connect AA:BB:CC:DD:EE:FF
```

Replace `AA:BB:CC:DD:EE:FF` with the actual address from Device 1.

### Test Messaging:

Once connected, on either device:
```
/join #global
```

Type messages and they should appear on both devices!

## Troubleshooting

### "Device not found"
- Make sure both devices are running
- Try scanning for longer (wait 10-15 seconds)
- Check that Bluetooth is enabled on both computers

### "Connection failed"
- Ensure the address is typed correctly
- Try running as Administrator on both devices
- Check Windows Developer Mode is enabled

### "Service discovery failed"
- Wait a few seconds after connecting
- The GATT service takes time to enumerate
- Try disconnecting and reconnecting

## Commands

- `scan` - Start scanning for devices
- `devices` - List all discovered devices with addresses
- `connect <address>` - Connect to a specific device
- `/join #global` - Join global chat after connection
- `help` - Show all commands

## Technical Details

- Uses GATT Service UUID: `F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C`
- Three characteristics: TX (write), RX (notify), MESH (write+notify)
- Messages sent via characteristic writes
- Received via characteristic notifications
- Mesh routing happens automatically
