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
    
    // BitChat specific UUIDs and characteristics
    static constexpr const char* BITCHAT_SERVICE_UUID = "0000180F-0000-1000-8000-00805F9B34FB";
    static constexpr const char* BITCHAT_MESSAGE_CHAR_UUID = "00002A19-0000-1000-8000-00805F9B34FB";
    
    // Private methods
    void initializeAdapter();
    void onPeripheralFound(SimpleBLE::Peripheral peripheral);
    void onPeripheralConnected(SimpleBLE::Peripheral peripheral);
    void onPeripheralDisconnected(SimpleBLE::Peripheral peripheral);
    bool isBitChatDevice(const SimpleBLE::Peripheral& peripheral) const;
    SimpleBLE::Peripheral* findConnectedPeripheral(const std::string& address);
};

} // namespace echo