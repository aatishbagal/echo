#pragma once

#include <simpleble/SimpleBLE.h>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>

namespace echo {

struct DiscoveredDevice {
    std::string address;
    std::string name;
    int16_t rssi;
    bool isConnectable;
    bool isEchoDevice;  // NEW: Is this an Echo/BitChat device?
    std::string echoUsername;  // NEW: Echo username if applicable
    std::string echoFingerprint;  // NEW: Device fingerprint
    std::chrono::steady_clock::time_point lastSeen;
};

class BluetoothManager {
public:
    BluetoothManager();
    ~BluetoothManager();
    
    // Basic Bluetooth operations
    bool isBluetoothAvailable() const;
    bool startScanning();
    void stopScanning();
    bool isScanning() const;
    
    // Device management
    std::vector<DiscoveredDevice> getDiscoveredDevices() const;
    std::vector<DiscoveredDevice> getEchoDevices() const;  // NEW: Get only Echo devices
    bool connectToDevice(const std::string& address);
    void disconnectFromDevice(const std::string& address);
    
    // BitChat specific operations
    bool startBitChatAdvertising();
    void stopBitChatAdvertising();
    
    // Callbacks for device events
    using DeviceDiscoveredCallback = std::function<void(const DiscoveredDevice&)>;
    using DeviceConnectedCallback = std::function<void(const std::string& address)>;
    using DeviceDisconnectedCallback = std::function<void(const std::string& address)>;
    using DataReceivedCallback = std::function<void(const std::string& address, const std::vector<uint8_t>& data)>;
    
    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback callback);
    void setDeviceConnectedCallback(DeviceConnectedCallback callback);
    void setDeviceDisconnectedCallback(DeviceDisconnectedCallback callback);
    void setDataReceivedCallback(DataReceivedCallback callback);
    
    // Data transmission
    bool sendData(const std::string& address, const std::vector<uint8_t>& data);
    
private:
    // SimpleBLE adapter
    std::shared_ptr<SimpleBLE::Adapter> adapter_;
    
    // Connected peripherals
    std::vector<SimpleBLE::Peripheral> connectedPeripherals_;
    
    // Discovered devices cache
    mutable std::mutex devicesMutex_;
    std::vector<DiscoveredDevice> discoveredDevices_;
    
    // Callbacks
    DeviceDiscoveredCallback deviceDiscoveredCallback_;
    DeviceConnectedCallback deviceConnectedCallback_;
    DeviceDisconnectedCallback deviceDisconnectedCallback_;
    DataReceivedCallback dataReceivedCallback_;
    
    // State
    std::atomic<bool> isScanning_;
    std::atomic<bool> isAdvertising_;
    
    // BitChat specific UUIDs (from actual BitChat implementation)
    static constexpr const char* BITCHAT_SERVICE_UUID = "F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C";
    static constexpr const char* BITCHAT_TX_CHAR_UUID = "8E9B7A4C-2D5F-4B6A-9C3E-1F8D7B2A5C4E";
    static constexpr const char* BITCHAT_RX_CHAR_UUID = "6D4A9B2E-5C7F-4A8D-9B3C-2E1F8D7A4B5C";
    static constexpr const char* BITCHAT_MESH_CHAR_UUID = "9A3B5C7D-4E6F-4B8A-9D2C-3F1E8D7B4A5C";
    
    // Private methods
    void initializeAdapter();
    void onPeripheralFound(SimpleBLE::Peripheral peripheral);
    void onPeripheralConnected(SimpleBLE::Peripheral peripheral);
    void onPeripheralDisconnected(SimpleBLE::Peripheral peripheral);
    bool isBitChatDevice(const SimpleBLE::Peripheral& peripheral) const;
    bool parseEchoDevice(const SimpleBLE::Peripheral& peripheral, DiscoveredDevice& device);  // NEW
    SimpleBLE::Peripheral* findConnectedPeripheral(const std::string& address);
};

} // namespace echo