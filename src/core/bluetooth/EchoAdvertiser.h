#pragma once

#include "../crypto/UserIdentity.h"
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <memory>

namespace echo {

class EchoAdvertiser {
public:
    EchoAdvertiser(const UserIdentity& identity);
    ~EchoAdvertiser();

    bool startAdvertising();
    void stopAdvertising();
    bool isAdvertising() const { return isAdvertising_; }

    void updateIdentity(const UserIdentity& identity);

    bool broadcastMessage(const std::vector<uint8_t>& messageData);
    
private:
    const UserIdentity& identity_;
    std::atomic<bool> isAdvertising_;

    std::vector<uint8_t> buildAdvertisingData() const;

    static constexpr const char* ECHO_SERVICE_UUID = "0000180F-0000-1000-8000-00805F9B34FB";
};

struct EchoDevice {
    std::string username;
    std::string fingerprint;
    std::string bluetoothAddress;
    int16_t rssi;
    bool isEchoDevice;
    
    EchoDevice() : rssi(0), isEchoDevice(false) {}
};

EchoDevice parseEchoAdvertising(const std::string& deviceName, 
                                const std::string& address,
                                int16_t rssi);

} // namespace echo