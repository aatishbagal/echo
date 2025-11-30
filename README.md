# Echo - Bluetooth P2P Messaging

![Status](https://img.shields.io/badge/status-under%20development-orange)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-blue)
![C++](https://img.shields.io/badge/C++-17-00599C?logo=cplusplus)
![License](https://img.shields.io/badge/license-Apache%202.0-green)
![Network](https://img.shields.io/badge/network-WiFi%20%7C%20Bluetooth-blueviolet)

Echo is a cross-platform desktop messaging application written in C++ that enables peer-to-peer communication over Bluetooth and local WiFi networks without requiring internet connectivity. The application is designed to be compatible with the BitChat network protocol.

## Current Status

### Working Features
- Local WiFi network discovery and messaging
- File sharing over WiFi (up to 32KB per file)
- BitChat device discovery (detection only)
- Cross-device Echo discovery
- Global chat broadcasts
- Personal direct messaging
- User identity generation and persistence

### In Development
- Bluetooth messaging (discovery works, messaging in progress)
- BitChat protocol messaging compatibility
- Full mesh network relay
- End-to-end encryption (Noise Protocol Framework)
- macOS support

### Platform Support
- Windows 10/11 (tested)
- Linux (Fedora, Ubuntu tested)
- macOS (in progress)

## Technical Stack

### Core Technologies
- **Language:** C++ 17
- **Build System:** CMake
- **Package Manager:** vcpkg

### External Dependencies
- **SimpleBLE** - Cross-platform Bluetooth Low Energy library
- **libsodium** - Cryptography library (planned integration)
- **Platform-specific:**
  - Windows: WinRT Bluetooth APIs
  - Linux: BlueZ D-Bus
  - macOS: Core Bluetooth (in development)

### Network Architecture
- **Bluetooth:** BLE mesh networking (discovery functional)
- **WiFi:** UDP broadcast discovery (port 48270) + TCP messaging (port 48271)
- **Protocol:** BitChat-compatible binary protocol
- **Range:** ~30m direct Bluetooth, extended range via local WiFi

## Installation

### Windows

#### Prerequisites
- Visual Studio 2019 or later
- CMake 3.20 or higher
- Git

#### Setup Steps

1. Clone the repository:
```cmd
git clone <repository-url>
cd echo
```

2. Run initial setup (installs vcpkg and dependencies):
```cmd
setup.bat
```

3. Build the application:
```cmd
build.bat
```

4. Configure firewall (run as Administrator):
```cmd
setup_firewall.bat
```

5. Run Echo:
```cmd
run_echo.bat
```

#### Subsequent Runs
After initial setup, you only need:
```cmd
run_echo.bat
```

To rebuild after code changes:
```cmd
build.bat
```

### Linux

#### Prerequisites
- GCC 9+ or Clang 10+
- CMake 3.20 or higher
- Git
- Python 3 (for BLE advertising)
- BlueZ development libraries

#### Setup Steps

1. Clone the repository:
```bash
git clone <repository-url>
cd echo
```

2. Run initial setup (installs vcpkg and dependencies):
```bash
chmod +x setup.sh
./setup.sh
```

3. Build the application:
```bash
chmod +x rebuild.sh
./rebuild.sh
```

4. Configure firewall (requires sudo):
```bash
chmod +x setup_firewall.sh
sudo ./setup_firewall.sh
```

5. Run Echo:
```bash
chmod +x run_echo.sh
./run_echo.sh
```

#### Subsequent Runs
After initial setup, you only need:
```bash
./run_echo.sh
```

To rebuild after code changes:
```bash
./rebuild.sh
```

#### Linux-Specific Notes
- BlueZ advertising uses a Python helper script (automatically managed)
- Requires Bluetooth service to be active: `systemctl status bluetooth`
- May require user to be in `bluetooth` group for some operations

## Firewall Configuration

Echo requires specific ports to be open for local network communication:

### Ports Used
- **UDP 48270** - WiFi peer discovery (broadcast)
- **TCP 48271** - WiFi direct messaging

### Troubleshooting Network Discovery

If devices on the same local network cannot discover each other:

**Windows:**
Run as Administrator:
```cmd
setup_firewall.bat
```

**Linux:**
Run with sudo:
```bash
sudo ./setup_firewall.sh
```

The scripts automatically detect your firewall system (firewalld, ufw, or iptables) and configure the necessary rules.

### Manual Firewall Configuration

**Windows (PowerShell as Admin):**
```powershell
netsh advfirewall firewall add rule name="Echo WiFi Discovery IN" dir=in action=allow protocol=UDP localport=48270
netsh advfirewall firewall add rule name="Echo WiFi Discovery OUT" dir=out action=allow protocol=UDP localport=48270
netsh advfirewall firewall add rule name="Echo WiFi Messaging IN" dir=in action=allow protocol=TCP localport=48271
netsh advfirewall firewall add rule name="Echo WiFi Messaging OUT" dir=out action=allow protocol=TCP localport=48271
```

**Linux (firewalld):**
```bash
sudo firewall-cmd --permanent --add-port=48270/udp
sudo firewall-cmd --permanent --add-port=48271/tcp
sudo firewall-cmd --reload
```

**Linux (ufw):**
```bash
sudo ufw allow 48270/udp
sudo ufw allow 48271/tcp
```

## Usage

### Basic Commands

When Echo starts, you'll see your user identity with a randomly generated username and fingerprint.

#### Discovery and Connection
```
scan              - Start scanning for Bluetooth devices
stop              - Stop scanning
devices           - List all discovered devices
echo              - List only Echo devices (WiFi and Bluetooth)
connect <addr>    - Connect to device by Bluetooth address
```

#### Messaging
```
/join #global     - Enter global chat (broadcasts to all users)
/chat @username   - Start personal chat with specific user
/exit             - Exit current chat
/who              - List users in current chat
whoami            - Show your identity
/nick <name>      - Change your username
```

#### In Chat Mode
```
/exit             - Exit chat mode
/who              - List participants
/status           - Show current chat info
/file 'path'      - Send file to chat
/accept <id>      - Accept received file
/decline <id>     - Decline received file
/help             - Show chat commands
```

#### WiFi Network
```
wifi start        - Enable WiFi verbose mode
wifi stop         - Disable WiFi verbose mode
wifi status       - Show WiFi status and discovered peers
wifi peers        - List discovered WiFi peers
```

#### Other Commands
```
clear             - Clear screen
help              - Show all commands
quit              - Exit application
```

### File Sharing

Echo supports file sharing up to 32KB per file over both global and personal chats.

#### Sending a File

1. Enter chat mode (global or personal):
```
/join #global
```
or
```
/chat @username
```

2. Send the file using full path with single quotes:
```
/file '/home/user/document.txt'
```
Windows:
```
/file 'C:\Users\username\document.txt'
```

3. You'll see confirmation:
```
[GLOBAL] sent
```
or
```
[PERSONAL] sent to username
```

#### Receiving a File

1. When someone sends you a file, you'll see:
```
[FILE] from username: document.txt bytes=1234 id=abc123def456
Use /accept abc123def456 or /decline abc123def456
```

2. Accept the file:
```
/accept abc123def456
```

3. File is saved and you'll see:
```
[File Sharing][username] Saved to: /path/to/FileSharing/document.txt
```

Files are saved in the `FileSharing/` directory relative to where you run the Echo executable.

#### File Sharing Limitations
- Maximum file size: 32KB (32,768 bytes)
- Only works within chat modes (global or personal)
- Transmitted as base64-encoded data
- No resume capability for interrupted transfers

### User Identity

On first run, Echo generates a random username (format: AdjectiveNoun, e.g., "SwiftFox") and Ed25519 keypair. Your identity is saved to `echo_identity.dat` in the working directory.

To change your username:
```
/nick NewUsername
```

Note: You must restart Echo for the new username to be advertised to other devices.

## Network Architecture

### WiFi Discovery Protocol
Echo uses UDP broadcast on port 48270 to discover peers on the local network. Each device broadcasts its username and TCP port every 2 seconds.

**Packet Format:**
```
[version=1][username_len][username][fingerprint_len][fingerprint][port_high][port_low]
```

### WiFi Messaging Protocol
Direct messaging uses TCP on port 48271. Messages are length-prefixed:
```
[4-byte length][message payload]
```

### Bluetooth Protocol
Echo implements the BitChat service UUID `F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C` for device identification. Currently supports:
- Device discovery via BLE advertising
- GATT characteristic enumeration
- Connection management

Messaging over Bluetooth is in development.

## Project Structure

```
echo/
├── src/
│   ├── core/
│   │   ├── bluetooth/        # Bluetooth device management
│   │   ├── crypto/           # User identity and cryptography
│   │   ├── network/          # WiFi Direct implementation
│   │   ├── protocol/         # BitChat protocol and messages
│   │   └── commands/         # IRC-style command parsing
│   ├── ui/                   # Console interface
│   └── main.cpp              # Application entry point
├── scripts/                  # Build and setup scripts
├── docs/                     # Documentation
└── build/                    # CMake build output
```

## Troubleshooting

### Windows

**BLE Advertising Not Working:**
- Enable Developer Mode in Windows Settings
- Or run Echo as Administrator
- Windows 11 requires one of these for BLE peripheral mode

**WiFi Discovery Issues:**
- Run `setup_firewall.bat` as Administrator
- Check if Windows Firewall is blocking UDP 48270 or TCP 48271
- Ensure devices are on the same subnet

### Linux

**Bluetooth Scanning Conflicts:**
- If you see "Operation already in progress" errors, this is expected when the Python advertiser is running
- The application handles this gracefully and continues functioning

**Firewall Blocking:**
- Run `sudo ./setup_firewall.sh`
- Check firewall status: `sudo firewall-cmd --list-all` or `sudo ufw status`

**BlueZ Issues:**
- Ensure Bluetooth service is running: `systemctl status bluetooth`
- Check D-Bus permissions if advertising fails

### General

**No Devices Found:**
- Ensure both devices are on the same local network
- Check firewall settings on both devices
- Try `wifi status` to see if peers are discovered
- Verify Bluetooth is enabled and powered on

**File Transfer Fails:**
- Check file size (must be under 32KB)
- Ensure you're in chat mode (global or personal)
- Verify recipient is connected (check `wifi peers` or `echo`)

**Messages Not Sending:**
- Currently only WiFi messaging is fully functional
- Use `/join #global` or `/chat @username` to enter chat mode
- Check `wifi peers` to confirm recipient is discovered

## Known Limitations

1. **Bluetooth Messaging:** Discovery works, but message transmission over Bluetooth is still in development
2. **BitChat Compatibility:** Can detect BitChat devices but cannot exchange messages yet
3. **File Size:** Limited to 32KB per file
4. **No Encryption:** End-to-end encryption not yet implemented
5. **No Persistence:** Messages are not saved (ephemeral only)
6. **No Mesh Relay:** Multi-hop message forwarding not implemented
7. **WiFi Only Messaging:** Most reliable communication currently over local WiFi

## Development

### Building from Source

#### Prerequisites
- CMake 3.20+
- C++17 compatible compiler
- vcpkg (automatically installed by setup scripts)

#### Manual Build
```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Code Organization

The project follows a modular architecture:
- **bluetooth/** - Platform-specific BLE implementations
- **network/** - WiFi Direct UDP/TCP communication
- **protocol/** - BitChat binary protocol and message types
- **crypto/** - User identity and cryptographic operations (placeholder)
- **ui/** - Console-based user interface

## License

Apache License 2.0

Copyright 2024 [Your Name]

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

See the LICENSE file in the root directory for the full license text.

## Acknowledgments

- **SimpleBLE** - Cross-platform Bluetooth library
- **BitChat** - Protocol inspiration and compatibility target
- **vcpkg** - C++ package management

---

For bug reports and issues, please use the issue tracker.

**Note:** This is experimental software under active development. Features and APIs may change.