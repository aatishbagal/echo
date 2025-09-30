cmake_minimum_required(VERSION 3.16)
project(Echo VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSODIUM REQUIRED libsodium)
    pkg_check_modules(LZ4 REQUIRED liblz4)
elseif(APPLE)
    # macOS-specific settings
endif()

# Find common packages
find_package(OpenSSL REQUIRED)

# Add SimpleBLE as subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/simpleble/simpleble/CMakeLists.txt")
    add_subdirectory(external/simpleble/simpleble)
    set(SIMPLEBLE_TARGET "simpleble")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/simpleble/CMakeLists.txt")
    add_subdirectory(external/simpleble)
    set(SIMPLEBLE_TARGET "simpleble")
else()
    message(FATAL_ERROR "SimpleBLE not found. Run: git clone --recursive https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble")
endif()

# Include directories
include_directories(src)

# Source files (start minimal, expand as we build)
set(SOURCES
    src/main.cpp
    src/core/bluetooth/BluetoothManager.cpp
    src/core/crypto/UserIdentity.cpp
    src/ui/ConsoleUI.cpp
)

# Create executable
add_executable(echo ${SOURCES})

# Link libraries
target_link_libraries(echo ${SIMPLEBLE_TARGET})

if(UNIX AND NOT APPLE)
    target_link_libraries(echo 
        ${LIBSODIUM_LIBRARIES}
        ${LZ4_LIBRARIES}
        OpenSSL::SSL 
        OpenSSL::Crypto
    )
    target_compile_options(echo PRIVATE ${LIBSODIUM_CFLAGS_OTHER})
elseif(WIN32)
    # Windows libraries will be added via vcpkg
    target_link_libraries(echo
        OpenSSL::SSL 
        OpenSSL::Crypto
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(echo PRIVATE /W4)
else()
    target_compile_options(echo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Enable debug symbols in debug builds
set_target_properties(echo PROPERTIES
    DEBUG_POSTFIX d
)