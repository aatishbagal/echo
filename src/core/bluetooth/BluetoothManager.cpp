#include "BluetoothManager.h"
#include "../mesh/MeshNetwork.h"
#include "../protocol/MessageTypes.h"
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
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Bluetooth Adapter Ready" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Name:    " << adapter_->identifier() << std::endl;
    std::cout << "Address: " << adapter_->address() << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nTo connect from another device, use:" << std::endl;
    std::cout << "  connect " << adapter_->address() << std::endl;
    std::cout << "========================================\n" << std::endl;
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
    
    std::string name = mutable_peripheral.identifier();
    
    if (name.find("Echo-") == 0) {
        size_t osStart = name.rfind('[');
        size_t osEnd = name.rfind(']');
        
        if (osStart != std::string::npos && osEnd != std::string::npos && osEnd > osStart) {
            device.osType = name.substr(osStart + 1, osEnd - osStart - 1);
            device.echoUsername = name.substr(5, osStart - 5);
        } else {
            device.echoUsername = name.substr(5);
            device.osType = "unknown";
        }
        device.echoFingerprint = "name";
        std::cout << "[Discovery] Echo device by name: " << device.echoUsername << std::endl;
        return true;
    }
    
    auto manufacturerData = mutable_peripheral.manufacturer_data();
    for (const auto& data : manufacturerData) {
        auto bytes = data.second;
        if (bytes.size() >= 2) {
            if (bytes[0] == 0xEC && bytes[1] == 0x40) {
                if (bytes.size() > 2) {
                    std::string username(bytes.begin() + 2, bytes.end());
                    device.echoUsername = username;
                    device.echoFingerprint = "mfg";
                    device.osType = "unknown";
                    std::cout << "[Discovery] Echo device by manufacturer data: " << username << std::endl;
                    return true;
                }
            }
        }
    }
    
    try {
        auto services = mutable_peripheral.services();
        for (auto& service : services) {
            std::string serviceUuid = service.uuid();
            std::transform(serviceUuid.begin(), serviceUuid.end(), serviceUuid.begin(), ::tolower);
            
            if (serviceUuid.find("f47b5e2d-4a9e-4c5a-9b3f-8e1d2c3a4b5c") != std::string::npos ||
                serviceUuid.find("f47b5e2d") != std::string::npos) {
                
                device.echoUsername = "Echo-" + device.address.substr(device.address.length() - 8);
                device.echoFingerprint = "gatt";
                device.osType = "windows";
                std::cout << "[Discovery] Echo device by service UUID: " << device.echoUsername << std::endl;
                return true;
            }
        }
    } catch (const std::exception& e) {
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
                peripheral.connect();
                
                if (peripheral.is_connected()) {
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
    std::lock_guard<std::mutex> lock(devicesMutex_);
    auto* peripheral = findConnectedPeripheral(address);
    if (!peripheral || !peripheral->is_connected()) {
        std::cerr << "[SEND FAILED] Device " << address << " not connected" << std::endl;
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

void BluetoothManager::setMeshNetwork(std::shared_ptr<MeshNetwork> meshNetwork) {
    meshNetwork_ = meshNetwork;
}

std::shared_ptr<MeshNetwork> BluetoothManager::getMeshNetwork() const {
    return meshNetwork_;
}

bool BluetoothManager::connectToDeviceByAddress(const std::string& address) {
    if (!adapter_) {
        std::cerr << "[Connect] No Bluetooth adapter available" << std::endl;
        return false;
    }
    
    std::cout << "\n[GATT-Only Mode] Attempting to connect to: " << address << std::endl;
    std::cout << "[Connect] This will connect directly without prior discovery..." << std::endl;
    
    try {
        // Check if already connected
        {
            std::lock_guard<std::mutex> lock(devicesMutex_);
            for (auto& p : connectedPeripherals_) {
                if (p.address() == address) {
                    std::cout << "[Connect] Already connected to this device" << std::endl;
                    return true;
                }
            }
        }
        
        // Stop scanning if active to avoid conflicts
        bool wasScanning = isScanning_;
        if (wasScanning) {
            adapter_->scan_stop();
        }
        
        // Start a fresh scan to find the device
        std::cout << "[Connect] Scanning for device..." << std::endl;
        adapter_->scan_for(5000); // 5 second scan
        
        auto peripherals = adapter_->scan_get_results();
        std::cout << "[Connect] Found " << peripherals.size() << " devices in range" << std::endl;
        
        SimpleBLE::Peripheral* targetPeripheral = nullptr;
        for (auto& peripheral : peripherals) {
            std::cout << "[Connect] Checking: " << peripheral.address() << std::endl;
            if (peripheral.address() == address) {
                targetPeripheral = &peripheral;
                break;
            }
        }
        
        if (!targetPeripheral) {
            std::cerr << "[Connect] Device not found at address: " << address << std::endl;
            std::cerr << "[Connect] Make sure the device is advertising and in range" << std::endl;
            if (wasScanning) {
                adapter_->scan_start();
            }
            return false;
        }
        
        std::cout << "[Connect] Found device! Connecting..." << std::endl;
        std::cout << "[Connect] Device name: " << targetPeripheral->identifier() << std::endl;
        
        targetPeripheral->connect();
        
        if (!targetPeripheral->is_connected()) {
            std::cerr << "[Connect] Failed to establish connection" << std::endl;
            if (wasScanning) {
                adapter_->scan_start();
            }
            return false;
        }
        
        std::cout << "[Connect] Connection established! Waiting for GATT services..." << std::endl;
        
        // Wait for service discovery
        bool servicesReady = false;
        for (int retry = 0; retry < 15; retry++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            try {
                auto services = targetPeripheral->services();
                std::cout << "[Connect] Service discovery attempt " << (retry + 1) 
                         << ": found " << services.size() << " services" << std::endl;
                
                // Look for our Echo service
                bool hasEchoService = false;
                for (auto& service : services) {
                    auto svcUuid = service.uuid();
                    std::cout << "[Connect]   - Service UUID: " << svcUuid << std::endl;
                    if (svcUuid.find(BITCHAT_SERVICE_UUID) != std::string::npos ||
                        svcUuid.find("f47b5e2d") != std::string::npos) {
                        hasEchoService = true;
                        std::cout << "[Connect]   ✓ Found Echo GATT service!" << std::endl;
                        
                        auto chars = service.characteristics();
                        for (auto& ch : chars) {
                            std::cout << "[Connect]     - Characteristic: " << ch.uuid() << std::endl;
                        }
                    }
                }
                
                if (services.size() > 0) {
                    servicesReady = true;
                    if (!hasEchoService) {
                        std::cout << "[Connect] WARNING: Device doesn't have Echo GATT service" << std::endl;
                        std::cout << "[Connect] This might not be an Echo device" << std::endl;
                    }
                    break;
                }
            } catch (const std::exception& e) {
                if (retry == 14) {
                    std::cerr << "[Connect] Service discovery failed: " << e.what() << std::endl;
                    targetPeripheral->disconnect();
                    if (wasScanning) {
                        adapter_->scan_start();
                    }
                    return false;
                }
            }
        }
        
        if (!servicesReady) {
            std::cerr << "[Connect] Service discovery timed out" << std::endl;
            targetPeripheral->disconnect();
            if (wasScanning) {
                adapter_->scan_start();
            }
            return false;
        }
        
        // Add to connected list
        {
            std::lock_guard<std::mutex> lock(devicesMutex_);
            connectedPeripherals_.push_back(*targetPeripheral);
        }
        
        // Setup notifications
        setupCharacteristicNotifications(*targetPeripheral);
        
        std::cout << "\n[Connect] ✓ Successfully connected to: " << address << std::endl;
        std::cout << "[Connect] You can now send messages to this device!" << std::endl;
        
        if (deviceConnectedCallback_) {
            deviceConnectedCallback_(address);
        }
        
        // Resume scanning if it was active
        if (wasScanning) {
            adapter_->scan_start();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[Connect] Connection failed: " << e.what() << std::endl;
        return false;
    }
}

void BluetoothManager::setupCharacteristicNotifications(SimpleBLE::Peripheral& peripheral) {
    try {
        auto services = peripheral.services();
        
        for (auto& service : services) {
            auto svcUuid = service.uuid();
            
            // Look for Echo service
            if (svcUuid.find(BITCHAT_SERVICE_UUID) != std::string::npos ||
                svcUuid.find("f47b5e2d") != std::string::npos) {
                
                auto characteristics = service.characteristics();
                
                for (auto& characteristic : characteristics) {
                    auto charUuid = characteristic.uuid();
                    
                    // Subscribe to RX characteristic (device sends to us)
                    if (charUuid.find(BITCHAT_RX_CHAR_UUID) != std::string::npos ||
                        charUuid.find("6d4a9b2e") != std::string::npos) {
                        
                        std::cout << "[BT] Subscribing to RX characteristic..." << std::endl;
                        
                        peripheral.notify(service.uuid(), characteristic.uuid(),
                            [this, addr = peripheral.address()](SimpleBLE::ByteArray data) {
                                std::vector<uint8_t> vecData(data.begin(), data.end());
                                std::cout << "[BT] Received " << vecData.size() 
                                         << " bytes from " << addr << std::endl;
                                
                                if (dataReceivedCallback_) {
                                    dataReceivedCallback_(addr, vecData);
                                }
                                
                                // Process through mesh network if available
                                if (meshNetwork_ && !vecData.empty()) {
                                    try {
                                        auto msg = Message::deserialize(vecData);
                                        meshNetwork_->processIncomingMessage(msg, addr);
                                    } catch (const std::exception& e) {
                                        std::cerr << "[BT] Failed to process message: " 
                                                 << e.what() << std::endl;
                                    }
                                }
                            });
                        
                        std::cout << "[BT] ✓ Subscribed to RX notifications" << std::endl;
                    }
                    
                    // Subscribe to MESH characteristic
                    if (charUuid.find(BITCHAT_MESH_CHAR_UUID) != std::string::npos ||
                        charUuid.find("9a3b5c7d") != std::string::npos) {
                        
                        std::cout << "[BT] Subscribing to MESH characteristic..." << std::endl;
                        
                        peripheral.notify(service.uuid(), characteristic.uuid(),
                            [this, addr = peripheral.address()](SimpleBLE::ByteArray data) {
                                std::vector<uint8_t> vecData(data.begin(), data.end());
                                std::cout << "[MESH] Received " << vecData.size() 
                                         << " bytes from " << addr << std::endl;
                                
                                if (meshNetwork_ && !vecData.empty()) {
                                    try {
                                        auto msg = Message::deserialize(vecData);
                                        meshNetwork_->processIncomingMessage(msg, addr);
                                    } catch (const std::exception& e) {
                                        std::cerr << "[MESH] Failed to process message: " 
                                                 << e.what() << std::endl;
                                    }
                                }
                            });
                        
                        std::cout << "[BT] ✓ Subscribed to MESH notifications" << std::endl;
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[BT] Failed to setup characteristic notifications: " 
                 << e.what() << std::endl;
    }
}

}