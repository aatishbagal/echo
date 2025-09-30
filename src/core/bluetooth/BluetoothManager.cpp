#include "BluetoothManager.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <mutex>

namespace echo {

BluetoothManager::BluetoothManager() 
    : isScanning_(false), isAdvertising_(false) {
    initializeAdapter();
}

BluetoothManager::~BluetoothManager() {
    stopScanning();
    stopBitChatAdvertising();
    
    // Disconnect all peripherals
    for (auto& peripheral : connectedPeripherals_) {
        if (peripheral.is_connected()) {
            peripheral.disconnect();
        }
    }
}

void BluetoothManager::initializeAdapter() {
    auto adapters = SimpleBLE::Adapter::get_adapters();
    
    if (adapters.empty()) {
        throw std::runtime_error("No Bluetooth adapters found");
    }
    
    // Use the first available adapter
    adapter_ = std::make_shared<SimpleBLE::Adapter>(adapters[0]);
    
    std::cout << "Using Bluetooth adapter: " << adapter_->identifier() 
              << " (" << adapter_->address() << ")" << std::endl;
}

bool BluetoothManager::isBluetoothAvailable() const {
    return adapter_ != nullptr && adapter_->initialized();
}

bool BluetoothManager::startScanning() {
    if (!adapter_ || isScanning_) {
        return false;
    }
    
    try {
        // Clear previous devices
        {
            std::lock_guard<std::mutex> lock(devicesMutex_);
            discoveredDevices_.clear();
        }
        
        // Set up scan callback
        adapter_->set_callback_on_scan_found([this](SimpleBLE::Peripheral peripheral) {
            onPeripheralFound(std::move(peripheral));
        });
        
        // Start continuous scanning (no timeout for better detection)
        adapter_->scan_start();
        isScanning_ = true;
        
        std::cout << "Started continuous Bluetooth LE scanning..." << std::endl;
        std::cout << "Looking for BLE devices (this may take 10-15 seconds)..." << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to start scanning: " << e.what() << std::endl;
        return false;
    }
}

void BluetoothManager::stopScanning() {
    if (!adapter_ || !isScanning_) {
        return;
    }
    
    try {
        adapter_->scan_stop();
        isScanning_ = false;
        std::cout << "Stopped Bluetooth scanning" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error stopping scan: " << e.what() << std::endl;
    }
}

bool BluetoothManager::isScanning() const {
    return isScanning_;
}

std::vector<DiscoveredDevice> BluetoothManager::getDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    return discoveredDevices_;
}

void BluetoothManager::onPeripheralFound(SimpleBLE::Peripheral peripheral) {
    DiscoveredDevice device;
    device.address = peripheral.address();
    device.name = peripheral.identifier();
    device.rssi = peripheral.rssi();
    device.isConnectable = peripheral.is_connectable();
    device.lastSeen = std::chrono::steady_clock::now();
    device.isEchoDevice = false;
    
    // Check if this is a BitChat/Echo device and parse Echo-specific data
    device.isEchoDevice = parseEchoDevice(peripheral, device);
    
    // Update discovered devices list
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        // Check if device already exists
        auto it = std::find_if(discoveredDevices_.begin(), discoveredDevices_.end(),
            [&device](const DiscoveredDevice& d) {
                return d.address == device.address;
            });
        
        if (it != discoveredDevices_.end()) {
            // Update existing device
            it->lastSeen = device.lastSeen;
            it->rssi = device.rssi;
            it->isEchoDevice = device.isEchoDevice;
            it->echoUsername = device.echoUsername;
            it->echoFingerprint = device.echoFingerprint;
        } else {
            // Add new device
            discoveredDevices_.push_back(device);
        }
    }
    
    // Notify callback if this is an Echo device or if callback wants all devices
    if (deviceDiscoveredCallback_) {
        deviceDiscoveredCallback_(device);
    }
    
    // Print discovery info
    if (device.isEchoDevice) {
        std::cout << "Found Echo device: " << device.echoUsername 
                  << " [" << device.echoFingerprint.substr(0, 8) << "] "
                  << "(" << device.address << ") "
                  << "RSSI: " << device.rssi << " dBm" << std::endl;
    } else {
        std::cout << "Found device: " << device.name << " (" << device.address << ")"
                  << " RSSI: " << device.rssi << " dBm" << std::endl;
    }
}

bool BluetoothManager::parseEchoDevice(const SimpleBLE::Peripheral& peripheral, DiscoveredDevice& device) {
    // We need to cast away const to call SimpleBLE methods
    auto& mutable_peripheral = const_cast<SimpleBLE::Peripheral&>(peripheral);
    
    // Check if the device advertises BitChat service UUID
    auto services = mutable_peripheral.services();
    for (auto& service : services) {
        std::string serviceUuid = service.uuid();
        
        // Convert to uppercase for comparison (UUIDs are case-insensitive)
        std::transform(serviceUuid.begin(), serviceUuid.end(), serviceUuid.begin(), ::toupper);
        
        // Check for BitChat service UUID
        if (serviceUuid.find("F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C") != std::string::npos ||
            serviceUuid.find("F47B5E2D") != std::string::npos) {
            
            // This is a BitChat device!
            std::string name = mutable_peripheral.identifier();
            
            // BitChat doesn't advertise device name, so use peer ID from address
            // Extract peer ID (would come from characteristic read in full implementation)
            device.echoUsername = "BitChat-" + device.address.substr(0, 8);
            device.echoFingerprint = "detected";
            
            std::cout << "  [DEBUG] Found BitChat service UUID in device!" << std::endl;
            return true;
        }
    }
    
    // Also check device name as fallback
    std::string name = mutable_peripheral.identifier();
    if (name.find("Echo:") == 0 || name.find("BitChat:") == 0 || name.find("BitChat") != std::string::npos) {
        // Parse name format if present: "Echo:Username:Fingerprint"
        size_t firstColon = name.find(':');
        size_t secondColon = name.find(':', firstColon + 1);
        
        if (firstColon != std::string::npos && secondColon != std::string::npos) {
            device.echoUsername = name.substr(firstColon + 1, secondColon - firstColon - 1);
            device.echoFingerprint = name.substr(secondColon + 1);
        } else {
            device.echoUsername = name;
            device.echoFingerprint = "unknown";
        }
        return true;
    }
    
    return false;
}

std::vector<DiscoveredDevice> BluetoothManager::getEchoDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    std::vector<DiscoveredDevice> echoDevices;
    
    for (const auto& device : discoveredDevices_) {
        if (device.isEchoDevice) {
            echoDevices.push_back(device);
        }
    }
    
    return echoDevices;
}

bool BluetoothManager::isBitChatDevice(const SimpleBLE::Peripheral& peripheral) const {
    // We need to cast away const to call SimpleBLE methods
    // This is a limitation of SimpleBLE's API design
    auto& mutable_peripheral = const_cast<SimpleBLE::Peripheral&>(peripheral);
    
    // Check if the device advertises BitChat service UUID
    auto services = mutable_peripheral.services();
    for (auto& service : services) {
        if (service.uuid() == BITCHAT_SERVICE_UUID) {
            return true;
        }
    }
    
    // Alternative check: look for specific manufacturer data or device name patterns
    std::string name = mutable_peripheral.identifier();
    return name.find("BitChat") != std::string::npos || 
           name.find("Echo") != std::string::npos;
}

bool BluetoothManager::connectToDevice(const std::string& address) {
    if (!adapter_) {
        return false;
    }
    
    try {
        // Find the peripheral in our discovered devices
        auto peripherals = adapter_->scan_get_results();
        
        for (auto& peripheral : peripherals) {
            if (peripheral.address() == address) {
                peripheral.connect();
                
                if (peripheral.is_connected()) {
                    connectedPeripherals_.push_back(peripheral);
                    
                    // Set up connection callbacks
                    peripheral.set_callback_on_connected([this, address]() {
                        onPeripheralConnected(*findConnectedPeripheral(address));
                    });
                    
                    peripheral.set_callback_on_disconnected([this, address]() {
                        onPeripheralDisconnected(*findConnectedPeripheral(address));
                    });
                    
                    std::cout << "Connected to device: " << address << std::endl;
                    return true;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to connect to device " << address << ": " << e.what() << std::endl;
    }
    
    return false;
}

void BluetoothManager::disconnectFromDevice(const std::string& address) {
    auto* peripheral = findConnectedPeripheral(address);
    if (peripheral && peripheral->is_connected()) {
        peripheral->disconnect();
        
        // Remove from connected list
        connectedPeripherals_.erase(
            std::remove_if(connectedPeripherals_.begin(), connectedPeripherals_.end(),
                [&address](SimpleBLE::Peripheral& p) {
                    return p.address() == address;
                }),
            connectedPeripherals_.end()
        );
        
        std::cout << "Disconnected from device: " << address << std::endl;
    }
}

void BluetoothManager::onPeripheralConnected(SimpleBLE::Peripheral peripheral) {
    if (deviceConnectedCallback_) {
        deviceConnectedCallback_(peripheral.address());
    }
}

void BluetoothManager::onPeripheralDisconnected(SimpleBLE::Peripheral peripheral) {
    if (deviceDisconnectedCallback_) {
        deviceDisconnectedCallback_(peripheral.address());
    }
}

SimpleBLE::Peripheral* BluetoothManager::findConnectedPeripheral(const std::string& address) {
    auto it = std::find_if(connectedPeripherals_.begin(), connectedPeripherals_.end(),
        [&address](SimpleBLE::Peripheral& p) {
            return p.address() == address;
        });
    
    return it != connectedPeripherals_.end() ? &(*it) : nullptr;
}

bool BluetoothManager::startBitChatAdvertising() {
    // TODO: Implement BLE advertising to announce this device as BitChat compatible
    // This will require platform-specific implementation or SimpleBLE peripheral mode
    std::cout << "BitChat advertising not yet implemented" << std::endl;
    return false;
}

void BluetoothManager::stopBitChatAdvertising() {
    isAdvertising_ = false;
}

bool BluetoothManager::sendData(const std::string& address, const std::vector<uint8_t>& data) {
    auto* peripheral = findConnectedPeripheral(address);
    if (!peripheral || !peripheral->is_connected()) {
        return false;
    }
    
    try {
        // Find BitChat message characteristic
        auto services = peripheral->services();
        for (auto& service : services) {
            if (service.uuid() == BITCHAT_SERVICE_UUID) {
                auto characteristics = service.characteristics();
                for (auto& characteristic : characteristics) {
                    if (characteristic.uuid() == BITCHAT_TX_CHAR_UUID) {
                        peripheral->write_request(service.uuid(), characteristic.uuid(), data);
                        return true;
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to send data to " << address << ": " << e.what() << std::endl;
    }
    
    return false;
}

// Callback setters
void BluetoothManager::setDeviceDiscoveredCallback(DeviceDiscoveredCallback callback) {
    deviceDiscoveredCallback_ = std::move(callback);
}

void BluetoothManager::setDeviceConnectedCallback(DeviceConnectedCallback callback) {
    deviceConnectedCallback_ = std::move(callback);
}

void BluetoothManager::setDeviceDisconnectedCallback(DeviceDisconnectedCallback callback) {
    deviceDisconnectedCallback_ = std::move(callback);
}

void BluetoothManager::setDataReceivedCallback(DataReceivedCallback callback) {
    dataReceivedCallback_ = std::move(callback);
}

} // namespace echo