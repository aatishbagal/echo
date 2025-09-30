#include "ConsoleUI.h"
#include "core/crypto/UserIdentity.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <chrono>

namespace echo {

ConsoleUI::ConsoleUI() : running_(false) {
}

ConsoleUI::~ConsoleUI() {
    running_ = false;
}

void ConsoleUI::run(BluetoothManager& bluetoothManager, UserIdentity& identity) {
    running_ = true;
    
    // Set up Bluetooth event callbacks
    bluetoothManager.setDeviceDiscoveredCallback(
        [this](const DiscoveredDevice& device) {
            onDeviceDiscovered(device);
        });
    
    bluetoothManager.setDeviceConnectedCallback(
        [this](const std::string& address) {
            onDeviceConnected(address);
        });
    
    bluetoothManager.setDeviceDisconnectedCallback(
        [this](const std::string& address) {
            onDeviceDisconnected(address);
        });
    
    bluetoothManager.setDataReceivedCallback(
        [this](const std::string& address, const std::vector<uint8_t>& data) {
            onDataReceived(address, data);
        });
    
    printHelp();
    
    std::string input;
    while (running_ && std::getline(std::cin, input)) {
        if (input.empty()) continue;
        
        // Trim whitespace
        input.erase(input.begin(), std::find_if(input.begin(), input.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        input.erase(std::find_if(input.rbegin(), input.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), input.end());
        
        if (input == "quit" || input == "exit") {
            running_ = false;
            break;
        }
        
        handleCommand(input, bluetoothManager, identity);
    }
}

void ConsoleUI::printHelp() const {
    std::cout << "\n=== Echo Console Commands ===" << std::endl;
    std::cout << "scan          - Start scanning for Echo/Bluetooth devices" << std::endl;
    std::cout << "stop          - Stop scanning" << std::endl;
    std::cout << "devices       - List discovered devices" << std::endl;
    std::cout << "whoami        - Show your identity" << std::endl;
    std::cout << "/nick <name>  - Change your username" << std::endl;
    std::cout << "connect <addr>- Connect to device (future feature)" << std::endl;
    std::cout << "help          - Show this help" << std::endl;
    std::cout << "quit/exit     - Exit application" << std::endl;
    std::cout << "==============================\n" << std::endl;
    std::cout << "echo> ";
}

void ConsoleUI::handleCommand(const std::string& command, BluetoothManager& bluetoothManager, UserIdentity& identity) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "scan") {
        if (bluetoothManager.startScanning()) {
            std::cout << "Started scanning for devices..." << std::endl;
        } else {
            std::cout << "Failed to start scanning" << std::endl;
        }
    }
    else if (cmd == "stop") {
        bluetoothManager.stopScanning();
        std::cout << "Stopped scanning" << std::endl;
    }
    else if (cmd == "devices") {
        printDevices(bluetoothManager);
    }
    else if (cmd == "whoami") {
        std::cout << "\nYour Echo Identity:" << std::endl;
        std::cout << "  Username: " << identity.getUsername() << std::endl;
        std::cout << "  Fingerprint: " << identity.getFingerprint() << std::endl;
        std::cout << std::endl;
    }
    else if (cmd == "/nick") {
        std::string newUsername;
        iss >> newUsername;
        if (newUsername.empty()) {
            std::cout << "Usage: /nick <new_username>" << std::endl;
        } else {
            identity.setUsername(newUsername);
            identity.saveToFile("echo_identity.dat");
            std::cout << "Username changed to: " << newUsername << std::endl;
            std::cout << "Note: Restart Echo for the new name to be advertised" << std::endl;
        }
    }
    else if (cmd == "connect") {
        std::string address;
        iss >> address;
        if (address.empty()) {
            std::cout << "Usage: connect <device_address>" << std::endl;
        } else {
            std::cout << "Note: Echo uses mesh networking - connections not required for messaging" << std::endl;
            std::cout << "Direct connections will be implemented for file transfers" << std::endl;
        }
    }
    else if (cmd == "help") {
        printHelp();
        return; // Don't print prompt twice
    }
    else {
        std::cout << "Unknown command: " << cmd << ". Type 'help' for available commands." << std::endl;
    }
    
    std::cout << "echo> ";
}

void ConsoleUI::printDevices(const BluetoothManager& bluetoothManager) const {
    auto devices = bluetoothManager.getDiscoveredDevices();
    
    if (devices.empty()) {
        std::cout << "No devices discovered. Run 'scan' to search for devices." << std::endl;
        return;
    }
    
    // Separate Echo devices from regular BT devices
    std::vector<const DiscoveredDevice*> echoDevices;
    std::vector<const DiscoveredDevice*> regularDevices;
    
    for (const auto& device : devices) {
        if (device.isEchoDevice) {
            echoDevices.push_back(&device);
        } else {
            regularDevices.push_back(&device);
        }
    }
    
    // Show Echo devices first
    if (!echoDevices.empty()) {
        std::cout << "\n=== Echo Network Devices ===" << std::endl;
        std::cout << std::left << std::setw(20) << "Username" 
                  << std::setw(12) << "Fingerprint"
                  << std::setw(18) << "Address" 
                  << std::setw(8) << "RSSI"
                  << "Last Seen" << std::endl;
        std::cout << std::string(75, '-') << std::endl;
        
        for (const auto* device : echoDevices) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - device->lastSeen).count();
            
            std::string fingerprint = device->echoFingerprint.substr(0, 8) + "...";
            
            std::cout << std::left << std::setw(20) << device->echoUsername
                      << std::setw(12) << fingerprint
                      << std::setw(18) << device->address
                      << std::setw(8) << device->rssi
                      << elapsed << "s ago" << std::endl;
        }
        std::cout << "==========================\n" << std::endl;
    }
    
    // Show regular Bluetooth devices
    if (!regularDevices.empty()) {
        std::cout << "\n=== Other Bluetooth Devices ===" << std::endl;
        std::cout << std::left << std::setw(20) << "Name" 
                  << std::setw(18) << "Address" 
                  << std::setw(8) << "RSSI"
                  << "Last Seen" << std::endl;
        std::cout << std::string(65, '-') << std::endl;
        
        for (const auto* device : regularDevices) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - device->lastSeen).count();
            
            std::cout << std::left << std::setw(20) << device->name.substr(0, 19)
                      << std::setw(18) << device->address
                      << std::setw(8) << device->rssi
                      << elapsed << "s ago" << std::endl;
        }
        std::cout << "==============================\n" << std::endl;
    }
}

void ConsoleUI::onDeviceDiscovered(const DiscoveredDevice& device) {
    if (device.isEchoDevice) {
        std::cout << "\n[ECHO DEVICE] " << device.echoUsername 
                  << " [" << device.echoFingerprint.substr(0, 8) << "...] "
                  << "(" << device.address << ") "
                  << "RSSI: " << device.rssi << " dBm" << std::endl;
    } else {
        std::cout << "\n[BLUETOOTH] " << device.name << " (" << device.address << ") "
                  << "RSSI: " << device.rssi << " dBm" << std::endl;
    }
    std::cout << "echo> ";
    std::cout.flush();
}

void ConsoleUI::onDeviceConnected(const std::string& address) {
    std::cout << "\n[CONNECTED] Device " << address << " connected successfully" << std::endl;
    std::cout << "echo> ";
    std::cout.flush();
}

void ConsoleUI::onDeviceDisconnected(const std::string& address) {
    std::cout << "\n[DISCONNECTED] Device " << address << " disconnected" << std::endl;
    std::cout << "echo> ";
    std::cout.flush();
}

void ConsoleUI::onDataReceived(const std::string& address, const std::vector<uint8_t>& data) {
    std::cout << "\n[DATA] Received " << data.size() << " bytes from " << address << ": ";
    
    // Print data as hex for now
    for (size_t i = 0; i < std::min(data.size(), size_t(16)); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }
    if (data.size() > 16) {
        std::cout << "...";
    }
    std::cout << std::dec << std::endl;
    std::cout << "echo> ";
    std::cout.flush();
}

} // namespace echo