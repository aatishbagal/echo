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
        std::cout << "\n[Windows Advertiser] Starting BLE advertising..." << std::endl;
        
        if (tryAdvertiserApproach(username, fingerprint)) {
            return true;
        }
        
        std::cout << "\n[Windows Advertiser] Falling back to GATT service..." << std::endl;
        if (tryGattServerApproach(username, fingerprint)) {
            return true;
        }
        
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "WINDOWS 11 BLE LIMITATION" << std::endl;
        std::cerr << "========================================" << std::endl;
        std::cerr << "Quick fixes:" << std::endl;
        std::cerr << "1. Run as Administrator" << std::endl;
        std::cerr << "2. Enable Developer Mode in Settings" << std::endl;
        std::cerr << "========================================\n" << std::endl;
        
        return false;
    }
    
    bool tryAdvertiserApproach(const std::string& username, const std::string& fingerprint) {
        try {
            std::cout << "[Windows Advertiser] Trying BluetoothLEAdvertisementPublisher..." << std::endl;
            
            publisher_ = BluetoothLEAdvertisementPublisher();
            
            auto advertisement = publisher_.Advertisement();
            
            winrt::guid serviceGuid(0xF47B5E2D, 0x4A9E, 0x4C5A,
                { 0x9B, 0x3F, 0x8E, 0x1D, 0x2C, 0x3A, 0x4B, 0x5C });
            
            advertisement.ServiceUuids().Append(serviceGuid);
            
            std::string localName = "Echo-" + username + "[win]";
            if (localName.length() > 20) {
                localName = "Echo-" + username.substr(0, 11) + "[win]";
            }
            advertisement.LocalName(winrt::to_hstring(localName));
            
            auto manufacturerData = BluetoothLEManufacturerData();
            manufacturerData.CompanyId(0xFFFF);
            
            std::vector<uint8_t> payload;
            payload.push_back(0x11);
            
            std::string truncatedUsername = username;
            if (truncatedUsername.length() > 20) {
                truncatedUsername = truncatedUsername.substr(0, 20);
            }
            
            for (char c : truncatedUsername) {
                payload.push_back(static_cast<uint8_t>(c));
            }
            
            auto dataWriter = DataWriter();
            dataWriter.WriteBytes(payload);
            manufacturerData.Data(dataWriter.DetachBuffer());
            
            advertisement.ManufacturerData().Append(manufacturerData);
            
            publisher_.StatusChanged([this](BluetoothLEAdvertisementPublisher const& sender,
                                            BluetoothLEAdvertisementPublisherStatusChangedEventArgs const& args) {
                onStatusChanged(sender, args);
            });
            
            publisher_.Start();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            auto status = publisher_.Status();
            if (status == BluetoothLEAdvertisementPublisherStatus::Started) {
                std::cout << "[Windows Advertiser] Advertisement started successfully!" << std::endl;
                std::cout << "[Windows Advertiser] Broadcasting as: " << localName << std::endl;
                std::cout << "[Windows Advertiser] Service UUID: " << ECHO_SERVICE_UUID << std::endl;
                std::cout << "[Windows Advertiser] Username in manufacturer data: " << truncatedUsername << std::endl;
                return true;
            } else {
                std::cerr << "[Windows Advertiser] Advertisement failed to start (Status: " << (int)status << ")" << std::endl;
                publisher_ = nullptr;
                return false;
            }
            
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows Advertiser] Exception (HRESULT: 0x" 
                     << std::hex << e.code() << std::dec << "): " 
                     << winrt::to_string(e.message()) << std::endl;
            
            if (publisher_) {
                try { publisher_.Stop(); } catch(...) {}
                publisher_ = nullptr;
            }
            return false;
        } catch (...) {
            std::cerr << "[Windows Advertiser] Unknown exception" << std::endl;
            if (publisher_) {
                try { publisher_.Stop(); } catch(...) {}
                publisher_ = nullptr;
            }
            return false;
        }
    }
    
    bool tryGattServerApproach(const std::string& username, const std::string& fingerprint) {
        try {
            std::cout << "[Windows GATT] Creating GATT Service Provider..." << std::endl;
            
            winrt::guid serviceGuid(0xF47B5E2D, 0x4A9E, 0x4C5A,
                { 0x9B, 0x3F, 0x8E, 0x1D, 0x2C, 0x3A, 0x4B, 0x5C });
            
            auto createResult = GattServiceProvider::CreateAsync(serviceGuid).get();
            
            if (createResult.Error() != BluetoothError::Success) {
                std::cerr << "[Windows GATT] Failed to create service provider (Error: " 
                         << (int)createResult.Error() << ")" << std::endl;
                return false;
            }
            
            gattServiceProvider_ = createResult.ServiceProvider();
            
            std::cout << "[Windows GATT] Service provider created successfully" << std::endl;
            std::cout << "[Windows GATT] Starting advertising..." << std::endl;
            
            gattServiceProvider_.StartAdvertising();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            std::cout << "[Windows GATT] GATT service started!" << std::endl;
            std::cout << "[Windows GATT] Service UUID: " << ECHO_SERVICE_UUID << std::endl;
            std::cout << "[Windows GATT] Device is now discoverable" << std::endl;
            std::cout << "[Windows GATT] WARNING: Username not in advertisement (Windows limitation)" << std::endl;
            std::cout << "[Windows GATT] Other devices will see generic identifier" << std::endl;
            
            return true;
            
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows GATT] Exception (HRESULT: 0x" 
                     << std::hex << e.code() << std::dec << "): " 
                     << winrt::to_string(e.message()) << std::endl;
            
            if (e.code() == 0x80070057) {
                std::cerr << "[Windows GATT] Windows 11 packaging restriction" << std::endl;
            }
            
            if (gattServiceProvider_) {
                try { gattServiceProvider_.StopAdvertising(); } catch(...) {}
                gattServiceProvider_ = nullptr;
            }
            return false;
        } catch (...) {
            std::cerr << "[Windows GATT] Unknown exception" << std::endl;
            if (gattServiceProvider_) {
                try { gattServiceProvider_.StopAdvertising(); } catch(...) {}
                gattServiceProvider_ = nullptr;
            }
            return false;
        }
    }
    
    void setMessageReceivedCallback(MessageReceivedCallback callback) {
        messageReceivedCallback_ = std::move(callback);
    }
    
    bool sendMessageViaCharacteristic(const std::vector<uint8_t>& data) {
        std::cout << "[Windows GATT] Message sending not yet implemented (need characteristics)" << std::endl;
        return false;
    }
    
    void stopAdvertising() {
        if (gattServiceProvider_) {
            try {
                gattServiceProvider_.StopAdvertising();
                std::cout << "[Windows GATT] Stopped advertising" << std::endl;
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
        bool gattActive = gattServiceProvider_ != nullptr;
        bool publisherActive = publisher_ != nullptr && 
                              publisher_.Status() == BluetoothLEAdvertisementPublisherStatus::Started;
        return gattActive || publisherActive;
    }
    
private:
    BluetoothLEAdvertisementPublisher publisher_;
    GattServiceProvider gattServiceProvider_;
    MessageReceivedCallback messageReceivedCallback_;
    
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

void WindowsAdvertiser::setMessageReceivedCallback(MessageReceivedCallback callback) {
    pImpl_->setMessageReceivedCallback(std::move(callback));
}

bool WindowsAdvertiser::sendMessageViaCharacteristic(const std::vector<uint8_t>& data) {
    return pImpl_->sendMessageViaCharacteristic(data);
}

}

#endif