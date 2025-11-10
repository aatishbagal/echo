# macOS BLE Advertising Implementation

## Overview

Echo now supports **full BLE advertising on macOS** using CoreBluetooth's `CBPeripheralManager`. This enables macOS instances to be fully discoverable by Linux, Windows, and other macOS Echo clients.

## Architecture

### Components

1. **MacOSAdvertiser.h** - Platform-specific advertiser interface
2. **MacOSAdvertiser.mm** - Objective-C++ implementation using CoreBluetooth
3. **BluetoothManager** - Integration with main BLE manager

### Design Pattern

The macOS advertiser follows the same pattern as Windows and Linux:

```
BluetoothManager
├── WindowsAdvertiser (WinRT APIs)
├── BluezAdvertiser (D-Bus/Python)
└── MacOSAdvertiser (CoreBluetooth)
```

## CoreBluetooth APIs Used

### CBPeripheralManager
Main peripheral role manager for advertising and GATT server operations.

```objective-c
CBPeripheralManager* peripheralManager = [[CBPeripheralManager alloc] 
    initWithDelegate:delegate 
    queue:queue
    options:nil];
```

### CBMutableService
Defines the Echo service with UUID `F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C`.

```objective-c
CBUUID* serviceUUID = [CBUUID UUIDWithString:@"F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C"];
CBMutableService* echoService = [[CBMutableService alloc] 
    initWithType:serviceUUID 
    primary:YES];
```

### CBMutableCharacteristic
Defines characteristics for data exchange (TX/RX).

```objective-c
CBMutableCharacteristic* characteristic = [[CBMutableCharacteristic alloc]
    initWithType:[CBUUID UUIDWithString:@"8E9B7A4C-2D5F-4B6A-9C3E-1F8D7B2A5C4E"]
    properties:CBCharacteristicPropertyRead | CBCharacteristicPropertyWrite | CBCharacteristicPropertyNotify
    value:nil
    permissions:CBAttributePermissionsReadable | CBAttributePermissionsWriteable];
```

### Advertisement Data
Broadcasts service UUID and local name.

```objective-c
NSDictionary* advertisingData = @{
    CBAdvertisementDataServiceUUIDsKey: @[serviceUUID],
    CBAdvertisementDataLocalNameKey: @"Echo-username[macos]"
};

[peripheralManager startAdvertising:advertisingData];
```

## Implementation Details

### Initialization Flow

1. Create `PeripheralDelegate` (Objective-C class)
2. Initialize `CBPeripheralManager` with delegate
3. Wait for `peripheralManagerDidUpdateState:` callback
4. Check for `CBManagerStatePoweredOn` state

### Advertising Flow

1. Create service UUID from Echo service ID
2. Create characteristic with read/write/notify properties
3. Create `CBMutableService` and add characteristic
4. Add service to peripheral manager
5. Start advertising with service UUID and local name
6. Wait for `peripheralManagerDidStartAdvertising:error:` callback

### Cleanup Flow

1. Stop advertising if active
2. Remove all services
3. Deallocate peripheral manager and delegate

### Thread Safety

- Uses dispatch queue for CoreBluetooth operations
- Atomic boolean for advertising state
- Timeout-based waiting for state changes

## Permissions

### Info.plist Required Keys

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Echo needs Bluetooth to discover and communicate with nearby devices</string>

<key>NSBluetoothPeripheralUsageDescription</key>
<string>Echo needs Bluetooth peripheral mode to be discoverable by other devices</string>
```

These are already included in the project's `Info.plist`.

### Runtime Permissions

On first run, macOS will prompt:
- "Echo would like to use Bluetooth"
- User must click "Allow" to enable advertising

If denied, advertising will fail silently. Users can grant permission later in:
**System Preferences → Security & Privacy → Privacy → Bluetooth**

## Platform Differences

| Aspect | Windows | Linux | macOS |
|--------|---------|-------|-------|
| **API** | WinRT | D-Bus | CoreBluetooth |
| **Language** | C++ | C++/Python | Objective-C++ |
| **Architecture** | Native async | Process spawn | Delegate pattern |
| **Init Time** | ~100ms | ~500ms | ~500ms |
| **File Extension** | .cpp | .cpp | .mm |

## Testing

### Building

```bash
cd echo
./setup_macos.sh    # First time
./rebuild_macos.sh  # Subsequent builds
```

### Running

```bash
./run_echo_macos.sh
```

Or directly:
```bash
./build/echo
```

### Verification

1. Start Echo on macOS
2. Use `/nick yourname` to set username
3. Echo will auto-start advertising
4. On another device (Linux/Windows/macOS), run `scan`
5. Should see `Echo-yourname[macos]` appear

### Debug Output

```
[macOS Advertiser] Bluetooth is powered on and ready
[macOS Advertiser] Service added successfully
[macOS Advertiser] Successfully started advertising
[macOS Advertiser] Broadcasting as: Echo-yourname[macos]
[macOS Advertiser] Service UUID: F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C
```

## Known Limitations

1. **Requires macOS 10.15+**: CoreBluetooth peripheral mode
2. **No background mode**: Terminal app doesn't support background BLE
3. **Single service**: Only Echo service advertised (future: multiple services)
4. **Manual start**: No auto-resume after sleep/wake

## Future Enhancements

- [ ] Background mode support (requires GUI app)
- [ ] Automatic permission checking
- [ ] Connection state management
- [ ] Multiple GATT services
- [ ] Enhanced error recovery
- [ ] Power management optimization

## Files Modified

1. **Created:**
   - `src/core/bluetooth/MacOSAdvertiser.h`
   - `src/core/bluetooth/MacOSAdvertiser.mm`
   - `docs/MACOS_ADVERTISING.md`

2. **Modified:**
   - `src/core/bluetooth/BluetoothManager.h` (added macOS advertiser member)
   - `src/core/bluetooth/BluetoothManager.cpp` (integrated macOS advertiser)
   - `CMakeLists.txt` (added .mm file, Objective-C++ flags)
   - `MACOS_BUILD.md` (updated to reflect advertising support)
   - `setup_macos.sh` (removed limitation warning)

## References

- [CoreBluetooth Framework](https://developer.apple.com/documentation/corebluetooth)
- [CBPeripheralManager](https://developer.apple.com/documentation/corebluetooth/cbperipheralmanager)
- [SimpleBLE Library](https://github.com/OpenBluetoothToolbox/SimpleBLE)
- [Echo Protocol Specification](../README.md)
