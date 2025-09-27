#!/bin/bash

# Echo Setup Script for Linux
# Supports Fedora/RHEL and Ubuntu/Debian

set -e

echo "==============================================="
echo "Echo - BitChat Compatible Desktop Messaging"
echo "Setup Script for Linux"
echo "==============================================="

# Detect distribution
if [ -f /etc/redhat-release ]; then
    DISTRO="fedora"
    echo "Detected: Red Hat-based distribution"
elif [ -f /etc/debian_version ]; then
    DISTRO="debian"
    echo "Detected: Debian-based distribution"
else
    echo "Warning: Unknown distribution. Assuming Debian-based."
    DISTRO="debian"
fi

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo "Error: Please run this script as a normal user (not root)"
    exit 1
fi

# Function to install dependencies
install_dependencies() {
    echo "Installing dependencies..."
    
    if [ "$DISTRO" = "fedora" ]; then
        # Check if we have DNF5 or classic DNF
        if dnf --version 2>/dev/null | grep -q "dnf5"; then
            echo "Detected DNF5, installing packages directly..."
            sudo dnf install -y gcc gcc-c++ make cmake git bluez-libs-devel libsodium-devel openssl-devel lz4-devel pkg-config dbus-devel systemd-devel
        else
            echo "Detected classic DNF, using group install..."
            sudo dnf groupinstall -y "Development Tools" "Development Libraries"
            sudo dnf install -y cmake git bluez-libs-devel libsodium-devel openssl-devel lz4-devel pkg-config dbus-devel
        fi
    else
        sudo apt update
        sudo apt install -y build-essential cmake git libbluetooth-dev libsodium-dev libssl-dev liblz4-dev pkg-config
    fi
    
    echo "Dependencies installed successfully!"
}

# Function to setup SimpleBLE submodule
setup_simpleble() {
    echo "Setting up SimpleBLE..."
    
    # Clean up any broken submodule state
    git rm --cached external/simpleble 2>/dev/null || true
    rm -rf external/simpleble
    rm -f .gitmodules
    
    # Try submodule approach first
    if git submodule add https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble 2>/dev/null; then
        git submodule update --init --recursive
        echo "SimpleBLE added as submodule"
    else
        echo "Submodule failed, cloning directly..."
        git clone --recursive https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble
    fi
    
    # Verify SimpleBLE was downloaded correctly (check both possible locations)
    if [ -f "external/simpleble/simpleble/CMakeLists.txt" ]; then
        echo "✓ SimpleBLE setup complete! (found in simpleble/simpleble/)"
    elif [ -f "external/simpleble/CMakeLists.txt" ]; then
        echo "✓ SimpleBLE setup complete! (found in root)"
    else
        echo "✗ SimpleBLE setup failed - CMakeLists.txt not found"
        echo "Contents of external/simpleble/:"
        ls -la external/simpleble/ || echo "Directory doesn't exist"
        exit 1
    fi
}

# Function to create directory structure
create_directories() {
    echo "Creating directory structure..."
    
    mkdir -p src/core/bluetooth
    mkdir -p src/core/protocol  
    mkdir -p src/core/crypto
    mkdir -p src/core/mesh
    mkdir -p src/core/commands
    mkdir -p src/ui
    mkdir -p src/utils
    mkdir -p external
    mkdir -p tests
    mkdir -p docs
    mkdir -p build
    
    echo "Directory structure created!"
}

# Function to check Bluetooth support
check_bluetooth() {
    echo "Checking Bluetooth support..."
    
    if command -v bluetoothctl &> /dev/null; then
        echo "✓ bluetoothctl found"
    else
        echo "⚠ bluetoothctl not found - installing bluez"
        if [ "$DISTRO" = "fedora" ]; then
            sudo dnf install -y bluez bluez-tools
        else
            sudo apt install -y bluez bluez-tools
        fi
    fi
    
    # Check if Bluetooth service is running
    if systemctl is-active --quiet bluetooth; then
        echo "✓ Bluetooth service is running"
    else
        echo "⚠ Starting Bluetooth service"
        sudo systemctl start bluetooth
        sudo systemctl enable bluetooth
    fi
    
    echo "Bluetooth check complete!"
}

# Function to build the project
build_project() {
    echo "Building Echo..."
    
    cd build
    
    # Clean any previous build
    rm -rf *
    
    # Run cmake
    if cmake ..; then
        echo "✓ CMake configuration successful"
    else
        echo "✗ CMake configuration failed"
        echo ""
        echo "Debugging information:"
        echo "SimpleBLE locations check:"
        ls -la ../external/simpleble/simpleble/CMakeLists.txt 2>/dev/null && echo "Found: simpleble/simpleble/CMakeLists.txt" || echo "Not found: simpleble/simpleble/CMakeLists.txt"
        ls -la ../external/simpleble/CMakeLists.txt 2>/dev/null && echo "Found: simpleble/CMakeLists.txt" || echo "Not found: simpleble/CMakeLists.txt"
        exit 1
    fi
    
    # Build
    if make -j$(nproc); then
        echo "✓ Build successful!"
        echo ""
        echo "Echo has been built successfully!"
        echo "Run with: ./build/echo"
    else
        echo "✗ Build failed"
        exit 1
    fi
    
    cd ..
}

# Function to set up development environment
setup_dev_env() {
    echo "Setting up development environment..."
    
    # Create .gitignore if it doesn't exist
    if [ ! -f .gitignore ]; then
        cat > .gitignore << 'EOF'
# Build directories
build/
*build*/

# IDE files
.vscode/
.idea/
*.swp
*.swo
*~

# OS files
.DS_Store
Thumbs.db

# Compiled binaries
*.exe
*.dll
*.so
*.dylib
*.a

# Debug files
*.pdb
*.dSYM/

# Temporary files
*.tmp
*.log
EOF
        echo "Created .gitignore"
    fi
    
    # Create basic VS Code configuration if .vscode doesn't exist
    if [ ! -d ".vscode" ]; then
        mkdir -p .vscode
        cat > .vscode/settings.json << 'EOF'
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "files.associations": {
        "*.h": "cpp",
        "*.cpp": "cpp"
    }
}
EOF
        echo "Created VS Code configuration"
    fi
}

# Main execution
main() {
    echo "Starting Echo setup..."
    echo ""
    
    # Check if we're in the right directory
    if [ ! -f "CMakeLists.txt" ]; then
        echo "Error: CMakeLists.txt not found. Are you in the Echo project directory?"
        exit 1
    fi
    
    # Initialize git if not already done
    if [ ! -d ".git" ]; then
        echo "Initializing git repository..."
        git init
        git add .
        git commit -m "Initial Echo project setup"
        echo ""
    fi
    
    # Run setup steps
    install_dependencies
    echo ""
    
    create_directories
    echo ""
    
    setup_dev_env
    echo ""
    
    setup_simpleble
    echo ""
    
    check_bluetooth
    echo ""
    
    build_project
    echo ""
    
    echo "==============================================="
    echo "Setup complete! 🎉"
    echo ""
    echo "Next steps:"
    echo "1. Run Echo: ./build/echo"
    echo "2. Test Bluetooth scanning: Type 'scan' in Echo"
    echo "3. Check docs/ for development guides"
    echo ""
    echo "Happy coding!"
    echo "==============================================="
}

# Run main function
main "$@"