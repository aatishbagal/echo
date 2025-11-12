# Echo - macOS Build Instructions

## Platform Support

Echo now has **full BLE advertising support on macOS** using CoreBluetooth's CBPeripheralManager:

- ✅ **Scanning**: Can detect other Echo devices (Linux/Windows/macOS)
- ✅ **Advertising**: Can broadcast presence (fully discoverable)
- ✅ **Full Mesh**: Complete mesh network node

## Prerequisites

- macOS 10.15 (Catalina) or later
- Xcode Command Line Tools
- Homebrew package manager

## Quick Start

### 1. Install Homebrew (if not installed)

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 2. Run Setup

```bash
chmod +x setup_macos.sh
./setup_macos.sh
```

This will:
- Install dependencies (OpenSSL, libsodium, lz4, CMake)
- Clone SimpleBLE submodule
- Configure and build Echo

### 3. Run Echo

```bash
chmod +x run_echo_macos.sh
./run_echo_macos.sh
```

Or directly:
```bash
./build/echo
```

## Manual Build

If you prefer manual control:

```bash
brew install cmake openssl@3 libsodium lz4
git submodule update --init --recursive

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake --build . --parallel
cd ..

./build/echo
```

## Rebuild

```bash
chmod +x rebuild_macos.sh
./rebuild_macos.sh
```

For debug build:
```bash
./rebuild_macos.sh debug
```

## Bluetooth Permissions

On first run, macOS will prompt for Bluetooth permission. Click "Allow" to enable device scanning.

If permission was denied:
1. Open System Preferences → Security & Privacy → Privacy
2. Select "Bluetooth" from the left sidebar
3. Add Terminal.app (or your terminal emulator)
4. Restart Terminal and run Echo again

## What Works

- Identity generation and management
- Bluetooth scanning for Echo devices
- Device discovery and listing
- Console UI and commands
- IRC-style command parsing
- Crypto operations

## What Doesn't Work

- BLE advertising (not discoverable by other devices)
## What Works

- Identity generation and management
- Bluetooth scanning for Echo devices
- **Bluetooth advertising (broadcasting presence)**
- Device discovery and listing
- Console UI and commands
- IRC-style command parsing
- **Full mesh network participation**

## Troubleshooting

### Build Errors

**CMake can't find OpenSSL:**
```bash
brew reinstall openssl@3
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
```

**Missing libsodium/lz4:**
```bash
brew install libsodium lz4
```

**SimpleBLE not found:**
```bash
git submodule update --init --recursive
```

### Runtime Issues

**Bluetooth permission denied:**
- Check System Preferences → Security & Privacy → Bluetooth
- Add Terminal to allowed apps

**No devices found:**
- Ensure other Echo instances are advertising
- Try scanning for longer periods
- Check that Bluetooth is enabled

**Advertising not working:**
- Ensure Bluetooth permission is granted
- Check that no other app is using Bluetooth peripheral mode
- Restart Bluetooth if needed: `sudo pkill bluetoothd`

## Commands

All standard Echo commands work:
- `scan` - Start scanning for devices
- `stop` - Stop scanning
- `devices` - List discovered devices
- `whoami` - Show your identity
- `/nick <name>` - Change username
- `help` - Show commands
- `quit` - Exit

## Platform Comparison

| Feature | Linux | Windows | macOS |
|---------|-------|---------|-------|
| Scanning | ✅ | ✅ | ✅ |
| Advertising | ✅ | ✅ | ✅ |
| Discoverable | ✅ | ✅ | ✅ |
| Full Mesh | ✅ | ✅ | ✅ |

## Technical Details

The macOS implementation uses:
- **CoreBluetooth** framework for BLE operations
- **CBCentralManager** for scanning (via SimpleBLE)
- **CBPeripheralManager** for advertising (custom implementation)
- **Objective-C++** (.mm files) for CoreBluetooth integration
