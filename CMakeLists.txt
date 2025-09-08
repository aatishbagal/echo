cmake_minimum_required(VERSION 3.16)
project(Echo VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSODIUM REQUIRED libsodium)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LZ4 REQUIRED liblz4)

# Add SimpleBLE
add_subdirectory(external/simpleble)

# Include directories
include_directories(src)
include_directories(${LIBSODIUM_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/core/bluetooth/BluetoothManager.cpp
    src/core/bluetooth/DeviceDiscovery.cpp
    src/core/protocol/BitChatProtocol.cpp
    src/core/crypto/CryptoManager.cpp
    src/core/mesh/MeshNetwork.cpp
    src/core/commands/IRCParser.cpp
    src/ui/ConsoleUI.cpp
)

# Create executable
add_executable(echo ${SOURCES})

# Link libraries
target_link_libraries(echo 
    simpleble
    ${LIBSODIUM_LIBRARIES}
    OpenSSL::SSL 
    OpenSSL::Crypto
    ${LZ4_LIBRARIES}
)

# Compiler flags
target_compile_options(echo PRIVATE ${LIBSODIUM_CFLAGS_OTHER})