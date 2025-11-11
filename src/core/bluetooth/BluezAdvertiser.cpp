#ifdef __linux__

#include "BluezAdvertiser.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <cstring>

namespace echo {

class BluezAdvertiser::Impl {
public:
    Impl() : advertiserPid_(-1) {}
    
    ~Impl() {
        stopAdvertising();
    }
    
    bool startAdvertising(const std::string& username, const std::string& fingerprint) {
        std::string deviceName = "Echo-" + username + "[linux]";
        std::string peerId = fingerprint.substr(0, 16);
        
        std::string truncatedUsername = username;
        if (truncatedUsername.length() > 20) {
            truncatedUsername = truncatedUsername.substr(0, 20);
        }
        
        std::string scriptContent = generatePythonScript(deviceName, truncatedUsername);
        
        std::string scriptPath = "/tmp/echo_advertise.py";
        std::ofstream scriptFile(scriptPath);
        if (!scriptFile.is_open()) {
            std::cerr << "[Linux Advertiser] Failed to create advertising script" << std::endl;
            return false;
        }
        
        scriptFile << scriptContent;
        scriptFile.close();
        
        system(("chmod +x " + scriptPath).c_str());
        
        advertiserPid_ = fork();
        
        if (advertiserPid_ == 0) {
            execlp("python3", "python3", scriptPath.c_str(), nullptr);
            std::cerr << "[Linux Advertiser] Failed to start Python advertising script" << std::endl;
            exit(1);
        } else if (advertiserPid_ > 0) {
            usleep(500000);
            
            int status;
            pid_t result = waitpid(advertiserPid_, &status, WNOHANG);
            
            if (result == 0) {
                std::cout << "[Linux Advertiser] Started advertising as: " << deviceName << std::endl;
                std::cout << "[Linux Advertiser] Service UUID: F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C" << std::endl;
                std::cout << "[Linux Advertiser] Username in manufacturer data: " << truncatedUsername << std::endl;
                std::cout << "[Linux Advertiser] Process PID: " << advertiserPid_ << std::endl;
                return true;
            } else {
                std::cerr << "[Linux Advertiser] Advertising process failed to start" << std::endl;
                advertiserPid_ = -1;
                return false;
            }
        } else {
            std::cerr << "[Linux Advertiser] Failed to fork advertising process" << std::endl;
            return false;
        }
    }
    
    void stopAdvertising() {
        if (advertiserPid_ > 0) {
            std::cout << "[Linux Advertiser] Stopping advertising (PID: " << advertiserPid_ << ")" << std::endl;
            kill(advertiserPid_, SIGTERM);
            
            int status;
            waitpid(advertiserPid_, &status, 0);
            
            advertiserPid_ = -1;
        }
    }
    
private:
    pid_t advertiserPid_;
    
    std::string generatePythonScript(const std::string& deviceName, const std::string& username) {
        std::ostringstream manufacturerData;
        manufacturerData << "dbus.Array([dbus.Byte(0x11)";
        for (unsigned char c : username) {
            manufacturerData << ", dbus.Byte(" << static_cast<int>(c) << ")";
        }
        manufacturerData << "], signature='y')";
        
        std::string manufacturerDataStr = manufacturerData.str();
        
        std::ostringstream script;
        script << R"(#!/usr/bin/env python3
import dbus
import dbus.exceptions
import dbus.mainloop.glib
import dbus.service
from gi.repository import GLib
import array
import sys

BLUEZ_SERVICE_NAME = 'org.bluez'
LE_ADVERTISING_MANAGER_IFACE = 'org.bluez.LEAdvertisingManager1'
GATT_MANAGER_IFACE = 'org.bluez.GattManager1'
DBUS_OM_IFACE = 'org.freedesktop.DBus.ObjectManager'
DBUS_PROP_IFACE = 'org.freedesktop.DBus.Properties'
LE_ADVERTISEMENT_IFACE = 'org.bluez.LEAdvertisement1'
GATT_SERVICE_IFACE = 'org.bluez.GattService1'
GATT_CHRC_IFACE = 'org.bluez.GattCharacteristic1'

class Advertisement(dbus.service.Object):
    PATH_BASE = '/org/bluez/echo/advertisement'

    def __init__(self, bus, index, advertising_type):
        self.path = self.PATH_BASE + str(index)
        self.bus = bus
        self.ad_type = advertising_type
        self.service_uuids = ['F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C']
        self.manufacturer_data = dbus.Dictionary({
            dbus.UInt16(0xFFFF): )";
        
        script << manufacturerDataStr;
        
        script << R"(
        }, signature='qv')
        self.local_name = ')";
        script << deviceName;
        script << R"('
        self.include_tx_power = False
        dbus.service.Object.__init__(self, bus, self.path)

    def get_properties(self):
        properties = dict()
        properties['Type'] = self.ad_type
        if self.service_uuids is not None:
            properties['ServiceUUIDs'] = dbus.Array(self.service_uuids, signature='s')
        if self.local_name is not None:
            properties['LocalName'] = dbus.String(self.local_name)
        if self.manufacturer_data is not None:
            properties['ManufacturerData'] = self.manufacturer_data
        if self.include_tx_power:
            properties['IncludeTxPower'] = dbus.Boolean(self.include_tx_power)
        return {LE_ADVERTISEMENT_IFACE: properties}

    def get_path(self):
        return dbus.ObjectPath(self.path)

    @dbus.service.method(DBUS_PROP_IFACE, in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != LE_ADVERTISEMENT_IFACE:
            raise dbus.exceptions.DBusException('org.freedesktop.DBus.Error.InvalidArgs', 'Invalid interface')
        return self.get_properties()[LE_ADVERTISEMENT_IFACE]

    @dbus.service.method(LE_ADVERTISEMENT_IFACE, in_signature='', out_signature='')
    def Release(self):
        print('[Advertiser] Advertisement released')

class Characteristic(dbus.service.Object):
    def __init__(self, bus, index, uuid, flags, service):
        self.path = service.path + '/char' + str(index)
        self.bus = bus
        self.uuid = uuid
        self.service = service
        self.flags = flags
        dbus.service.Object.__init__(self, bus, self.path)

    def get_properties(self):
        return {
            GATT_CHRC_IFACE: {
                'Service': self.service.get_path(),
                'UUID': self.uuid,
                'Flags': self.flags,
            }
        }

    def get_path(self):
        return dbus.ObjectPath(self.path)

    @dbus.service.method(DBUS_PROP_IFACE, in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != GATT_CHRC_IFACE:
            raise dbus.exceptions.DBusException('org.freedesktop.DBus.Error.InvalidArgs', 'Invalid interface')
        return self.get_properties()[GATT_CHRC_IFACE]

    @dbus.service.method(GATT_CHRC_IFACE, in_signature='a{sv}', out_signature='ay')
    def ReadValue(self, options):
        print('[GATT] Read on characteristic ' + self.uuid)
        return []

    @dbus.service.method(GATT_CHRC_IFACE, in_signature='aya{sv}')
    def WriteValue(self, value, options):
        print('[GATT] Write on characteristic ' + self.uuid + ': ' + str(len(value)) + ' bytes')

    @dbus.service.method(GATT_CHRC_IFACE)
    def StartNotify(self):
        print('[GATT] Notify started on ' + self.uuid)

    @dbus.service.method(GATT_CHRC_IFACE)
    def StopNotify(self):
        print('[GATT] Notify stopped on ' + self.uuid)

class EchoService(dbus.service.Object):
    PATH_BASE = '/org/bluez/echo/service'

    def __init__(self, bus, index):
        self.path = self.PATH_BASE + str(index)
        self.bus = bus
        self.uuid = 'F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C'
        self.primary = True
        self.characteristics = []
        dbus.service.Object.__init__(self, bus, self.path)

        self.add_characteristic(TxCharacteristic(bus, 0, self))
        self.add_characteristic(RxCharacteristic(bus, 1, self))
        self.add_characteristic(MeshCharacteristic(bus, 2, self))

    def get_properties(self):
        return {
            GATT_SERVICE_IFACE: {
                'UUID': self.uuid,
                'Primary': self.primary,
                'Characteristics': dbus.Array(self.get_characteristic_paths(), signature='o')
            }
        }

    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_characteristic(self, characteristic):
        self.characteristics.append(characteristic)

    def get_characteristic_paths(self):
        result = []
        for chrc in self.characteristics:
            result.append(chrc.get_path())
        return result

    def get_characteristics(self):
        return self.characteristics

    @dbus.service.method(DBUS_PROP_IFACE, in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != GATT_SERVICE_IFACE:
            raise dbus.exceptions.DBusException('org.freedesktop.DBus.Error.InvalidArgs', 'Invalid interface')
        return self.get_properties()[GATT_SERVICE_IFACE]

class TxCharacteristic(Characteristic):
    def __init__(self, bus, index, service):
        Characteristic.__init__(self, bus, index, '8E9B7A4C-2D5F-4B6A-9C3E-1F8D7B2A5C4E',
                              ['write', 'write-without-response'], service)

class RxCharacteristic(Characteristic):
    def __init__(self, bus, index, service):
        Characteristic.__init__(self, bus, index, '6D4A9B2E-5C7F-4A8D-9B3C-2E1F8D7A4B5C',
                              ['notify', 'indicate'], service)

class MeshCharacteristic(Characteristic):
    def __init__(self, bus, index, service):
        Characteristic.__init__(self, bus, index, '9A3B5C7D-4E6F-4B8A-9D2C-3F1E8D7B4A5C',
                              ['write', 'notify'], service)

class Application(dbus.service.Object):
    def __init__(self, bus):
        self.path = '/'
        self.services = []
        dbus.service.Object.__init__(self, bus, self.path)
        self.add_service(EchoService(bus, 0))

    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_service(self, service):
        self.services.append(service)

    @dbus.service.method(DBUS_OM_IFACE, out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        response = {}
        for service in self.services:
            response[service.get_path()] = service.get_properties()
            chrcs = service.get_characteristics()
            for chrc in chrcs:
                response[chrc.get_path()] = chrc.get_properties()
        return response

def register_ad_cb():
    print('[Advertiser] Advertisement registered')

def register_ad_error_cb(error):
    print('[Advertiser] Failed to register advertisement: ' + str(error))
    mainloop.quit()

def register_app_cb():
    print('[GATT] Application registered')

def register_app_error_cb(error):
    print('[GATT] Failed to register application: ' + str(error))
    mainloop.quit()

def find_adapter(bus):
    remote_om = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, '/'), DBUS_OM_IFACE)
    objects = remote_om.GetManagedObjects()
    for o, props in objects.items():
        if LE_ADVERTISING_MANAGER_IFACE in props and GATT_MANAGER_IFACE in props:
            return o
    return None

def main():
    global mainloop
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    
    adapter = find_adapter(bus)
    if not adapter:
        print('[Error] No suitable adapter found')
        return
    
    adapter_props = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, adapter), DBUS_PROP_IFACE)
    adapter_props.Set('org.bluez.Adapter1', 'Powered', dbus.Boolean(1))
    
    service_manager = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, adapter), GATT_MANAGER_IFACE)
    ad_manager = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, adapter), LE_ADVERTISING_MANAGER_IFACE)
    
    app = Application(bus)
    advertisement = Advertisement(bus, 0, 'peripheral')
    
    mainloop = GLib.MainLoop()
    
    service_manager.RegisterApplication(app.get_path(), {},
                                       reply_handler=register_app_cb,
                                       error_handler=register_app_error_cb)
    
    ad_manager.RegisterAdvertisement(advertisement.get_path(), {},
                                     reply_handler=register_ad_cb,
                                     error_handler=register_ad_error_cb)
    
    print('[Echo] GATT server running with TX/RX/MESH characteristics')
    
    try:
        mainloop.run()
    except KeyboardInterrupt:
        pass
    finally:
        ad_manager.UnregisterAdvertisement(advertisement.get_path())
        service_manager.UnregisterApplication(app.get_path())

if __name__ == '__main__':
    main()
)";
        
        return script.str();
    }
};

BluezAdvertiser::BluezAdvertiser() 
    : pImpl_(std::make_unique<Impl>()), advertising_(false) {
}

BluezAdvertiser::~BluezAdvertiser() {
    stopAdvertising();
}

bool BluezAdvertiser::startAdvertising(const std::string& username, const std::string& fingerprint) {
    if (advertising_) {
        return true;
    }
    
    advertising_ = pImpl_->startAdvertising(username, fingerprint);
    return advertising_;
}

void BluezAdvertiser::stopAdvertising() {
    if (advertising_) {
        pImpl_->stopAdvertising();
        advertising_ = false;
    }
}

bool BluezAdvertiser::isAdvertising() const {
    return advertising_;
}

void BluezAdvertiser::setAdvertisingInterval(uint16_t minInterval, uint16_t maxInterval) {
    (void)minInterval;
    (void)maxInterval;
}

}

#endif