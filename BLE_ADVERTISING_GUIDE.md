# BLE Advertising Implementation Guide

## Current Status

Echo can **scan and detect** BLE devices but **cannot advertise** itself yet. This means:
- ✅ Echo can see BitChat devices (if properly configured)
- ❌ BitChat devices cannot see Echo
- ❌ Echo cannot participate in mesh relay

## Why You Don't See BitChat Devices

BitChat uses **BLE GATT services** with specific UUIDs. Your phone appears differently when:
1. **Bluetooth Settings (Classic BT)**: Shows as regular paired device with MAC address
2. **BLE Scanning**: Shows as peripheral with service UUIDs - this is what Echo needs

### Debugging: Check What's Actually Visible

```bash
# On Linux, use bluetoothctl to see BLE devices
sudo bluetoothctl
scan on
# Wait 30 seconds - look for devices with service UUIDs

# Or use hcitool
sudo hcitool lescan

# Check if BitChat is advertising
sudo btmon
# Open BitChat on phone, look for advertising packets
```

## BitChat Service UUIDs

According to the protocol, BitChat uses:
- **Service UUID**: `0000180F-0000-1000-8000-00805F9B34FB` (Battery Service - repurposed)
- **Characteristic UUID**: `00002A19-0000-1000-8000-00805F9B34FB`
- **Device Name Format**: `Echo:Username:Fingerprint` or `BitChat:Username:Fingerprint`

## Linux BLE Advertising Implementation

### Dependencies Required

```bash
# Fedora/RHEL
sudo dnf install bluez bluez-tools python3-dbus glib2-devel libdbus-1-dev

# Ubuntu/Debian  
sudo apt install bluez python3-dbus libglib2.0-dev libdbus-1-dev
```

### Permissions Setup

```bash
# Add user to bluetooth group
sudo usermod -a -G bluetooth $USER

# Log out and back in, or:
newgrp bluetooth

# Give Echo capabilities (after building)
sudo setcap cap_net_raw,cap_net_admin+eip ./build/echo
```

### BlueZ Version Check

```bash
bluetoothctl --version
# Need BlueZ 5.50 or higher for proper BLE advertising
```

### Implementation Options

#### Option 1: BlueZ D-Bus API (Recommended)
- **Pros**: Proper, native Linux support
- **Cons**: Complex, requires D-Bus/GLib integration
- **Libraries**: libdbus, glib2
- **Status**: Not yet implemented

#### Option 2: HCI Direct Access
- **Pros**: Direct hardware control
- **Cons**: Requires root, may conflict with BlueZ
- **Status**: Not recommended

#### Option 3: Python Helper Script
- **Pros**: Quick to implement, uses BlueZ
- **Cons**: External dependency
- **Status**: Can implement as temporary solution

## Windows BLE Advertising Implementation

### Dependencies Required

- Windows 10 version 1703+ (Creators Update)
- Windows SDK 10.0.15063.0+
- WinRT C++ libraries

### Windows BLE APIs

Windows provides native BLE advertising through:
```cpp
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>

using namespace winrt::Windows::Devices::Bluetooth::Advertisement;

// Create advertiser
BluetoothLEAdvertisementPublisher publisher;

// Set device name
publisher.Advertisement().LocalName(L"Echo:Username:Fingerprint");

// Add service UUID
BluetoothLEAdvertisementDataSection dataSection;
// ... configure service UUID

publisher.Start();
```

### Implementation Status
- **API Available**: Yes, WinRT
- **Cross-platform**: Windows only
- **Status**: Not yet implemented

## Temporary Testing Solution

Until advertising is implemented, test mesh discovery using:

### Python BLE Advertising Script (Linux)

```python
#!/usr/bin/env python3
# ble_advertise.py - Temporary Echo BLE advertiser

import dbus
import dbus.mainloop.glib
from gi.repository import GLib

ECHO_SERVICE_UUID = '0000180f-0000-1000-8000-00805f9b34fb'

# Full implementation would go here
# This advertises Echo as a BLE peripheral
```

### Usage
```bash
# Run Python advertiser alongside Echo
python3 ble_advertise.py --username YourName --fingerprint abc123 &
./build/echo
```

## Testing BitChat Detection

### On Your Phone (BitChat)
1. Open BitChat app
2. Make sure Bluetooth is on
3. BitChat should auto-start advertising
4. Check "Users" or main screen for nearby devices

### On Linux (Echo)
```bash
# Run Echo
./build/echo

# In Echo console
echo> scan

# Wait 10-15 seconds
echo> devices

# Should show BitChat devices if:
# - BitChat is advertising
# - SimpleBLE is scanning correctly
# - Service UUIDs match
```

### If BitChat Still Doesn't Appear

1. **Check BitChat is actually advertising**:
   ```bash
   # On another device with BlueZ
   sudo bluetoothctl
   scan on
   # Look for device with Battery Service UUID
   ```

2. **Verify SimpleBLE scanning**:
   - SimpleBLE scans for **all** BLE peripherals
   - Should show any device advertising
   - If you see other BLE devices but not BitChat, it's a BitChat issue

3. **Check BitChat permissions**:
   - Android: Location permission required for BLE
   - iOS: Bluetooth permission required
   - Make sure BitChat has all permissions granted

## Next Steps for Full Implementation

### Phase 1: Linux Advertising (Priority)
1. Implement BlueZ D-Bus advertising
2. Add to CMakeLists.txt with `#ifdef __linux__`
3. Test with BitChat Android/iOS

### Phase 2: Windows Advertising
1. Implement WinRT BLE advertising
2. Add to CMakeLists.txt with `#ifdef _WIN32`
3. Test cross-platform

### Phase 3: macOS (Optional)
1. Use Core Bluetooth (Swift/Objective-C++)
2. Or wait for SimpleBLE peripheral support

## Current Workaround

For now, Echo can:
- Scan for BitChat devices
- Display BitChat usernames/fingerprints
- Parse BitChat protocol messages (when implemented)
- Cannot advertise itself
- Cannot relay messages (needs advertising)

To test if scanning works, try finding **any** BLE device first (fitness tracker, smart watch, etc.) to verify SimpleBLE is working correctly.