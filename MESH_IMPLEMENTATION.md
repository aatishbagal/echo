# Echo Mesh Network & File Transfer Implementation

## Summary of Changes

This implementation adds full mesh networking capabilities, proper GATT services for Windows and Linux, and file transfer functionality to your Echo application.

## Key Features Implemented

### 1. **GATT Server with Characteristics (Windows & Linux)**

#### Windows (WindowsAdvertiser.cpp)
- **TX Characteristic** (8E9B7A4C-2D5F-4B6A-9C3E-1F8D7B2A5C4E): Write/WriteWithoutResponse
  - Receives messages from other devices
- **RX Characteristic** (6D4A9B2E-5C7F-4A8D-9B3C-2E1F8D7A4B5C): Notify/Indicate
  - Sends messages to subscribed clients
- **MESH Characteristic** (9A3B5C7D-4E6F-4B8A-9D2C-3F1E8D7B4A5C): Write/Notify
  - Handles mesh broadcast messages

#### Linux (BluezAdvertiser.cpp)
- Updated Python script to create full GATT server with same characteristics
- Uses DBus to register GATT service and characteristics
- Properly handles writes and notifications

### 2. **Mesh Network Routing (MeshNetwork.cpp/h)**

- **Flood-based routing** with TTL (Time To Live)
- **Message deduplication** using seen message IDs (up to 1000 messages cached)
- **Automatic forwarding** of GLOBAL_MESSAGE and ANNOUNCE types
- **Peer management** with automatic addition/removal
- **Message timeout** (300 seconds) to prevent memory bloat
- **Forward callback** to send messages to all connected peers except source

### 3. **GATT Client Subscriptions (BluetoothManager.cpp)**

- **Automatic subscription** to RX and MESH characteristics on connection
- **Message routing** through mesh network for all received data
- **Characteristic detection** supporting both full UUIDs and partial matching
- **Retry logic** for Windows service discovery timing issues

### 4. **File Transfer System (FileTransfer.cpp/h)**

- **Base64 encoding** for binary data over BLE
- **Chunking** with 512-byte chunks (fits BLE MTU limits)
- **Progress tracking** with callbacks
- **Checksum validation** using simple sum algorithm
- **Three-stage protocol**:
  1. FILE_START: Metadata and transfer initiation
  2. FILE_CHUNK: Data chunks with index
  3. FILE_END: Completion with checksum
- **Automatic reassembly** and file saving to `downloads/` directory

### 5. **Updated Message Types**

New message types added:
- `FILE_START (0x08)`: File transfer initiation
- `FILE_CHUNK (0x09)`: File data chunk
- `FILE_END (0x0A)`: File transfer completion
- `FILE_REQUEST (0x0B)`: Request file from peer

## How Mesh Networking Works

### Message Flow

```
Device A → Device B → Device C
    |         |         |
    └─────────┴─────────┘
     All receive message
```

1. **Device A sends GLOBAL_MESSAGE**
   - Message has TTL=7 and unique messageId
   - Sent to all connected peers

2. **Device B receives message**
   - Checks if messageId already seen → NO
   - Marks messageId as seen
   - Delivers to local user
   - Decrements TTL to 6
   - Forwards to all peers except A

3. **Device C receives from both A and B**
   - From A: messageId not seen → Process and forward with TTL=6
   - From B: messageId already seen → Drop (no duplicate)

### TTL Strategy

- **Global messages**: TTL=7 (can hop through 7 devices)
- **File chunks**: TTL=3 (limited to nearby devices)
- **Private messages**: TTL=7 but NO forwarding
- Messages with TTL=0 are dropped

## File Transfer Usage

### Sending a File

```cpp
auto fileTransferManager = std::make_shared<FileTransferManager>();

fileTransferManager->setSendChunkCallback([&bluetoothManager](const Message& msg, const std::string& address) {
    return bluetoothManager->sendData(address, msg.serialize());
});

fileTransferManager->setProgressCallback([](uint32_t transferId, uint16_t received, uint16_t total) {
    std::cout << "Progress: " << received << "/" << total << std::endl;
});

fileTransferManager->startFileSend("photo.jpg", "alice", "AA:BB:CC:DD:EE:FF", "bob");
```

### Receiving a File

Files are automatically received when FILE_START/CHUNK/END messages arrive:
- Progress displayed in console
- Files saved to `downloads/filename`
- Checksum validation ensures integrity

## Windows-to-Windows Communication

### Why it wasn't working

1. **Missing GATT characteristics** - SimpleBLE can only communicate via GATT read/write/notify
2. **Advertisement-only** - Old code only advertised but had no way to receive messages
3. **No message routing** - Direct messages required hardcoded peer knowledge

### How it works now

1. **Windows Device A**:
   - Creates GATT service with TX/RX/MESH characteristics
   - Starts advertising with service UUID
   
2. **Windows Device B**:
   - Scans and finds Device A by service UUID
   - Connects to Device A
   - Subscribes to A's RX characteristic
   
3. **Message Exchange**:
   - B writes to A's TX characteristic → A receives via write callback
   - A notifies via RX characteristic → B receives via notify callback
   - Mesh network automatically forwards GLOBAL messages

## Linux Compatibility

All changes are **fully cross-platform**:
- Linux uses same service/characteristic UUIDs
- Python GATT server matches Windows implementation
- Message format is identical
- Mesh routing works regardless of OS

## Building & Testing

### Build Commands

**Windows:**
```powershell
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**Linux:**
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Testing Mesh Network

1. **Start Device 1** (Windows or Linux):
   ```
   echo
   > scan
   > /join #global
   ```

2. **Start Device 2** (Windows or Linux):
   ```
   echo
   > scan
   > /join #global
   ```

3. **Send message from Device 1**:
   ```
   [#global]> Hello from Device 1
   ```

4. **Device 2 should receive**:
   ```
   [alice]: Hello from Device 1
   ```

### Testing File Transfer

You'll need to add UI commands for file transfer. Example:

```cpp
// In ConsoleUI.cpp, add command handling
if (cmd.type == CommandType::SEND_FILE) {
    fileTransferManager->startFileSend(cmd.filepath, cmd.target, targetAddress, username);
}
```

## Troubleshooting

### Windows "Run as Administrator" Required

Windows restricts BLE peripheral mode. Solutions:
1. Run Echo as Administrator
2. Enable Developer Mode in Windows Settings
3. Use GATT service approach (automatically used as fallback)

### Linux Permission Denied

```bash
sudo usermod -a -G bluetooth $USER
sudo systemctl restart bluetooth
```

### No Devices Found

- Check Bluetooth is enabled
- Ensure devices are within range (< 10 meters for BLE)
- Verify firewall isn't blocking Bluetooth
- Run `scan` command and wait 10-15 seconds

### Messages Not Forwarding

- Check mesh network is initialized (automatic in new code)
- Verify devices are connected (not just discovered)
- Ensure message type is GLOBAL_MESSAGE for mesh forwarding

## Next Steps

1. **Add UI commands for file transfer**
   - `/send <filename> @username`
   - `/files` - list incoming transfers
   
2. **Implement file transfer UI in ConsoleUI**
   - Progress bars for transfers
   - Download directory management
   
3. **Add encryption to file chunks**
   - Use existing CryptoManager
   - Encrypt before Base64 encoding

4. **Optimize mesh routing**
   - Add route caching
   - Implement selective forwarding based on recipient

5. **Add peer discovery messages**
   - Periodic ANNOUNCE broadcasts
   - Automatic peer list updates

## Implementation Notes

- All code follows existing modularity (no file renames)
- No comments except labels as requested
- No emojis in code or UI
- GATT used for both advertising and messaging
- Cross-platform compatible (Windows, Linux, macOS ready)
