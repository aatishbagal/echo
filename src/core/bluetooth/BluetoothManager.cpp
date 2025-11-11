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
                        try {
                            prepareMessagingForPeripheral(connectedPeripherals_.back());
                        } catch (const std::exception& e) {
                            std::cerr << "[GATT PREP FAILED] " << e.what() << std::endl;
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
    
    auto services = mutable_peripheral.services();
    for (auto& service : services) {
        std::string serviceUuid = service.uuid();
        std::transform(serviceUuid.begin(), serviceUuid.end(), serviceUuid.begin(), ::toupper);
        
        if (serviceUuid.find("F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C") != std::string::npos ||
            serviceUuid.find("F47B5E2D") != std::string::npos) {
            
            auto rawServiceData = static_cast<std::vector<uint8_t>>(service.data());
            if (!rawServiceData.empty()) {
                uint8_t header = rawServiceData[0];
                uint8_t version = header >> 4;
                uint8_t flags = header & 0x0F;

                if (version == 1) {
                    std::string decodedUsername;
                    if (rawServiceData.size() > 1) {
                        decodedUsername.assign(rawServiceData.begin() + 1, rawServiceData.end());
                    }

                    if (!decodedUsername.empty()) {
                        device.echoUsername = decodedUsername;
                        device.osType = (flags & 0x1) ? "windows" : "linux";
                        device.echoFingerprint = "mesh";
                        std::cout << "[Parser] Found Echo device with service data: " << decodedUsername << std::endl;
                        return true;
                    }
                }
            }
            
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
                device.echoFingerprint = "gatt";
                std::cout << "[Parser] Found Echo device by name: " << device.echoUsername << std::endl;
                return true;
            }
            
            std::cout << "[Parser] Found Echo service UUID but no name/data" << std::endl;
            std::cout << "[Parser] This appears to be a Windows 11 GATT-advertised device" << std::endl;
            
            device.echoUsername = "Win11-" + device.address.substr(0, 8);
            device.echoFingerprint = "gatt-win11";
            device.osType = "windows11";
            
            std::cout << "[Parser] Assigned temporary username: " << device.echoUsername << std::endl;
            return true;
        }
    }
    
    std::string name = mutable_peripheral.identifier();
    if (name.find("Echo-") == 0) {
        device.echoUsername = name.substr(5);
        device.echoFingerprint = "detected";
        device.osType = "unknown";
        std::cout << "[Parser] Found Echo device by name only: " << device.echoUsername << std::endl;
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
    try {
        prepareMessagingForPeripheral(peripheral);
    } catch (const std::exception& e) {
        std::cerr << "[GATT INIT FAILED] " << e.what() << std::endl;
    }
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

void BluetoothManager::prepareMessagingForPeripheral(SimpleBLE::Peripheral& peripheral) {
    auto services = peripheral.services();
    for (auto& service : services) {
        if (service.uuid() == BITCHAT_SERVICE_UUID) {
            auto characteristics = service.characteristics();
            for (auto& characteristic : characteristics) {
                if (characteristic.uuid() == BITCHAT_RX_CHAR_UUID || characteristic.uuid() == BITCHAT_MESH_CHAR_UUID) {
                    if (characteristic.can_notify()) {
                        peripheral.notify(service.uuid(), characteristic.uuid(), [this, addr = peripheral.address()](SimpleBLE::ByteArray payload) {
                            std::vector<uint8_t> data(payload.begin(), payload.end());
                            if (dataReceivedCallback_) {
                                dataReceivedCallback_(addr, data);
                            }
                        });
                    }
                }
            }
        }
    }
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
        if (success) {
            startLinuxInbox();
        }
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

#ifdef __linux__
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#endif

void BluetoothManager::startLinuxInbox() {
#ifdef __linux__
    if (inboxRunning_) return;
    inboxRunning_ = true;
    inboxThread_ = std::thread([this]() {
        int s = socket(AF_UNIX, SOCK_STREAM, 0);
        if (s < 0) { inboxRunning_ = false; return; }
        struct sockaddr_un addr; memset(&addr, 0, sizeof(addr)); addr.sun_family = AF_UNIX; std::string path = "/tmp/echo_gatt.sock"; strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path)-1);
        int rc = -1;
        for (int i=0;i<20; i++) {
            rc = connect(s,(struct sockaddr*)&addr,sizeof(addr));
            if (rc == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        if (rc < 0) { close(s); inboxRunning_ = false; return; }
        inboxSocket_ = s;
        std::vector<uint8_t> buf(512);
        while (inboxRunning_) {
            ssize_t n = recv(s, buf.data(), buf.size(), 0);
            if (n <= 0) break;
            std::vector<uint8_t> data(buf.begin(), buf.begin()+n);
            if (dataReceivedCallback_) {
                dataReceivedCallback_("local", data);
            }
        }
        close(s);
        inboxSocket_ = -1; inboxRunning_ = false;
    });
#endif
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

#ifdef __linux__
    if (inboxRunning_) {
        inboxRunning_ = false;
        if (inboxSocket_ >= 0) { close(inboxSocket_); inboxSocket_ = -1; }
        if (inboxThread_.joinable()) inboxThread_.join();
    }
#endif
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
                    if (characteristic.uuid() == BITCHAT_RX_CHAR_UUID && characteristic.can_write_request()) {
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