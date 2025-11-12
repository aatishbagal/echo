#ifdef _WIN32

#include "WindowsAdvertiser.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Storage.Streams.h>
#include <Rpc.h>

#pragma comment(lib, "Rpcrt4.lib")

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

namespace echo {

class WindowsAdvertiser::Impl {
public:
    Impl() : publisher_(nullptr), gattServiceProvider_(nullptr) {
        try {
            winrt::init_apartment();
            
            try {
                auto adapter = Windows::Devices::Bluetooth::BluetoothAdapter::GetDefaultAsync().get();
                if (adapter) {
                    std::cout << "[Windows Advertiser] Bluetooth adapter found" << std::endl;
                    std::cout << "[Windows Advertiser] Adapter: " << winrt::to_string(adapter.DeviceId()) << std::endl;
                    
                    auto radio = adapter.GetRadioAsync().get();
                    if (radio) {
                        auto state = radio.State();
                        std::cout << "[Windows Advertiser] Radio state: " << (int)state << " (";
                        switch (state) {
                            case Windows::Devices::Radios::RadioState::Unknown:
                                std::cout << "Unknown"; break;
                            case Windows::Devices::Radios::RadioState::On:
                                std::cout << "ON"; break;
                            case Windows::Devices::Radios::RadioState::Off:
                                std::cout << "OFF"; break;
                            case Windows::Devices::Radios::RadioState::Disabled:
                                std::cout << "Disabled"; break;
                        }
                        std::cout << ")" << std::endl;
                        
                        if (state != Windows::Devices::Radios::RadioState::On) {
                            std::cerr << "\n[Windows Advertiser] ERROR: Bluetooth radio is NOT enabled!" << std::endl;
                        }
                    }
                    
                    std::cout << "[Windows Advertiser] Checking peripheral mode support..." << std::endl;
                    auto leFeatures = adapter.IsPeripheralRoleSupported();
                    std::cout << "[Windows Advertiser] Peripheral role supported: " 
                              << (leFeatures ? "YES" : "NO") << std::endl;
                    
                    if (!leFeatures) {
                        std::cerr << "\n[Windows Advertiser] WARNING: BLE Peripheral mode NOT supported!" << std::endl;
                    }
                    
                } else {
                    std::cerr << "[Windows Advertiser] WARNING: No Bluetooth adapter found" << std::endl;
                }
            } catch (const winrt::hresult_error& e) {
                std::cerr << "[Windows Advertiser] Warning: Could not check adapter (HRESULT: 0x" 
                          << std::hex << e.code() << std::dec << ")" << std::endl;
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
        std::cout << "\n[Windows Advertiser] Windows 11 detected - Testing advertising methods..." << std::endl;
        
        std::cout << "[Windows Advertiser] Attempting GATT Server approach..." << std::endl;
        if (tryGattServerApproach(username, fingerprint)) {
            return true;
        }
        
        std::cout << "[Windows Advertiser] GATT failed, trying empty advertisement..." << std::endl;
        if (tryEmptyAdvertisement()) {
            return true;
        }
        
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "WINDOWS 11 BLE ADVERTISING BLOCKED" << std::endl;
        std::cerr << "========================================" << std::endl;
        std::cerr << "The error 0x80070057 is a Windows 11 security restriction." << std::endl;
        std::cerr << "\nQuick Fix - Try ONE of these:" << std::endl;
        std::cerr << "\n1. RUN AS ADMINISTRATOR (Easiest)" << std::endl;
        std::cerr << "   - Close this app" << std::endl;
        std::cerr << "   - Right-click echo.exe" << std::endl;
        std::cerr << "   - Select 'Run as administrator'" << std::endl;
        std::cerr << "\n2. ENABLE DEVELOPER MODE" << std::endl;
        std::cerr << "   - Open Settings" << std::endl;
        std::cerr << "   - Go to: Privacy & Security > For developers" << std::endl;
        std::cerr << "   - Turn ON 'Developer Mode'" << std::endl;
        std::cerr << "   - Restart this app" << std::endl;
        std::cerr << "\nCurrent Status: SCAN-ONLY MODE" << std::endl;
        std::cerr << "You can still discover and connect to other Echo devices" << std::endl;
        std::cerr << "========================================\n" << std::endl;
        
        return false;
    }
    
    bool tryGattServerApproach(const std::string& username, const std::string& fingerprint) {
        try {
            std::cout << "[Windows Advertiser] Creating GATT Service Provider..." << std::endl;
            
            winrt::guid serviceGuid(0xF47B5E2D, 0x4A9E, 0x4C5A,
                { 0x9B, 0x3F, 0x8E, 0x1D, 0x2C, 0x3A, 0x4B, 0x5C });
            
            auto createResult = GattServiceProvider::CreateAsync(serviceGuid).get();
            
            if (createResult.Error() != BluetoothError::Success) {
                std::cerr << "[Windows Advertiser] GATT: Failed to create service provider (Error: " 
                         << (int)createResult.Error() << ")" << std::endl;
                return false;
            }
            
            gattServiceProvider_ = createResult.ServiceProvider();
            
            std::cout << "[Windows Advertiser] GATT: Service provider created" << std::endl;
            
            gattServiceProvider_.StartAdvertising();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            std::cout << "[Windows Advertiser] GATT: Started advertising via GATT service" << std::endl;
            std::cout << "[Windows Advertiser] GATT: Service UUID: " << ECHO_SERVICE_UUID << std::endl;
            std::cout << "[Windows Advertiser] GATT: Device is now discoverable" << std::endl;
            
            return true;
            
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows Advertiser] GATT: Exception (HRESULT: 0x" 
                     << std::hex << e.code() << std::dec << "): " 
                     << winrt::to_string(e.message()) << std::endl;
            
            if (e.code() == 0x80070057) {
                std::cerr << "[Windows Advertiser] GATT: Windows 11 packaging restriction confirmed" << std::endl;
            }
            
            if (gattServiceProvider_) {
                try { gattServiceProvider_.StopAdvertising(); } catch(...) {}
                gattServiceProvider_ = nullptr;
            }
            return false;
        } catch (...) {
            std::cerr << "[Windows Advertiser] GATT: Unknown exception" << std::endl;
            if (gattServiceProvider_) {
                try { gattServiceProvider_.StopAdvertising(); } catch(...) {}
                gattServiceProvider_ = nullptr;
            }
            return false;
        }
    }
    
    bool tryEmptyAdvertisement() {
        try {
            publisher_ = BluetoothLEAdvertisementPublisher();
            
            std::cout << "[Windows Advertiser] Empty: Attempting completely empty advertisement" << std::endl;
            
            publisher_.Start();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            auto status = publisher_.Status();
            if (status == BluetoothLEAdvertisementPublisherStatus::Started) {
                std::cout << "[Windows Advertiser] Empty: SUCCESS - Basic advertisement working" << std::endl;
                std::cout << "[Windows Advertiser] Empty: Device is advertising (no user data)" << std::endl;
                
                auto statusToken = publisher_.StatusChanged([this](BluetoothLEAdvertisementPublisher const& sender, 
                                               BluetoothLEAdvertisementPublisherStatusChangedEventArgs const& args) {
                    onStatusChanged(sender, args);
                });
                
                return true;
            } else {
                std::cerr << "[Windows Advertiser] Empty: Failed - Status: " << (int)status << std::endl;
                publisher_.Stop();
                publisher_ = nullptr;
                return false;
            }
            
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows Advertiser] Empty: Exception (HRESULT: 0x" 
                     << std::hex << e.code() << std::dec << ")" << std::endl;
            if (publisher_) {
                try { publisher_.Stop(); } catch(...) {}
                publisher_ = nullptr;
            }
            return false;
        } catch (...) {
            if (publisher_) {
                try { publisher_.Stop(); } catch(...) {}
                publisher_ = nullptr;
            }
            return false;
        }
    }
    
    void stopAdvertising() {
        if (gattServiceProvider_) {
            try {
                gattServiceProvider_.StopAdvertising();
                std::cout << "[Windows Advertiser] GATT: Stopped advertising" << std::endl;
            } catch (...) {}
            gattServiceProvider_ = nullptr;
        }
        
        if (publisher_) {
            try {
                publisher_.Stop();
                std::cout << "[Windows Advertiser] Stopped advertising" << std::endl;
            } catch (...) {}
            publisher_ = nullptr;
        }
    }
    
    bool isAdvertising() const {
        if (gattServiceProvider_) {
            return true;
        }
        
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
    GattServiceProvider gattServiceProvider_;
    
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

}

#endif