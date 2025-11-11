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
        std::cout << "\n[Windows Advertiser] Starting GATT service with characteristics..." << std::endl;
        
        if (tryGattServerWithCharacteristics(username, fingerprint)) {
            return true;
        }
        
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "WINDOWS 11 BLE ADVERTISING BLOCKED" << std::endl;
        std::cerr << "========================================" << std::endl;
        std::cerr << "Quick Fix - Try ONE of these:" << std::endl;
        std::cerr << "\n1. RUN AS ADMINISTRATOR (Easiest)" << std::endl;
        std::cerr << "   - Close this app" << std::endl;
        std::cerr << "   - Right-click echo.exe" << std::endl;
        std::cerr << "   - Select 'Run as administrator'" << std::endl;
        std::cerr << "\n2. ENABLE DEVELOPER MODE" << std::endl;
        std::cerr << "   - Open Settings" << std::endl;
        std::cerr << "   - Go to: Privacy & Security > For developers" << std::endl;
        std::cerr << "   - Turn ON 'Developer Mode'" << std::endl;
        std::cerr << "   - Restart this app" << std::endl;
        std::cerr << "========================================\n" << std::endl;
        
        return false;
    }
    
    bool tryGattServerWithCharacteristics(const std::string& username, const std::string& fingerprint) {
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
            auto service = gattServiceProvider_.Service();
            
            std::cout << "[Windows GATT] Creating TX characteristic (for sending)..." << std::endl;
            
            winrt::guid txCharGuid(0x8E9B7A4C, 0x2D5F, 0x4B6A,
                { 0x9C, 0x3E, 0x1F, 0x8D, 0x7B, 0x2A, 0x5C, 0x4E });
            
            GattLocalCharacteristicParameters txParams;
            txParams.CharacteristicProperties(
                GattCharacteristicProperties::Read | 
                GattCharacteristicProperties::Notify
            );
            txParams.ReadProtectionLevel(GattProtectionLevel::Plain);
            
            auto txResult = service.CreateCharacteristicAsync(txCharGuid, txParams).get();
            
            if (txResult.Error() != BluetoothError::Success) {
                std::cerr << "[Windows GATT] Failed to create TX characteristic" << std::endl;
                return false;
            }
            
            txCharacteristic_ = txResult.Characteristic();
            std::cout << "[Windows GATT] TX characteristic created successfully" << std::endl;
            
            std::cout << "[Windows GATT] Creating RX characteristic (for receiving)..." << std::endl;
            
            winrt::guid rxCharGuid(0x6D4A9B2E, 0x5C7F, 0x4A8D,
                { 0x9B, 0x3C, 0x2E, 0x1F, 0x8D, 0x7A, 0x4B, 0x5C });
            
            GattLocalCharacteristicParameters rxParams;
            rxParams.CharacteristicProperties(
                GattCharacteristicProperties::Write | 
                GattCharacteristicProperties::WriteWithoutResponse
            );
            rxParams.WriteProtectionLevel(GattProtectionLevel::Plain);
            
            auto rxResult = service.CreateCharacteristicAsync(rxCharGuid, rxParams).get();
            
            if (rxResult.Error() != BluetoothError::Success) {
                std::cerr << "[Windows GATT] Failed to create RX characteristic" << std::endl;
                return false;
            }
            
            rxCharacteristic_ = rxResult.Characteristic();
            
            rxCharacteristic_.WriteRequested([this](
                GattLocalCharacteristic const& characteristic,
                GattWriteRequestedEventArgs const& args) {
                    onWriteRequested(characteristic, args);
                });
            
            std::cout << "[Windows GATT] RX characteristic created successfully" << std::endl;
            
            std::cout << "[Windows GATT] Starting advertising..." << std::endl;
            gattServiceProvider_.StartAdvertising();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            std::cout << "[Windows GATT] GATT service started successfully!" << std::endl;
            std::cout << "[Windows GATT] Service UUID: " << ECHO_SERVICE_UUID << std::endl;
            std::cout << "[Windows GATT] TX Characteristic: 8E9B7A4C-2D5F-4B6A-9C3E-1F8D7B2A5C4E" << std::endl;
            std::cout << "[Windows GATT] RX Characteristic: 6D4A9B2E-5C7F-4A8D-9B3C-2E1F8D7A4B5C" << std::endl;
            std::cout << "[Windows GATT] Device is now discoverable and can send/receive messages" << std::endl;
            
            return true;
            
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows GATT] Exception (HRESULT: 0x" 
                     << std::hex << e.code() << std::dec << "): " 
                     << winrt::to_string(e.message()) << std::endl;
            
            if (e.code() == 0x80070057) {
                std::cerr << "[Windows GATT] Windows 11 packaging restriction detected" << std::endl;
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
    
    void onWriteRequested(GattLocalCharacteristic const& characteristic,
                         GattWriteRequestedEventArgs const& args) {
        try {
            auto deferral = args.GetDeferral();
            auto request = args.GetRequestAsync().get();
            
            if (request) {
                auto buffer = request.Value();
                std::vector<uint8_t> data(buffer.Length());
                
                DataReader reader = DataReader::FromBuffer(buffer);
                reader.ReadBytes(data);
                
                std::cout << "[Windows GATT] Received " << data.size() << " bytes via RX characteristic" << std::endl;
                
                if (messageReceivedCallback_) {
                    messageReceivedCallback_(data);
                }
                
                request.Respond();
            }
            
            deferral.Complete();
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows GATT] Error in write handler: " << winrt::to_string(e.message()) << std::endl;
        }
    }
    
    bool sendMessageViaCharacteristic(const std::vector<uint8_t>& data) {
        if (!txCharacteristic_) {
            std::cerr << "[Windows GATT] TX characteristic not available" << std::endl;
            return false;
        }
        
        try {
            DataWriter writer;
            writer.WriteBytes(winrt::array_view<const uint8_t>(data.data(), data.data() + data.size()));
            auto buffer = writer.DetachBuffer();
            
            txCharacteristic_.NotifyValueAsync(buffer).get();
            std::cout << "[Windows GATT] Sent " << data.size() << " bytes via TX characteristic" << std::endl;
            return true;
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows GATT] Failed to send: " << winrt::to_string(e.message()) << std::endl;
            return false;
        }
    }
    
    void setMessageReceivedCallback(MessageReceivedCallback callback) {
        messageReceivedCallback_ = std::move(callback);
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
        
        txCharacteristic_ = nullptr;
        rxCharacteristic_ = nullptr;
    }
    
    bool isAdvertising() const {
        return gattServiceProvider_ != nullptr || publisher_ != nullptr;
    }
    
private:
    BluetoothLEAdvertisementPublisher publisher_;
    GattServiceProvider gattServiceProvider_;
    GattLocalCharacteristic txCharacteristic_;
    GattLocalCharacteristic rxCharacteristic_;
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