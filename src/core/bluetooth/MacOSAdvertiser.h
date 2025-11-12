#pragma once

#ifdef __APPLE__

#include <string>
#include <memory>
#include <atomic>

namespace echo {

class MacOSAdvertiser {
public:
    MacOSAdvertiser();
    ~MacOSAdvertiser();
    
    bool startAdvertising(const std::string& username, const std::string& fingerprint);
    void stopAdvertising();
    bool isAdvertising() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
    std::atomic<bool> advertising_;
    
    static constexpr const char* ECHO_SERVICE_UUID = "F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C";
};

} // namespace echo

#endif // __APPLE__
