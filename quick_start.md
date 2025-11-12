# Windows Advertising - Quick Setup Checklist

## Files to Download and Place

### New Files to Add:
1. `WindowsAdvertiser.h` → `src/core/bluetooth/WindowsAdvertiser.h`
2. `WindowsAdvertiser.cpp` → `src/core/bluetooth/WindowsAdvertiser.cpp`
3. `rebuild.bat` → project root (for clean rebuilds)

### Files to Replace:
1. `BluetoothManager.h` → `src/core/bluetooth/BluetoothManager.h`
2. `BluetoothManager.cpp` → `src/core/bluetooth/BluetoothManager.cpp`
3. `main.cpp` → `src/main.cpp`

### CMakeLists.txt Updates:
Add the platform-specific sections from `CMakeLists_additions.txt` to your CMakeLists.txt

## Build Steps

### Windows:
```batch
# Option 1: Clean rebuild (recommended)
rebuild.bat

# Option 2: Quick rebuild
build.bat
```

### Linux:
```bash
# Option 1: Clean rebuild (recommended)
chmod +x rebuild.sh
./rebuild.sh

# Option 2: Quick rebuild
cd build
make -j$(nproc)
cd ..

# Option 3: Debug build
./rebuild.sh Debug
```

## Test It

1. Start Echo: `build\Release\echo.exe`
2. Look for: "Echo advertising started successfully"
3. Run `scan` command
4. Wait 10-15 seconds
5. Run `echo` command to see discovered Echo devices

## What You Get

✓ Windows devices now advertise themselves as "Echo-{username}[windows]"
✓ Other devices (Linux, Android, etc.) can discover Windows Echo
✓ Windows can still discover other Echo devices (already working)
✓ Auto-start advertising on launch
✓ Same commands and UI on Windows and Linux

## Quick Test with Two Devices

**Device 1 (Windows):**
```
echo> scan
```

**Device 2 (Linux/Another Windows):**
```
echo> scan
echo> echo
```

You should see Device 1 listed with its username!

## Troubleshooting

- **"Failed to start advertising"** → Run as Administrator first time
- **Build errors** → Make sure CMakeLists.txt has windowsapp library
- **Can't find other devices** → Wait 15+ seconds after starting scan