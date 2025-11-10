#ifdef __APPLE__

#include "MacOSAdvertiser.h"
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#include <iostream>
#include <vector>

@interface PeripheralDelegate : NSObject <CBPeripheralManagerDelegate>
@property (nonatomic) bool isReady;
@property (nonatomic) bool isAdvertising;
@end

@implementation PeripheralDelegate

- (instancetype)init {
    self = [super init];
    if (self) {
        _isReady = false;
        _isAdvertising = false;
    }
    return self;
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
    switch (peripheral.state) {
        case CBManagerStatePoweredOn:
            std::cout << "[macOS Advertiser] Bluetooth is powered on and ready" << std::endl;
            self.isReady = true;
            break;
        case CBManagerStatePoweredOff:
            std::cout << "[macOS Advertiser] Bluetooth is powered off" << std::endl;
            self.isReady = false;
            break;
        case CBManagerStateResetting:
            std::cout << "[macOS Advertiser] Bluetooth is resetting" << std::endl;
            self.isReady = false;
            break;
        case CBManagerStateUnauthorized:
            std::cout << "[macOS Advertiser] Bluetooth authorization required" << std::endl;
            self.isReady = false;
            break;
        case CBManagerStateUnsupported:
            std::cout << "[macOS Advertiser] Bluetooth LE not supported on this device" << std::endl;
            self.isReady = false;
            break;
        case CBManagerStateUnknown:
            std::cout << "[macOS Advertiser] Bluetooth state unknown" << std::endl;
            self.isReady = false;
            break;
    }
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral error:(NSError *)error {
    if (error) {
        std::cerr << "[macOS Advertiser] Failed to start advertising: " 
                  << [[error localizedDescription] UTF8String] << std::endl;
        self.isAdvertising = false;
    } else {
        std::cout << "[macOS Advertiser] Successfully started advertising" << std::endl;
        self.isAdvertising = true;
    }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral didAddService:(CBService *)service error:(NSError *)error {
    if (error) {
        std::cerr << "[macOS Advertiser] Failed to add service: " 
                  << [[error localizedDescription] UTF8String] << std::endl;
    } else {
        std::cout << "[macOS Advertiser] Service added successfully" << std::endl;
    }
}

@end

namespace echo {

class MacOSAdvertiser::Impl {
public:
    CBPeripheralManager* peripheralManager;
    PeripheralDelegate* delegate;
    CBMutableService* echoService;
    
    Impl() : peripheralManager(nil), delegate(nil), echoService(nil) {
        delegate = [[PeripheralDelegate alloc] init];
        
        dispatch_queue_t queue = dispatch_queue_create("com.echo.peripheral", DISPATCH_QUEUE_SERIAL);
        peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:delegate 
                                                                     queue:queue
                                                                   options:nil];
        
        int attempts = 0;
        while (!delegate.isReady && attempts < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }
        
        if (!delegate.isReady) {
            std::cerr << "[macOS Advertiser] Bluetooth not ready after timeout" << std::endl;
        }
    }
    
    ~Impl() {
        if (peripheralManager) {
            if (peripheralManager.isAdvertising) {
                [peripheralManager stopAdvertising];
            }
            if (echoService) {
                [peripheralManager removeService:echoService];
            }
            peripheralManager = nil;
        }
        if (delegate) {
            delegate = nil;
        }
    }
};

MacOSAdvertiser::MacOSAdvertiser() 
    : pImpl_(std::make_unique<Impl>()), advertising_(false) {
}

MacOSAdvertiser::~MacOSAdvertiser() {
    stopAdvertising();
}

bool MacOSAdvertiser::startAdvertising(const std::string& username, const std::string& fingerprint) {
    if (!pImpl_ || !pImpl_->delegate.isReady) {
        std::cerr << "[macOS Advertiser] Bluetooth not ready" << std::endl;
        return false;
    }
    
    if (advertising_) {
        std::cout << "[macOS Advertiser] Already advertising" << std::endl;
        return true;
    }
    
    @autoreleasepool {
        NSString* uuidString = @(ECHO_SERVICE_UUID);
        CBUUID* serviceUUID = [CBUUID UUIDWithString:uuidString];
        
        CBMutableCharacteristic* characteristic = [[CBMutableCharacteristic alloc]
            initWithType:[CBUUID UUIDWithString:@"8E9B7A4C-2D5F-4B6A-9C3E-1F8D7B2A5C4E"]
            properties:CBCharacteristicPropertyRead | CBCharacteristicPropertyWrite | CBCharacteristicPropertyNotify
            value:nil
            permissions:CBAttributePermissionsReadable | CBAttributePermissionsWriteable];
        
        pImpl_->echoService = [[CBMutableService alloc] initWithType:serviceUUID primary:YES];
        pImpl_->echoService.characteristics = @[characteristic];
        
        [pImpl_->peripheralManager addService:pImpl_->echoService];
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::string localName = "Echo-" + username + "[macos]";
        NSString* name = [NSString stringWithUTF8String:localName.c_str()];
        
        NSDictionary* advertisingData = @{
            CBAdvertisementDataServiceUUIDsKey: @[serviceUUID],
            CBAdvertisementDataLocalNameKey: name
        };
        
        [pImpl_->peripheralManager startAdvertising:advertisingData];
        
        int attempts = 0;
        while (!pImpl_->delegate.isAdvertising && attempts < 20) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }
        
        if (pImpl_->delegate.isAdvertising) {
            advertising_ = true;
            std::cout << "[macOS Advertiser] Broadcasting as: " << localName << std::endl;
            std::cout << "[macOS Advertiser] Service UUID: " << ECHO_SERVICE_UUID << std::endl;
            return true;
        } else {
            std::cerr << "[macOS Advertiser] Failed to start advertising" << std::endl;
            return false;
        }
    }
}

void MacOSAdvertiser::stopAdvertising() {
    if (!advertising_) {
        return;
    }
    
    @autoreleasepool {
        if (pImpl_ && pImpl_->peripheralManager) {
            if (pImpl_->peripheralManager.isAdvertising) {
                [pImpl_->peripheralManager stopAdvertising];
                std::cout << "[macOS Advertiser] Stopped advertising" << std::endl;
            }
            if (pImpl_->echoService) {
                [pImpl_->peripheralManager removeService:pImpl_->echoService];
                pImpl_->echoService = nil;
            }
        }
        advertising_ = false;
        if (pImpl_->delegate) {
            pImpl_->delegate.isAdvertising = false;
        }
    }
}

bool MacOSAdvertiser::isAdvertising() const {
    return advertising_;
}

} // namespace echo

#endif // __APPLE__
