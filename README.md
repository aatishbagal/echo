# Echo - BitChat Compatible Desktop Messaging

Echo is a cross-platform desktop Bluetooth messaging application that is fully compatible with the BitChat network. It enables secure, offline peer-to-peer communication using Bluetooth mesh networking.

## Features

- Cross-platform support (Linux, Windows, macOS)
- BitChat protocol compatibility
- Bluetooth Low Energy mesh networking
- End-to-end encryption using Noise Protocol Framework
- IRC-style command interface
- No internet required
- Store-and-forward messaging (coming soon)
- GUI interface (coming soon)

## Current Status

**Phase 1 (In Progress)**: Core Messaging MVP
- [x] Project setup and build system
- [x] SimpleBLE integration
- [x] Basic device discovery
- [x] Console UI for testing
- [ ] BitChat protocol implementation
- [ ] Cryptographic layer
- [ ] Message routing

## Quick Start

### Linux (Fedora/RHEL)

```bash
# Install dependencies
sudo dnf groupinstall "Development Tools" "Development Libraries"
sudo dnf install cmake git bluez-libs-devel libsodium-devel openssl-devel lz4-devel

# Clone and setup
git clone https://github.com/aatishbagal/echo.git echo
cd echo
git submodule add https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble
git submodule update --init --recursive

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run
./echo
```

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake git libbluetooth-dev libsodium-dev libssl-dev liblz4-dev

# Follow same clone/build steps as above
```

### Windows

```bash
# Using vcpkg (recommended)
vcpkg install libsodium:x64-windows openssl:x64-windows lz4:x64-windows

# Clone and build with Visual Studio or MinGW
git clone <your-repo-url> echo
cd echo
git submodule add https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble
git submodule update --init --recursive

mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## Usage

Once built, run Echo and use these commands:

```
echo> scan          # Start scanning for BitChat devices
echo> devices       # List discovered devices  
echo> connect <addr> # Connect to a device
echo> help          # Show all commands
echo> quit          # Exit
```

## Architecture

```
echo/
├── src/core/bluetooth/    # SimpleBLE integration, device management
├── src/core/protocol/     # BitChat protocol implementation
├── src/core/crypto/       # Noise Protocol Framework, encryption
├── src/core/mesh/         # Mesh networking, message routing
├── src/core/commands/     # IRC-style command parsing
└── src/ui/               # User interface (console, future GUI)
```

## BitChat Compatibility

Echo implements the exact same protocol as BitChat mobile apps:

- **Transport**: Bluetooth Low Energy mesh
- **Protocol**: Binary format with 1-byte type + 13-byte headers
- **Crypto**: Noise Protocol XX pattern, X25519/Ed25519, AES-256-GCM
- **Commands**: IRC-style (`/join`, `/msg`, `/who`, etc.)
- **Range**: ~30m direct, 300m+ with multi-hop relay

## Development Roadmap

### Phase 1: Core Messaging (Current)
- Basic Bluetooth operations
- Device discovery and connection
- BitChat protocol parser
- Console interface

### Phase 2: Protocol Implementation
- Noise Protocol Framework integration
- Message encryption/decryption  
- IRC command handling
- Mesh routing

### Phase 3: Advanced Features
- GUI interface (Qt or Dear ImGui)
- File transfer
- Group messaging improvements
- Performance optimizations

## Dependencies

- **SimpleBLE**: Cross-platform Bluetooth LE library
- **libsodium**: Cryptographic library
- **OpenSSL**: Additional crypto support
- **LZ4**: Compression (BitChat compatibility)
- **CMake**: Build system
