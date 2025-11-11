#pragma once

#ifdef _WIN32

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <atomic>
#include <functional>

namespace echo {

class WindowsAdvertiser {
public:
    WindowsAdvertiser();
    ~WindowsAdvertiser();
    
    bool startAdvertising(const std::string& username, const std::string& fingerprint);
    void stopAdvertising();
    bool isAdvertising() const;
    
    void setAdvertisingInterval(uint16_t minInterval, uint16_t maxInterval);
    
    using MessageReceivedCallback = std::function<void(const std::vector<uint8_t>&)>;
    void setMessageReceivedCallback(MessageReceivedCallback callback);
    
    bool sendMessageViaCharacteristic(const std::vector<uint8_t>& data);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
    std::atomic<bool> advertising_;
    
    static constexpr const char* ECHO_SERVICE_UUID = "F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C";
};

} // namespace echo

#endif // _WIN32