#ifdef _WIN32

#include "WindowsAdvertiser.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Storage::Streams;

namespace echo {

class WindowsAdvertiser::Impl {
public:
    Impl() : publisher_(nullptr) {
        init_apartment();
    }
    
    ~Impl() {
        stopAdvertising();
    }
    
    bool startAdvertising(const std::string& username, const std::string& fingerprint) {
        try {
            publisher_ = BluetoothLEAdvertisementPublisher();
            
            auto advertisement = publisher_.Advertisement();
            advertisement.LocalName(winrt::to_hstring("Echo-" + username + "[windows]"));
            
            BluetoothLEManufacturerData manufacturerData;
            manufacturerData.CompanyId(0xFFFF);
            
            std::vector<uint8_t> data;
            data.push_back(0x01);
            
            std::string peerIdShort = fingerprint.substr(0, 16);
            data.insert(data.end(), peerIdShort.begin(), peerIdShort.end());
            
            DataWriter writer;
            writer.WriteBytes(data);
            manufacturerData.Data(writer.DetachBuffer());
            
            advertisement.ManufacturerData().Append(manufacturerData);
            
            GUID serviceGuid;
            UuidFromStringA((RPC_CSTR)ECHO_SERVICE_UUID, &serviceGuid);
            
            advertisement.ServiceUuids().Append(serviceGuid);
            
            publisher_.StatusChanged([this](BluetoothLEAdvertisementPublisher const& sender, 
                                           BluetoothLEAdvertisementPublisherStatusChangedEventArgs const& args) {
                onStatusChanged(sender, args);
            });
            
            publisher_.Start();
            
            std::cout << "[Windows Advertiser] Started advertising as: Echo-" << username << "[windows]" << std::endl;
            std::cout << "[Windows Advertiser] Service UUID: " << ECHO_SERVICE_UUID << std::endl;
            std::cout << "[Windows Advertiser] Peer ID: " << peerIdShort << std::endl;
            
            return true;
            
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