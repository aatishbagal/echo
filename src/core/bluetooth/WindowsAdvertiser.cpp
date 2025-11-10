#ifdef _WIN32

#include "WindowsAdvertiser.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Storage.Streams.h>
#include <Rpc.h>

#pragma comment(lib, "Rpcrt4.lib")

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Storage::Streams;

namespace echo {

class WindowsAdvertiser::Impl {
public:
    Impl() : publisher_(nullptr) {
        try {
            winrt::init_apartment();
            
            // Check if Bluetooth adapter supports peripheral mode
            try {
                auto adapter = Windows::Devices::Bluetooth::BluetoothAdapter::GetDefaultAsync().get();
                if (adapter) {
                    std::cout << "[Windows Advertiser] Bluetooth adapter found" << std::endl;
                    std::cout << "[Windows Advertiser] Adapter: " << winrt::to_string(adapter.DeviceId()) << std::endl;
                    
                    // Check if LE advertising is supported
                    auto radio = adapter.GetRadioAsync().get();
                    if (radio) {
                        auto state = radio.State();
                        std::cout << "[Windows Advertiser] Radio state: " << (int)state << std::endl;
                        if (state != Windows::Devices::Radios::RadioState::On) {
                            std::cerr << "[Windows Advertiser] WARNING: Bluetooth radio is not ON" << std::endl;
                            std::cerr << "[Windows Advertiser] Please enable Bluetooth in Windows Settings" << std::endl;
                        }
                    }
                } else {
                    std::cerr << "[Windows Advertiser] WARNING: No Bluetooth adapter found" << std::endl;
                }
            } catch (const winrt::hresult_error& e) {
                std::cerr << "[Windows Advertiser] Warning: Could not check adapter (HRESULT: 0x" 
                          << std::hex << e.code() << std::dec << ")" << std::endl;
                // Don't fail - we'll try advertising anyway
            }
            
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows Advertiser] Failed to initialize WinRT: " 
                      << winrt::to_string(e.message()) << std::endl;
            throw;
        } catch (...) {
            std::cerr << "[Windows Advertiser] Failed to initialize WinRT" << std::endl;
            throw;
        }
    }
    
    ~Impl() {
        stopAdvertising();
    }
    
    bool startAdvertising(const std::string& username, const std::string& fingerprint) {
        try {
            publisher_ = BluetoothLEAdvertisementPublisher();
            
            auto advertisement = publisher_.Advertisement();
            
            // Set local name
            try {
                std::string advertisingName = "Echo-" + username + "[windows]";
                advertisement.LocalName(winrt::to_hstring(advertisingName));
                std::cout << "[Windows Advertiser] Set local name: " << advertisingName << std::endl;
            } catch (const winrt::hresult_error& e) {
                std::cerr << "[Windows Advertiser] Warning: Could not set local name (HRESULT: 0x" 
                          << std::hex << e.code() << std::dec << ")" << std::endl;
            }
            
            // Add service UUID using CBUUID string format
            try {
                // Try using the string-based UUID approach instead of GUID
                auto serviceUuids = advertisement.ServiceUuids();
                
                // Windows expects GUIDs in standard format
                winrt::guid serviceGuid(0xF47B5E2D, 0x4A9E, 0x4C5A, 
                    { 0x9B, 0x3F, 0x8E, 0x1D, 0x2C, 0x3A, 0x4B, 0x5C });
                
                serviceUuids.Append(serviceGuid);
                std::cout << "[Windows Advertiser] Added service UUID" << std::endl;
            } catch (const winrt::hresult_error& e) {
                std::cerr << "[Windows Advertiser] Error adding service UUID (HRESULT: 0x" 
                          << std::hex << e.code() << std::dec << "): " 
                          << winrt::to_string(e.message()) << std::endl;
                // Service UUID is critical - re-throw
                throw;
            }
            
            // IMPORTANT: Don't set status callback before Start() - it can cause E_INVALIDARG
            // Set publisher status callback AFTER starting
            auto statusToken = publisher_.StatusChanged([this](BluetoothLEAdvertisementPublisher const& sender, 
                                           BluetoothLEAdvertisementPublisherStatusChangedEventArgs const& args) {
                onStatusChanged(sender, args);
            });
            
            // Start advertising
            std::cout << "[Windows Advertiser] Starting publisher..." << std::endl;
            
            try {
                publisher_.Start();
            } catch (const winrt::hresult_error& e) {
                std::cerr << "[Windows Advertiser] Start() failed with HRESULT: 0x" 
                          << std::hex << e.code() << std::dec << std::endl;
                std::cerr << "[Windows Advertiser] Error: " << winrt::to_string(e.message()) << std::endl;
                
                // Try to get more detailed error
                if (e.code() == 0x80070057) {  // E_INVALIDARG
                    std::cerr << "[Windows Advertiser] E_INVALIDARG - Possible causes:" << std::endl;
                    std::cerr << "  - Bluetooth is disabled or unavailable" << std::endl;
                    std::cerr << "  - Another app is already advertising" << std::endl;
                    std::cerr << "  - Windows Bluetooth LE Peripheral mode not supported" << std::endl;
                    std::cerr << "  - Check: Settings > Bluetooth & devices > Bluetooth is ON" << std::endl;
                }
                throw;
            }
            
            // Give it a moment to start
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            std::cout << "[Windows Advertiser] Successfully started advertising" << std::endl;
            std::cout << "[Windows Advertiser] Broadcasting as: Echo-" << username << "[windows]" << std::endl;
            std::cout << "[Windows Advertiser] Service UUID: " << ECHO_SERVICE_UUID << std::endl;
            
            return true;
            
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows Advertiser] WinRT Error: " << winrt::to_string(e.message()) << std::endl;
            std::cerr << "[Windows Advertiser] HRESULT: 0x" << std::hex << e.code() << std::dec << std::endl;
            return false;
        } catch (const std::exception& e) {
            std::cerr << "[Windows Advertiser] Failed to start: " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "[Windows Advertiser] Failed to start: Unknown error" << std::endl;
            return false;
        }
    }
    
    void stopAdvertising() {
        if (publisher_) {
            try {
                publisher_.Stop();
                std::cout << "[Windows Advertiser] Stopped advertising" << std::endl;
            } catch (const winrt::hresult_error& e) {
                std::cerr << "[Windows Advertiser] Error stopping: " << winrt::to_string(e.message()) << std::endl;
            } catch (...) {
                std::cerr << "[Windows Advertiser] Error stopping advertising" << std::endl;
            }
            publisher_ = nullptr;
        }
    }
    
    bool isAdvertising() const {
        if (!publisher_) return false;
        
        try {
            auto status = publisher_.Status();
            return status == BluetoothLEAdvertisementPublisherStatus::Started;
        } catch (...) {
            return false;
        }
    }
    
private:
    BluetoothLEAdvertisementPublisher publisher_;
    
    void onStatusChanged(BluetoothLEAdvertisementPublisher const& sender,
                        BluetoothLEAdvertisementPublisherStatusChangedEventArgs const& args) {
        auto status = args.Status();
        
        switch (status) {
            case BluetoothLEAdvertisementPublisherStatus::Started:
                std::cout << "[Windows Advertiser] Status: Started" << std::endl;
                break;
            case BluetoothLEAdvertisementPublisherStatus::Stopped:
                std::cout << "[Windows Advertiser] Status: Stopped" << std::endl;
                break;
            case BluetoothLEAdvertisementPublisherStatus::Aborted:
                std::cerr << "[Windows Advertiser] Status: Aborted (Error: " 
                         << (int)args.Error() << ")" << std::endl;
                break;
            default:
                std::cout << "[Windows Advertiser] Status: " << (int)status << std::endl;
                break;
        }
    }
};

WindowsAdvertiser::WindowsAdvertiser() 
    : pImpl_(std::make_unique<Impl>()), advertising_(false) {
}

WindowsAdvertiser::~WindowsAdvertiser() {
    stopAdvertising();
}

bool WindowsAdvertiser::startAdvertising(const std::string& username, const std::string& fingerprint) {
    if (advertising_) {
        return true;
    }
    
    advertising_ = pImpl_->startAdvertising(username, fingerprint);
    return advertising_;
}

void WindowsAdvertiser::stopAdvertising() {
    if (advertising_) {
        pImpl_->stopAdvertising();
        advertising_ = false;
    }
}

bool WindowsAdvertiser::isAdvertising() const {
    return advertising_ && pImpl_->isAdvertising();
}

void WindowsAdvertiser::setAdvertisingInterval(uint16_t minInterval, uint16_t maxInterval) {
    (void)minInterval;
    (void)maxInterval;
}

} // namespace echo

#endif // _WIN32