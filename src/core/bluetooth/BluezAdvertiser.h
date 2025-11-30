#pragma once

#ifdef __linux__

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace echo {

class BluezAdvertiser {
public:
    BluezAdvertiser();
    ~BluezAdvertiser();

    bool startAdvertising(const std::string& username, const std::string& fingerprint);

    void stopAdvertising();

    bool isAdvertising() const;

    void setAdvertisingInterval(uint16_t minInterval, uint16_t maxInterval);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
    
    bool advertising_;

    static constexpr const char* ECHO_SERVICE_UUID = "0000180F-0000-1000-8000-00805F9B34FB";
};

} // namespace echo

#endif // __linux__