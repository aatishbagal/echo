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
        std::cout << "\n========================================" << std::endl;
        std::cout << "Starting Windows BLE Advertising" << std::endl;
        std::cout << "========================================" << std::endl;
        
        bool gattSuccess = tryGattServerApproach(username, fingerprint);
        
        if (gattSuccess) {
            std::cout << "\nGATT server active" << std::endl;
            std::cout << "Ready for connections and messaging" << std::endl;
            std::cout << "========================================\n" << std::endl;
            return true;
        }
        
        std::cout << "\nGATT failed, trying Publisher..." << std::endl;
        bool publisherSuccess = tryAdvertiserApproach(username, fingerprint);
        
        if (publisherSuccess) {
            std::cout << "\nPublisher active (discoverable)" << std::endl;
            std::cout << "GATT server failed (limited messaging)" << std::endl;
            std::cout << "========================================\n" << std::endl;
            return true;
        }
        
        std::cerr << "\nBoth GATT and Publisher failed" << std::endl;
        std::cerr << "Try running as Administrator" << std::endl;
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
            payload.push_back(0xEC);
            payload.push_back(0x40);
            
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
            
            advertisement.Flags(BluetoothLEAdvertisementFlags::GeneralDiscoverableMode |
                               BluetoothLEAdvertisementFlags::ClassicNotSupported);
            
            publisher_.StatusChanged([this](BluetoothLEAdvertisementPublisher const& sender,
                                            BluetoothLEAdvertisementPublisherStatusChangedEventArgs const& args) {
                onStatusChanged(sender, args);
            });
            
            std::cout << "[Windows Advertiser] Starting publisher..." << std::endl;
            publisher_.Start();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            auto status = publisher_.Status();
            std::cout << "[Windows Advertiser] Publisher status: " << (int)status << std::endl;
            
            if (status == BluetoothLEAdvertisementPublisherStatus::Started) {
                std::cout << "[Windows Advertiser] SUCCESS: Advertisement active!" << std::endl;
                std::cout << "[Windows Advertiser] Broadcasting as: " << localName << std::endl;
                std::cout << "[Windows Advertiser] Service UUID: " << ECHO_SERVICE_UUID << std::endl;
                std::cout << "[Windows Advertiser] Username in manufacturer data: " << truncatedUsername << std::endl;
                std::cout << "[Windows Advertiser] Other Windows devices should now see you" << std::endl;
                return true;
            } else if (status == BluetoothLEAdvertisementPublisherStatus::Waiting) {
                std::cout << "[Windows Advertiser] Status: Waiting (checking again in 2 seconds)..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                status = publisher_.Status();
                if (status == BluetoothLEAdvertisementPublisherStatus::Started) {
                    std::cout << "[Windows Advertiser] SUCCESS: Advertisement active after wait!" << std::endl;
                    return true;
                }
                std::cerr << "[Windows Advertiser] FAILED: Still waiting after 2 seconds" << std::endl;
            } else if (status == BluetoothLEAdvertisementPublisherStatus::Aborted) {
                std::cerr << "[Windows Advertiser] FAILED: Advertisement aborted" << std::endl;
                std::cerr << "[Windows Advertiser] This usually means:" << std::endl;
                std::cerr << "[Windows Advertiser]   1. Bluetooth adapter doesn't support peripheral mode" << std::endl;
                std::cerr << "[Windows Advertiser]   2. Need to run as Administrator" << std::endl;
                std::cerr << "[Windows Advertiser]   3. Bluetooth radio is off or busy" << std::endl;
            } else {
                std::cerr << "[Windows Advertiser] FAILED: Unknown status " << (int)status << std::endl;
            }
            
            publisher_.Stop();
            publisher_ = nullptr;
            return false;
            
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows Advertiser] EXCEPTION (HRESULT: 0x" 
                     << std::hex << e.code() << std::dec << "): " 
                     << winrt::to_string(e.message()) << std::endl;
            
            if (e.code() == 0x8007000E) {
                std::cerr << "[Windows Advertiser] ERROR: Out of memory / Resources unavailable" << std::endl;
            } else if (e.code() == 0x80070490) {
                std::cerr << "[Windows Advertiser] ERROR: Element not found (driver issue?)" << std::endl;
            } else if (e.code() == 0x80004005) {
                std::cerr << "[Windows Advertiser] ERROR: Unspecified error (permissions?)" << std::endl;
            }
            
            if (publisher_) {
                try { publisher_.Stop(); } catch(...) {}
                publisher_ = nullptr;
            }
            return false;
        } catch (...) {
            std::cerr << "[Windows Advertiser] EXCEPTION: Unknown error" << std::endl;
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
            auto service = gattServiceProvider_.Service();
            
            std::cout << "[Windows GATT] Creating characteristics..." << std::endl;
            
            winrt::guid txCharGuid(0x8E9B7A4C, 0x2D5F, 0x4B6A,
                { 0x9C, 0x3E, 0x1F, 0x8D, 0x7B, 0x2A, 0x5C, 0x4E });
            
            GattLocalCharacteristicParameters txParams;
            txParams.CharacteristicProperties(
                GattCharacteristicProperties::Write |
                GattCharacteristicProperties::WriteWithoutResponse
            );
            txParams.WriteProtectionLevel(GattProtectionLevel::Plain);
            
            auto txResult = service.CreateCharacteristicAsync(txCharGuid, txParams).get();
            if (txResult.Error() != BluetoothError::Success) {
                std::cerr << "[Windows GATT] Failed to create TX characteristic" << std::endl;
            } else {
                auto txChar = txResult.Characteristic();
                txChar.WriteRequested([this](GattLocalCharacteristic const& characteristic,
                                            GattWriteRequestedEventArgs const& args) {
                    onCharacteristicWriteRequested(characteristic, args);
                });
                std::cout << "[Windows GATT] TX characteristic created (write)" << std::endl;
            }
            
            winrt::guid rxCharGuid(0x6D4A9B2E, 0x5C7F, 0x4A8D,
                { 0x9B, 0x3C, 0x2E, 0x1F, 0x8D, 0x7A, 0x4B, 0x5C });
            
            GattLocalCharacteristicParameters rxParams;
            rxParams.CharacteristicProperties(
                GattCharacteristicProperties::Notify |
                GattCharacteristicProperties::Indicate
            );
            rxParams.ReadProtectionLevel(GattProtectionLevel::Plain);
            
            auto rxResult = service.CreateCharacteristicAsync(rxCharGuid, rxParams).get();
            if (rxResult.Error() != BluetoothError::Success) {
                std::cerr << "[Windows GATT] Failed to create RX characteristic" << std::endl;
            } else {
                rxCharacteristic_ = rxResult.Characteristic();
                rxCharacteristic_.SubscribedClientsChanged([this](GattLocalCharacteristic const& characteristic,
                                                                  IInspectable const&) {
                    auto count = characteristic.SubscribedClients().Size();
                    std::cout << "[Windows GATT] RX subscribers: " << count << std::endl;
                });
                std::cout << "[Windows GATT] RX characteristic created (notify)" << std::endl;
            }
            
            winrt::guid meshCharGuid(0x9A3B5C7D, 0x4E6F, 0x4B8A,
                { 0x9D, 0x2C, 0x3F, 0x1E, 0x8D, 0x7B, 0x4A, 0x5C });
            
            GattLocalCharacteristicParameters meshParams;
            meshParams.CharacteristicProperties(
                GattCharacteristicProperties::Write |
                GattCharacteristicProperties::Notify
            );
            meshParams.WriteProtectionLevel(GattProtectionLevel::Plain);
            meshParams.ReadProtectionLevel(GattProtectionLevel::Plain);
            
            auto meshResult = service.CreateCharacteristicAsync(meshCharGuid, meshParams).get();
            if (meshResult.Error() != BluetoothError::Success) {
                std::cerr << "[Windows GATT] Failed to create MESH characteristic" << std::endl;
            } else {
                meshCharacteristic_ = meshResult.Characteristic();
                meshCharacteristic_.WriteRequested([this](GattLocalCharacteristic const& characteristic,
                                                         GattWriteRequestedEventArgs const& args) {
                    onCharacteristicWriteRequested(characteristic, args);
                });
                std::cout << "[Windows GATT] MESH characteristic created (write+notify)" << std::endl;
            }
            
            std::cout << "[Windows GATT] Starting advertising with characteristics..." << std::endl;
            gattServiceProvider_.StartAdvertising();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            std::cout << "[Windows GATT] GATT service started with full characteristics!" << std::endl;
            std::cout << "[Windows GATT] Service UUID: " << ECHO_SERVICE_UUID << std::endl;
            std::cout << "[Windows GATT] TX UUID: 8E9B7A4C-2D5F-4B6A-9C3E-1F8D7B2A5C4E" << std::endl;
            std::cout << "[Windows GATT] RX UUID: 6D4A9B2E-5C7F-4A8D-9B3C-2E1F8D7A4B5C" << std::endl;
            std::cout << "[Windows GATT] MESH UUID: 9A3B5C7D-4E6F-4B8A-9D2C-3F1E8D7B4A5C" << std::endl;
            std::cout << "[Windows GATT] Device is ready for messaging!" << std::endl;
            
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
            rxCharacteristic_ = nullptr;
            meshCharacteristic_ = nullptr;
            return false;
        } catch (...) {
            std::cerr << "[Windows GATT] Unknown exception" << std::endl;
            if (gattServiceProvider_) {
                try { gattServiceProvider_.StopAdvertising(); } catch(...) {}
                gattServiceProvider_ = nullptr;
            }
            rxCharacteristic_ = nullptr;
            meshCharacteristic_ = nullptr;
            return false;
        }
    }
    
    void setMessageReceivedCallback(MessageReceivedCallback callback) {
        messageReceivedCallback_ = std::move(callback);
    }
    
    void onCharacteristicWriteRequested(GattLocalCharacteristic const& characteristic,
                                       GattWriteRequestedEventArgs const& args) {
        try {
            auto deferral = args.GetDeferral();
            auto request = args.GetRequestAsync().get();
            
            if (request) {
                auto buffer = request.Value();
                auto dataReader = DataReader::FromBuffer(buffer);
                
                std::vector<uint8_t> data(buffer.Length());
                if (buffer.Length() > 0) {
                    dataReader.ReadBytes(data);
                    
                    std::cout << "[Windows GATT] Received " << data.size() << " bytes on characteristic" << std::endl;
                    
                    if (messageReceivedCallback_) {
                        messageReceivedCallback_(data);
                    }
                }
                
                request.Respond();
            }
            
            deferral.Complete();
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows GATT] Write request error: " 
                     << winrt::to_string(e.message()) << std::endl;
        } catch (...) {
            std::cerr << "[Windows GATT] Unknown write request error" << std::endl;
        }
    }
    
    bool sendMessageViaCharacteristic(const std::vector<uint8_t>& data) {
        if (!rxCharacteristic_) {
            std::cout << "[Windows GATT] No RX characteristic available for sending" << std::endl;
            return false;
        }
        
        try {
            auto subscribers = rxCharacteristic_.SubscribedClients();
            if (subscribers.Size() == 0) {
                std::cout << "[Windows GATT] No subscribers to send to" << std::endl;
                return false;
            }
            
            auto dataWriter = DataWriter();
            dataWriter.WriteBytes(data);
            auto buffer = dataWriter.DetachBuffer();
            
            rxCharacteristic_.NotifyValueAsync(buffer).get();
            
            std::cout << "[Windows GATT] Sent " << data.size() << " bytes to " 
                     << subscribers.Size() << " subscriber(s)" << std::endl;
            return true;
            
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[Windows GATT] Failed to send notification: " 
                     << winrt::to_string(e.message()) << std::endl;
            return false;
        } catch (...) {
            std::cerr << "[Windows GATT] Unknown error sending notification" << std::endl;
            return false;
        }
    }
    
    void stopAdvertising() {
        if (gattServiceProvider_) {
            try {
                gattServiceProvider_.StopAdvertising();
                std::cout << "[Windows GATT] Stopped advertising" << std::endl;
            } catch (...) {}
            rxCharacteristic_ = nullptr;
            meshCharacteristic_ = nullptr;
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
    GattLocalCharacteristic rxCharacteristic_{nullptr};
    GattLocalCharacteristic meshCharacteristic_{nullptr};
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