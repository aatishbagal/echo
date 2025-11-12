# Windows Build Instructions for Echo

## Quick Start

### First-Time Setup
Run `setup_optimized.bat` once to install all dependencies and configure the project.

### Subsequent Builds
Use `build.bat` for quick rebuilds without reinstalling dependencies.

---

## Build Scripts Overview

### 1. `setup_optimized.bat` - Smart Setup Script
- **When to use**: First time setup or when dependencies change
- **What it does**:
  - Checks for existing vcpkg installation (build/vcpkg or VCPKG_ROOT)
  - Only installs missing dependencies
  - Configures CMake if not already configured
  - Asks before reconfiguring or building

### 2. `build.bat` - Quick Build
- **When to use**: Regular development builds
- **What it does**:
  - Rebuilds the project without touching dependencies
  - Uses existing CMake configuration
  - Optionally runs the executable

### 3. `build.ps1` - PowerShell Build (More Options)
```powershell
# Basic build
.\build.ps1

# Debug build
.\build.ps1 -Debug

# Clean and rebuild
.\build.ps1 -Clean

# Build and run
.\build.ps1 -Run

# Custom configuration
.\build.ps1 -Configuration Debug -Clean -Run
```

---

## Manual Build Commands

### Using Command Prompt

#### Initial Setup (Once)
```cmd
# 1. Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git build\vcpkg
cd build\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install libsodium:x64-windows openssl:x64-windows lz4:x64-windows
cd ..\..

# 2. Setup SimpleBLE
git submodule add https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble
git submodule update --init --recursive

# 3. Configure CMake
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE="build\vcpkg\scripts\buildsystems\vcpkg.cmake"
cd ..
```

#### Regular Builds
```cmd
# Quick rebuild (from project root)
cmake --build build --config Release --parallel

# Or navigate to build directory
cd build
cmake --build . --config Release --parallel
cd ..
```

### Using PowerShell

```powershell
# Quick rebuild
cmake --build build --config Release --parallel

# Debug build
cmake --build build --config Debug --parallel

# Clean rebuild
cmake --build build --target clean
cmake --build build --config Release --parallel

# Specific target
cmake --build build --target echo --config Release
```

### Using Visual Studio

1. Open `build\echo.sln` in Visual Studio
2. Select configuration (Debug/Release) from toolbar
3. Build with `Ctrl+Shift+B` or `Build → Build Solution`
4. Run with `F5` (Debug) or `Ctrl+F5` (without debugging)

---

## Linux-Style Commands on Windows

If you prefer Linux-style commands, use PowerShell or Git Bash:

### PowerShell
```powershell
# Create alias for make
function make { cmake --build build --config Release --parallel }

# Then use like Linux
cd build
cmake ..
make
```

### Git Bash
```bash
# Almost identical to Linux
cd build/
cmake ..
make -j$(nproc)

# Or use cmake directly
cmake --build . --parallel
```

---

## Dependency Management

### vcpkg Location Priority
1. `build/vcpkg` - Project-local installation (recommended)
2. `%VCPKG_ROOT%` - System-wide installation
3. Fresh installation if neither exists

### Checking Installed Dependencies
```cmd
# List all installed packages
build\vcpkg\vcpkg list

# Check specific package
build\vcpkg\vcpkg list libsodium
```

### Updating Dependencies
```cmd
# Update vcpkg
cd build\vcpkg
git pull
.\bootstrap-vcpkg.bat

# Update packages
.\vcpkg update
.\vcpkg upgrade --no-dry-run
```

---

## Troubleshooting

### Issue: "vcpkg not found"
```cmd
# Set VCPKG_ROOT environment variable
setx VCPKG_ROOT "%CD%\build\vcpkg"
# Restart command prompt after this
```

### Issue: "Dependencies reinstall every time"
Use `build.bat` instead of `setup.bat` for regular builds.

### Issue: "CMake configuration out of date"
```cmd
# Reconfigure manually
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="vcpkg\scripts\buildsystems\vcpkg.cmake"
cd ..
```

### Issue: "Build fails with linking errors"
```cmd
# Clean and rebuild
cd build
cmake --build . --target clean
cmake --build . --config Release --parallel
```

---

## Build Configurations

### Release Build (Optimized)
```cmd
cmake --build build --config Release
```
- Optimizations enabled
- No debug symbols
- Faster execution

### Debug Build
```cmd
cmake --build build --config Debug
```
- Debug symbols included
- No optimizations
- Better for debugging

### RelWithDebInfo
```cmd
cmake --build build --config RelWithDebInfo
```
- Optimizations enabled
- Debug symbols included
- Good for profiling

### MinSizeRel
```cmd
cmake --build build --config MinSizeRel
```
- Size optimizations
- Smallest executable

---

## Environment Variables

### Optional Setup
```cmd
# Set these for convenience (optional)
setx ECHO_PROJECT "%CD%"
setx VCPKG_ROOT "%CD%\build\vcpkg"

# Then you can build from anywhere
cmake --build %ECHO_PROJECT%\build --config Release
```

---

## Tips

1. **Use `build.bat`** for day-to-day development - it's the fastest
2. **Keep vcpkg in `build/vcpkg`** to avoid global conflicts
3. **Use PowerShell** for more build options and better scripting
4. **Open in VS** for full IDE debugging experience
5. **Git Bash** works great if you prefer Linux commands

---

## Quick Reference

```cmd
# First time only
setup_optimized.bat

# Daily development
build.bat

# Manual rebuild
cd build && cmake --build . --config Release --parallel && cd ..

# Clean everything
rmdir /s /q build

# Start fresh
setup_optimized.bat
```