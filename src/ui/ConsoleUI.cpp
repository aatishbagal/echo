#include "ConsoleUI.h"
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

void ConsoleUI::run(BluetoothManager& bluetoothManager) {
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
        
        handleCommand(input, bluetoothManager);
    }
}

void ConsoleUI::printHelp() const {
    std::cout << "\n=== Echo Console Commands ===" << std::endl;
    std::cout << "scan          - Start scanning for BitChat devices" << std::endl;
    std::cout << "stop          - Stop scanning" << std::endl;
    std::cout << "devices       - List discovered devices" << std::endl;
    std::cout << "connect <addr>- Connect to device by address" << std::endl;
    std::cout << "disconnect <addr> - Disconnect from device" << std::endl;
    std::cout << "help          - Show this help" << std::endl;
    std::cout << "quit/exit     - Exit application" << std::endl;
    std::cout << "==============================\n" << std::endl;
    std::cout << "echo> ";
}

void ConsoleUI::handleCommand(const std::string& command, BluetoothManager& bluetoothManager) {
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
    else if (cmd == "connect") {
        std::string address;
        iss >> address;
        if (address.empty()) {
            std::cout << "Usage: connect <device_address>" << std::endl;
        } else {
            if (bluetoothManager.connectToDevice(address)) {
                std::cout << "Attempting to connect to " << address << std::endl;
            } else {
                std::cout << "Failed to connect to " << address << std::endl;
            }
        }
    }
    else if (cmd == "disconnect") {
        std::string address;
        iss >> address;
        if (address.empty()) {
            std::cout << "Usage: disconnect <device_address>" << std::endl;
        } else {
            bluetoothManager.disconnectFromDevice(address);
            std::cout << "Disconnected from " << address << std::endl;
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
    
    std::cout << "\n=== Discovered Devices ===" << std::endl;
    std::cout << std::left << std::setw(20) << "Name" 
              << std::setw(18) << "Address" 
              << std::setw(8) << "RSSI" 
              << std::setw(12) << "Connectable"
              << "Last Seen" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    
    for (const auto& device : devices) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - device.lastSeen).count();
        
        std::cout << std::left << std::setw(20) << device.name.substr(0, 19)
                  << std::setw(18) << device.address
                  << std::setw(8) << device.rssi
                  << std::setw(12) << (device.isConnectable ? "Yes" : "No")
                  << elapsed << "s ago" << std::endl;
    }
    std::cout << "========================\n" << std::endl;
}

void ConsoleUI::onDeviceDiscovered(const DiscoveredDevice& device) {
    std::cout << "\n[DISCOVERED] " << device.name << " (" << device.address << ") "
              << "RSSI: " << device.rssi << " dBm" << std::endl;
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