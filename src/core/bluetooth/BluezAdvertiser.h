#pragma once

#ifdef __linux__

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace echo {

// Linux-specific BLE advertising using BlueZ D-Bus
class BluezAdvertiser {
public:
    BluezAdvertiser();
    ~BluezAdvertiser();
    
    // Start advertising with Echo identity
    bool startAdvertising(const std::string& username, const std::string& fingerprint);
    
    // Stop advertising
    void stopAdvertising();
    
    // Check if currently advertising
    bool isAdvertising() const;
    
    // Set advertising interval (milliseconds)
    void setAdvertisingInterval(uint16_t minInterval, uint16_t maxInterval);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
    
    bool advertising_;
    
    // BitChat/Echo service UUID
    static constexpr const char* ECHO_SERVICE_UUID = "0000180F-0000-1000-8000-00805F9B34FB";
};

} // namespace echo

#endif // __linux__