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
                std::cout << "[Linux Advertiser] Username: " << truncatedUsername << std::endl;
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
        std::ostringstream manufacturerData; manufacturerData << "dbus.Array([dbus.Byte(0x11)"; for (unsigned char c : username) { manufacturerData << ", dbus.Byte(" << static_cast<int>(c) << ")"; } manufacturerData << "], signature='y')"; std::string md = manufacturerData.str(); std::ostringstream script; script << R"(#!/usr/bin/env python3
import dbus,dbus.mainloop.glib,dbus.service,sys,socket,os
from gi.repository import GLib
BLUEZ_SERVICE_NAME='org.bluez'
LE_ADVERTISING_MANAGER_IFACE='org.bluez.LEAdvertisingManager1'
DBUS_OM_IFACE='org.freedesktop.DBus.ObjectManager'
DBUS_PROP_IFACE='org.freedesktop.DBus.Properties'
LE_ADVERTISEMENT_IFACE='org.bluez.LEAdvertisement1'
GATT_MANAGER_IFACE='org.bluez.GattManager1'
GATT_SERVICE_IFACE='org.bluez.GattService1'
GATT_CHRC_IFACE='org.bluez.GattCharacteristic1'
APP_PATH='/org/bluez/echo/app'
SERVICE_PATH='/org/bluez/echo/service0'
CHRC_TX_PATH='/org/bluez/echo/service0/char0'
SOCK_PATH='/tmp/echo_gatt.sock'
class Advertisement(dbus.service.Object):
    def __init__(self,bus):
        self.path='/org/bluez/echo/advertisement0';self.bus=bus
        self.service_uuids=['F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C']
        self.manufacturer_data=dbus.Dictionary({dbus.UInt16(0xFFFF):)"; script << md; script << R"(},signature='qv')
        self.local_name=')"; script << deviceName; script << R"('
        dbus.service.Object.__init__(self,bus,self.path)
    def get_properties(self):
        return {LE_ADVERTISEMENT_IFACE:{'Type':'peripheral','ServiceUUIDs':dbus.Array(self.service_uuids,signature='s'),'LocalName':dbus.String(self.local_name),'ManufacturerData':self.manufacturer_data}}
    def get_path(self):return dbus.ObjectPath(self.path)
    @dbus.service.method(DBUS_PROP_IFACE,in_signature='s',out_signature='a{sv}')
    def GetAll(self,i):return self.get_properties()[LE_ADVERTISEMENT_IFACE]
    @dbus.service.method(LE_ADVERTISEMENT_IFACE,in_signature='',out_signature='')
    def Release(self):pass
class Application(dbus.service.Object):
    def __init__(self,bus):
        self.bus=bus
        dbus.service.Object.__init__(self,bus,APP_PATH)
    @dbus.service.method(DBUS_OM_IFACE, out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        o={}
        o[SERVICE_PATH]={GATT_SERVICE_IFACE:{'UUID':'F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C','Primary':dbus.Boolean(1)}}
        o[CHRC_TX_PATH]={GATT_CHRC_IFACE:{'UUID':'8E9B7A4C-2D5F-4B6A-9C3E-1F8D7B2A5C4E','Service':dbus.ObjectPath(SERVICE_PATH),'Flags':dbus.Array(['write','write-without-response'],signature='s')}}
        return o
class Service(dbus.service.Object):
    def __init__(self,bus):
        dbus.service.Object.__init__(self,bus,SERVICE_PATH)
class TxCharacteristic(dbus.service.Object):
    def __init__(self,bus,socket_sender):
        self.socket_sender=socket_sender
        dbus.service.Object.__init__(self,bus,CHRC_TX_PATH)
    @dbus.service.method(GATT_CHRC_IFACE,in_signature='aya{sv}',out_signature='')
    def WriteValue(self,value,options):
        try:self.socket_sender(bytes(value))
        except:pass
    @dbus.service.method(GATT_CHRC_IFACE,in_signature='a{sv}',out_signature='ay')
    def ReadValue(self,options):return dbus.Array([],signature='y')
def find_adapter(bus):
    om=dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME,'/'),DBUS_OM_IFACE);objs=om.GetManagedObjects()
    for o,p in objs.items():
        if LE_ADVERTISING_MANAGER_IFACE in p and GATT_MANAGER_IFACE in p:return o
    return None
def main():
    if os.path.exists(SOCK_PATH):
        try: os.remove(SOCK_PATH)
        except: pass
    server=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);server.bind(SOCK_PATH);server.listen(1)
    conn=[None]
    def socket_sender(data):
        if conn[0]:
            try:conn[0].sendall(data)
            except:pass
    def accept_conn():
        try:c,_=server.accept();conn[0]=c
        except:pass
        return True
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True);bus=dbus.SystemBus();adapter=find_adapter(bus)
    if not adapter:return
    props=dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME,adapter),DBUS_PROP_IFACE);props.Set('org.bluez.Adapter1','Powered',dbus.Boolean(1))
    ad_manager=dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME,adapter),LE_ADVERTISING_MANAGER_IFACE)
    gatt_manager=dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME,adapter),GATT_MANAGER_IFACE)
    advertisement=Advertisement(bus)
    app=Application(bus)
    Service(bus)
    TxCharacteristic(bus,socket_sender)
    mainloop=GLib.MainLoop();GLib.timeout_add_seconds(1,accept_conn)
    gatt_manager.RegisterApplication(dbus.ObjectPath(APP_PATH),{},reply_handler=lambda:None,error_handler=lambda e:None)
    ad_manager.RegisterAdvertisement(advertisement.get_path(),{},reply_handler=lambda:None,error_handler=lambda e:None)
    try:mainloop.run()
    except KeyboardInterrupt:pass
if __name__=='__main__':main()
)"; return script.str(); }
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