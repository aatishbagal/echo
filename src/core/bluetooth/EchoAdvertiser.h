#pragma once

#include "../crypto/UserIdentity.h"
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <memory>

namespace echo {

// Handles BLE advertising for Echo presence and messaging
class EchoAdvertiser {
public:
    EchoAdvertiser(const UserIdentity& identity);
    ~EchoAdvertiser();
    
    // Start/stop advertising Echo presence
    bool startAdvertising();
    void stopAdvertising();
    bool isAdvertising() const { return isAdvertising_; }
    
    // Update advertised data
    void updateIdentity(const UserIdentity& identity);
    
    // Broadcast message via advertising (for mesh network)
    bool broadcastMessage(const std::vector<uint8_t>& messageData);
    
private:
    const UserIdentity& identity_;
    std::atomic<bool> isAdvertising_;
    
    // Build advertising packet data
    std::vector<uint8_t> buildAdvertisingData() const;
    
    // BitChat/Echo service UUID
    static constexpr const char* ECHO_SERVICE_UUID = "0000180F-0000-1000-8000-00805F9B34FB";
};

// Parse Echo device from advertising data
struct EchoDevice {
    std::string username;
    std::string fingerprint;
    std::string bluetoothAddress;
    int16_t rssi;
    bool isEchoDevice;
    
    EchoDevice() : rssi(0), isEchoDevice(false) {}
};

// Parse advertising data to detect Echo devices
EchoDevice parseEchoAdvertising(const std::string& deviceName, 
                                const std::string& address,
                                int16_t rssi);

} // namespace echo