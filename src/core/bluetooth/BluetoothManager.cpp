#include "BluetoothManager.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>
#include <thread>

namespace echo {

BluetoothManager::BluetoothManager() 
    : isScanning_(false), isAdvertising_(false) {
    initializeAdapter();
    
#ifdef _WIN32
    windowsAdvertiser_ = std::make_unique<WindowsAdvertiser>();
#endif
#ifdef __linux__
    bluezAdvertiser_ = std::make_unique<BluezAdvertiser>();
#endif
#ifdef __APPLE__
    macosAdvertiser_ = std::make_unique<MacOSAdvertiser>();
#endif
}

BluetoothManager::~BluetoothManager() {
    stopScanning();
    stopBitChatAdvertising();
    
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
        {
            std::lock_guard<std::mutex> lock(devicesMutex_);
            discoveredDevices_.clear();
        }
        
        adapter_->set_callback_on_scan_found([this](SimpleBLE::Peripheral peripheral) {
            onPeripheralFound(std::move(peripheral));
        });
        
        adapter_->scan_start();
        isScanning_ = true;
        
        std::cout << "Started continuous Bluetooth LE scanning..." << std::endl;
        std::cout << "Looking for BLE devices (this may take 10-15 seconds)..." << std::endl;
        std::cout << "Echo devices will be automatically connected for messaging" << std::endl;
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
    
    device.isEchoDevice = parseEchoDevice(peripheral, device);
    
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = std::find_if(discoveredDevices_.begin(), discoveredDevices_.end(),
            [&device](const DiscoveredDevice& d) {
                return d.address == device.address;
            });
        
        if (it != discoveredDevices_.end()) {
            it->lastSeen = device.lastSeen;
            it->rssi = device.rssi;
            it->isEchoDevice = device.isEchoDevice;
            it->echoUsername = device.echoUsername;
            it->echoFingerprint = device.echoFingerprint;
        } else {
            discoveredDevices_.push_back(device);
        }
    }
    
    if (deviceDiscoveredCallback_) {
        deviceDiscoveredCallback_(device);
    }
    
    if (device.isEchoDevice) {
        std::cout << "Found Echo device: " << device.echoUsername 
                  << " (" << device.address << ") "
                  << "RSSI: " << device.rssi << " dBm" << std::endl;
        
        bool alreadyConnected = false;
        {
            std::lock_guard<std::mutex> lock(devicesMutex_);
            alreadyConnected = (findConnectedPeripheral(device.address) != nullptr);
        }
        
        if (device.isConnectable && !alreadyConnected) {
            std::cout << "Auto-connecting to " << device.echoUsername << "..." << std::endl;
            std::thread([this, peripheral]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                try {
                    peripheral.connect();
                    if (peripheral.is_connected()) {
                        {
                            std::lock_guard<std::mutex> lock(devicesMutex_);
                            connectedPeripherals_.push_back(peripheral);
                        }
                        std::cout << "[AUTO-CONNECTED] " << peripheral.identifier() 
                                 << " ready for messaging" << std::endl;
                        if (deviceConnectedCallback_) {
                            deviceConnectedCallback_(peripheral.address());
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[AUTO-CONNECT FAILED] " << peripheral.identifier() 
                             << ": " << e.what() << std::endl;
                }
            }).detach();
        }
    } else {
        std::cout << "Found device: " << device.name << " (" << device.address << ")"
                  << " RSSI: " << device.rssi << " dBm" << std::endl;
    }
}

bool BluetoothManager::parseEchoDevice(const SimpleBLE::Peripheral& peripheral, DiscoveredDevice& device) {
    auto& mutable_peripheral = const_cast<SimpleBLE::Peripheral&>(peripheral);
    
    bool hasEchoService = false;
    std::string parsedUsername;
    std::string parsedOS;
    
    auto mfgData = mutable_peripheral.manufacturer_data();
    for (const auto& [companyId, data] : mfgData) {
        if (companyId == 0xFFFF && !data.empty()) {
            if (data[0] == 0x11) {
                if (data.size() > 1) {
                    parsedUsername = std::string(data.begin() + 1, data.end());
                    parsedOS = "unknown";
                    std::cout << "[Parser] Found Echo manufacturer data with username: " << parsedUsername << std::endl;
                }
            }
        }
    }
    
    auto services = mutable_peripheral.services();
    for (auto& service : services) {
        std::string serviceUuid = service.uuid();
        std::transform(serviceUuid.begin(), serviceUuid.end(), serviceUuid.begin(), ::toupper);
        
        if (serviceUuid.find("F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C") != std::string::npos ||
            serviceUuid.find("F47B5E2D") != std::string::npos) {
            hasEchoService = true;
            
            auto rawServiceData = static_cast<std::vector<uint8_t>>(service.data());
            if (!rawServiceData.empty() && parsedUsername.empty()) {
                uint8_t header = rawServiceData[0];
                uint8_t version = header >> 4;
                uint8_t flags = header & 0x0F;

                if (version == 1 && rawServiceData.size() > 1) {
                    parsedUsername = std::string(rawServiceData.begin() + 1, rawServiceData.end());
                    parsedOS = (flags & 0x1) ? "windows" : "linux";
                    std::cout << "[Parser] Found Echo service data with username: " << parsedUsername << std::endl;
                }
            }
            break;
        }
    }
    
    std::string name = mutable_peripheral.identifier();
    if (name.find("Echo-") == 0 && parsedUsername.empty()) {
        size_t osStart = name.rfind('[');
        size_t osEnd = name.rfind(']');
        
        if (osStart != std::string::npos && osEnd != std::string::npos && osEnd > osStart) {
            parsedOS = name.substr(osStart + 1, osEnd - osStart - 1);
            parsedUsername = name.substr(5, osStart - 5);
        } else {
            parsedUsername = name.substr(5);
            parsedOS = "unknown";
        }
        std::cout << "[Parser] Parsed Echo username from device name: " << parsedUsername << std::endl;
    }
    
    if (hasEchoService || name.find("Echo-") == 0 || !parsedUsername.empty()) {
        device.isEchoDevice = true;
        device.echoUsername = parsedUsername.empty() ? ("Unknown-" + device.address.substr(0, 8)) : parsedUsername;
        device.osType = parsedOS.empty() ? "unknown" : parsedOS;
        device.echoFingerprint = parsedUsername.empty() ? "gatt-only" : "detected";
        
        std::cout << "[Parser] Echo device identified: " << device.echoUsername 
                  << " (" << device.osType << ")" << std::endl;
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
    auto& mutable_peripheral = const_cast<SimpleBLE::Peripheral&>(peripheral);
    
    auto services = mutable_peripheral.services();
    for (auto& service : services) {
        if (service.uuid() == BITCHAT_SERVICE_UUID) {
            return true;
        }
    }
    
    std::string name = mutable_peripheral.identifier();
    return name.find("BitChat") != std::string::npos || 
           name.find("Echo") != std::string::npos;
}

bool BluetoothManager::connectToDevice(const std::string& address) {
    if (!adapter_) {
        return false;
    }
    
    try {
        auto peripherals = adapter_->scan_get_results();
        
        for (auto& peripheral : peripherals) {
            if (peripheral.address() == address) {
                std::cout << "Connecting to device: " << address << std::endl;
                
                peripheral.connect();
                
                if (peripheral.is_connected()) {
                    std::cout << "Connection established, waiting for services..." << std::endl;
                    
                    // Windows needs longer time for service discovery
                    bool servicesReady = false;
                    for (int retry = 0; retry < 10; retry++) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(300));
                        
                        try {
                            auto services = peripheral.services();
                            if (services.size() > 0) {
                                std::cout << "Found " << services.size() << " services (retry " << retry << ")" << std::endl;
                                servicesReady = true;
                                break;
                            }
                        } catch (const std::exception& e) {
                            if (retry == 9) {
                                std::cerr << "Service discovery failed after retries: " << e.what() << std::endl;
                                peripheral.disconnect();
                                return false;
                            }
                            // Continue retrying
                        }
                    }
                    
                    if (!servicesReady) {
                        std::cerr << "Service discovery timed out" << std::endl;
                        peripheral.disconnect();
                        return false;
                    }
                    
                    std::lock_guard<std::mutex> lock(devicesMutex_);
                    connectedPeripherals_.push_back(peripheral);
                    
                    peripheral.set_callback_on_connected([this, address]() {
                        auto* p = findConnectedPeripheral(address);
                        if (p) onPeripheralConnected(*p);
                    });
                    
                    peripheral.set_callback_on_disconnected([this, address]() {
                        auto* p = findConnectedPeripheral(address);
                        if (p) onPeripheralDisconnected(*p);
                    });
                    
                    std::cout << "Successfully connected to device: " << address << std::endl;
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
    std::lock_guard<std::mutex> lock(devicesMutex_);
    auto* peripheral = findConnectedPeripheral(address);
    if (peripheral && peripheral->is_connected()) {
        peripheral->disconnect();
        
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
    std::cout << "[DEPRECATED] Use startEchoAdvertising() instead" << std::endl;
    return false;
}

void BluetoothManager::stopBitChatAdvertising() {
    stopEchoAdvertising();
}

bool BluetoothManager::startEchoAdvertising(const std::string& username, const std::string& fingerprint) {
    if (isAdvertising_) {
        std::cout << "Already advertising" << std::endl;
        return true;
    }
    
    bool success = false;
    
#ifdef _WIN32
    if (windowsAdvertiser_) {
        success = windowsAdvertiser_->startAdvertising(username, fingerprint);
    } else {
        std::cerr << "Windows advertiser not initialized" << std::endl;
    }
#endif

#ifdef __linux__
    if (bluezAdvertiser_) {
        success = bluezAdvertiser_->startAdvertising(username, fingerprint);
    } else {
        std::cerr << "BlueZ advertiser not initialized" << std::endl;
    }
#endif

#ifdef __APPLE__
    if (macosAdvertiser_) {
        success = macosAdvertiser_->startAdvertising(username, fingerprint);
    } else {
        std::cerr << "macOS advertiser not initialized" << std::endl;
    }
#endif

    if (success) {
        isAdvertising_ = true;
        std::cout << "Echo advertising started successfully" << std::endl;
    } else {
        std::cout << "Failed to start Echo advertising" << std::endl;
    }
    
    return success;
}

void BluetoothManager::stopEchoAdvertising() {
    if (!isAdvertising_) {
        return;
    }
    
#ifdef _WIN32
    if (windowsAdvertiser_) {
        windowsAdvertiser_->stopAdvertising();
    }
#endif

#ifdef __linux__
    if (bluezAdvertiser_) {
        bluezAdvertiser_->stopAdvertising();
    }
#endif

#ifdef __APPLE__
    if (macosAdvertiser_) {
        macosAdvertiser_->stopAdvertising();
    }
#endif

    isAdvertising_ = false;
    std::cout << "Echo advertising stopped" << std::endl;
}

bool BluetoothManager::isAdvertising() const {
    return isAdvertising_;
}

bool BluetoothManager::sendData(const std::string& address, const std::vector<uint8_t>& data) {
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        auto* peripheral = findConnectedPeripheral(address);
        
        if (peripheral && peripheral->is_connected()) {
            // Retry logic for Windows service access timing
            for (int retry = 0; retry < 5; retry++) {
                try {
                    std::cout << "[SEND] Accessing services for " << address << " (attempt " << (retry+1) << ")" << std::endl;
                    auto services = peripheral->services();
                    std::cout << "[SEND] Found " << services.size() << " services" << std::endl;
                    
                    for (auto& service : services) {
                        if (service.uuid() == BITCHAT_SERVICE_UUID) {
                            auto characteristics = service.characteristics();
                            for (auto& characteristic : characteristics) {
                                if (characteristic.uuid() == BITCHAT_TX_CHAR_UUID) {
                                    peripheral->write_request(service.uuid(), characteristic.uuid(), data);
                                    std::cout << "[SENT] " << data.size() << " bytes to " << address << std::endl;
                                    return true;
                                }
                            }
                        }
                    }
                    
                    std::cerr << "[SEND FAILED] No TX characteristic found for " << address << std::endl;
                    return false;
                    
                } catch (const std::exception& e) {
                    std::cerr << "[SEND] Error accessing device " << address << " (attempt " << (retry+1) << "): " << e.what() << std::endl;
                    
                    if (retry < 4) {
                        std::cout << "[SEND] Retrying after 500ms..." << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    } else {
                        std::cerr << "[SEND] All retry attempts failed" << std::endl;
                        return false;
                    }
                }
            }
        }
    }
    
    std::cout << "[SEND] Device " << address << " not connected, attempting connection..." << std::endl;
    
    bool connected = connectToDevice(address);
    if (!connected) {
        std::cerr << "[SEND FAILED] Could not connect to device " << address << std::endl;
        return false;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::lock_guard<std::mutex> lock(devicesMutex_);
    auto* peripheral = findConnectedPeripheral(address);
    if (!peripheral || !peripheral->is_connected()) {
        std::cerr << "[SEND FAILED] Device " << address << " connection lost" << std::endl;
        return false;
    }
    
    try {
        auto services = peripheral->services();
        for (auto& service : services) {
            if (service.uuid() == BITCHAT_SERVICE_UUID) {
                auto characteristics = service.characteristics();
                for (auto& characteristic : characteristics) {
                    if (characteristic.uuid() == BITCHAT_TX_CHAR_UUID) {
                        peripheral->write_request(service.uuid(), characteristic.uuid(), data);
                        std::cout << "[SENT] " << data.size() << " bytes to " << address << std::endl;
                        return true;
                    }
                }
            }
        }
        
        std::cerr << "[SEND FAILED] No TX characteristic found for " << address << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to send data to " << address << ": " << e.what() << std::endl;
    }
    
    return false;
}

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

bool BluetoothManager::broadcastMessage(const std::vector<uint8_t>& data) {
    std::cout << "[BROADCAST] Simulating mesh broadcast of " << data.size() << " bytes" << std::endl;
    std::cout << "[BROADCAST] Note: True mesh broadcasting requires BLE advertisement updates" << std::endl;
    std::cout << "[BROADCAST] For now, this is a placeholder - messages won't actually send" << std::endl;
    
    if (messageBroadcastCallback_) {
        messageBroadcastCallback_(data);
    }
    
    return true;
}

void BluetoothManager::setMessageBroadcastCallback(MessageBroadcastCallback callback) {
    messageBroadcastCallback_ = std::move(callback);
}

}