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
    bool isEchoDevice;
    std::string echoUsername;
    std::string echoFingerprint;
    std::chrono::steady_clock::time_point lastSeen;
};

class BluetoothManager {
public:
    BluetoothManager();
    ~BluetoothManager();
    
    bool isBluetoothAvailable() const;
    bool startScanning();
    void stopScanning();
    bool isScanning() const;
    
    std::vector<DiscoveredDevice> getDiscoveredDevices() const;
    std::vector<DiscoveredDevice> getEchoDevices() const;
    bool connectToDevice(const std::string& address);
    void disconnectFromDevice(const std::string& address);
    
    bool startBitChatAdvertising();
    void stopBitChatAdvertising();
    
    using DeviceDiscoveredCallback = std::function<void(const DiscoveredDevice&)>;
    using DeviceConnectedCallback = std::function<void(const std::string& address)>;
    using DeviceDisconnectedCallback = std::function<void(const std::string& address)>;
    using DataReceivedCallback = std::function<void(const std::string& address, const std::vector<uint8_t>& data)>;
    
    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback callback);
    void setDeviceConnectedCallback(DeviceConnectedCallback callback);
    void setDeviceDisconnectedCallback(DeviceDisconnectedCallback callback);
    void setDataReceivedCallback(DataReceivedCallback callback);
    
    bool sendData(const std::string& address, const std::vector<uint8_t>& data);
    
private:
    std::shared_ptr<SimpleBLE::Adapter> adapter_;
    std::vector<SimpleBLE::Peripheral> connectedPeripherals_;
    
    mutable std::mutex devicesMutex_;
    std::vector<DiscoveredDevice> discoveredDevices_;
    
    DeviceDiscoveredCallback deviceDiscoveredCallback_;
    DeviceConnectedCallback deviceConnectedCallback_;
    DeviceDisconnectedCallback deviceDisconnectedCallback_;
    DataReceivedCallback dataReceivedCallback_;
    
    std::atomic<bool> isScanning_;
    std::atomic<bool> isAdvertising_;
    
    static constexpr const char* BITCHAT_SERVICE_UUID = "0000180F-0000-1000-8000-00805F9B34FB";
    static constexpr const char* BITCHAT_MESSAGE_CHAR_UUID = "00002A19-0000-1000-8000-00805F9B34FB";
    
    void initializeAdapter();
    void onPeripheralFound(SimpleBLE::Peripheral peripheral);
    void onPeripheralConnected(SimpleBLE::Peripheral peripheral);
    void onPeripheralDisconnected(SimpleBLE::Peripheral peripheral);
    bool isBitChatDevice(const SimpleBLE::Peripheral& peripheral) const;
    bool parseEchoDevice(const SimpleBLE::Peripheral& peripheral, DiscoveredDevice& device);
    SimpleBLE::Peripheral* findConnectedPeripheral(const std::string& address);
};

} // namespace echo