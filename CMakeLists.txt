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
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSODIUM REQUIRED libsodium)
    pkg_check_modules(LZ4 REQUIRED liblz4)
elseif(APPLE)
    find_library(COREBLUETOOTH_LIBRARY CoreBluetooth REQUIRED)
    find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
    
    if(NOT LIBSODIUM_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBSODIUM libsodium)
        endif()
    endif()
    
    if(NOT LIBSODIUM_FOUND)
        find_library(LIBSODIUM_LIBRARY NAMES sodium libsodium)
        find_path(LIBSODIUM_INCLUDE_DIR sodium.h)
        if(LIBSODIUM_LIBRARY AND LIBSODIUM_INCLUDE_DIR)
            set(LIBSODIUM_LIBRARIES ${LIBSODIUM_LIBRARY})
            set(LIBSODIUM_INCLUDE_DIRS ${LIBSODIUM_INCLUDE_DIR})
        endif()
    endif()
    
    find_library(LZ4_LIBRARY NAMES lz4 liblz4)
    find_path(LZ4_INCLUDE_DIR lz4.h)
    if(LZ4_LIBRARY AND LZ4_INCLUDE_DIR)
        set(LZ4_LIBRARIES ${LZ4_LIBRARY})
        set(LZ4_INCLUDE_DIRS ${LZ4_INCLUDE_DIR})
    endif()
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

# Source files
set(SOURCES
    src/main.cpp
    src/core/bluetooth/BluetoothManager.cpp
    src/core/crypto/UserIdentity.cpp
    src/core/protocol/MessageTypes.cpp
    src/core/commands/IRCParser.cpp
    src/ui/ConsoleUI.cpp
)

if(WIN32)
    list(APPEND SOURCES
        src/core/bluetooth/WindowsAdvertiser.cpp
    )
endif()

if(UNIX AND NOT APPLE)
    list(APPEND SOURCES
        src/core/bluetooth/BluezAdvertiser.cpp
    )
endif()

if(APPLE)
    list(APPEND SOURCES
        src/core/bluetooth/MacOSAdvertiser.mm
    )
    set_source_files_properties(src/core/bluetooth/MacOSAdvertiser.mm PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
endif()

add_executable(echo ${SOURCES})

target_link_libraries(echo ${SIMPLEBLE_TARGET})

if(UNIX AND NOT APPLE)
    target_link_libraries(echo 
        ${LIBSODIUM_LIBRARIES}
        ${LZ4_LIBRARIES}
        OpenSSL::SSL 
        OpenSSL::Crypto
        pthread
    )
    target_compile_options(echo PRIVATE ${LIBSODIUM_CFLAGS_OTHER})
elseif(WIN32)
    target_link_libraries(echo
        OpenSSL::SSL 
        OpenSSL::Crypto
        windowsapp
    )
    
    if(DEFINED VCPKG_TARGET_TRIPLET)
        find_package(unofficial-sodium CONFIG REQUIRED)
        find_package(lz4 CONFIG REQUIRED)
        target_link_libraries(echo
            unofficial-sodium::sodium
            lz4::lz4
        )
    endif()
elseif(APPLE)
    target_link_libraries(echo
        OpenSSL::SSL
        OpenSSL::Crypto
        ${COREBLUETOOTH_LIBRARY}
        ${FOUNDATION_LIBRARY}
    )
    
    if(LIBSODIUM_LIBRARIES)
        target_link_libraries(echo ${LIBSODIUM_LIBRARIES})
        target_include_directories(echo PRIVATE ${LIBSODIUM_INCLUDE_DIRS})
    endif()
    
    if(LZ4_LIBRARIES)
        target_link_libraries(echo ${LZ4_LIBRARIES})
        target_include_directories(echo PRIVATE ${LZ4_INCLUDE_DIRS})
    endif()
endif()

if(MSVC)
    target_compile_options(echo PRIVATE /W4 /await)
else()
    target_compile_options(echo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Enable debug symbols in debug builds
set_target_properties(echo PROPERTIES
    DEBUG_POSTFIX d
)