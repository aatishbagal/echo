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

#ifdef _WIN32
#include "WindowsAdvertiser.h"
#endif

#ifdef __linux__
#include "BluezAdvertiser.h"
#endif

#ifdef __APPLE__
#include "MacOSAdvertiser.h"
#endif

namespace echo {

struct DiscoveredDevice {
    std::string address;
    std::string name;
    int16_t rssi;
    bool isConnectable;
    bool isEchoDevice; 
    std::string echoUsername;  
    std::string echoFingerprint;
    std::string osType;  
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
    
    bool startEchoAdvertising(const std::string& username, const std::string& fingerprint);
    void stopEchoAdvertising();
    bool isAdvertising() const;
    
    bool broadcastMessage(const std::vector<uint8_t>& data);
    
    using DeviceDiscoveredCallback = std::function<void(const DiscoveredDevice&)>;
    using DeviceConnectedCallback = std::function<void(const std::string& address)>;
    using DeviceDisconnectedCallback = std::function<void(const std::string& address)>;
    using DataReceivedCallback = std::function<void(const std::string& address, const std::vector<uint8_t>& data)>;
    using MessageBroadcastCallback = std::function<void(const std::vector<uint8_t>& data)>;
    
    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback callback);
    void setDeviceConnectedCallback(DeviceConnectedCallback callback);
    void setDeviceDisconnectedCallback(DeviceDisconnectedCallback callback);
    void setDataReceivedCallback(DataReceivedCallback callback);
    void setMessageBroadcastCallback(MessageBroadcastCallback callback);
    
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
    MessageBroadcastCallback messageBroadcastCallback_;
    
    std::atomic<bool> isScanning_;
    std::atomic<bool> isAdvertising_;
    
#ifdef _WIN32
    std::unique_ptr<WindowsAdvertiser> windowsAdvertiser_;
#endif
#ifdef __linux__
    std::unique_ptr<BluezAdvertiser> bluezAdvertiser_;
#endif
#ifdef __APPLE__
    std::unique_ptr<MacOSAdvertiser> macosAdvertiser_;
#endif
    
    static constexpr const char* BITCHAT_SERVICE_UUID = "F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C";
    static constexpr const char* BITCHAT_TX_CHAR_UUID = "8E9B7A4C-2D5F-4B6A-9C3E-1F8D7B2A5C4E";
    static constexpr const char* BITCHAT_RX_CHAR_UUID = "6D4A9B2E-5C7F-4A8D-9B3C-2E1F8D7A4B5C";
    static constexpr const char* BITCHAT_MESH_CHAR_UUID = "9A3B5C7D-4E6F-4B8A-9D2C-3F1E8D7B4A5C";
    void startLinuxInbox();
    int inboxSocket_ = -1;
    std::thread inboxThread_;
    std::atomic<bool> inboxRunning_{false};
    
    void initializeAdapter();
    void onPeripheralFound(SimpleBLE::Peripheral peripheral);
    void onPeripheralConnected(SimpleBLE::Peripheral peripheral);
    void onPeripheralDisconnected(SimpleBLE::Peripheral peripheral);
    bool isBitChatDevice(const SimpleBLE::Peripheral& peripheral) const;
    bool parseEchoDevice(const SimpleBLE::Peripheral& peripheral, DiscoveredDevice& device);
    SimpleBLE::Peripheral* findConnectedPeripheral(const std::string& address);
    void prepareMessagingForPeripheral(SimpleBLE::Peripheral& peripheral);
};

} // namespace echo