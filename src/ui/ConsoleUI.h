#pragma once

#include "core/bluetooth/BluetoothManager.h"
#include "core/crypto/UserIdentity.h"
#include <string>
#include <atomic>
#include <thread>
#include <memory>

namespace echo {

class ConsoleUI {
public:
    ConsoleUI();
    ~ConsoleUI();
    
    void run(BluetoothManager& bluetoothManager, UserIdentity& identity);
    
private:
    std::atomic<bool> running_;
    
    void printHelp() const;
    void printDevices(const BluetoothManager& bluetoothManager) const;
    void handleCommand(const std::string& command, BluetoothManager& bluetoothManager, UserIdentity& identity);
    
    // Event handlers for Bluetooth events
    void onDeviceDiscovered(const DiscoveredDevice& device);
    void onDeviceConnected(const std::string& address);
    void onDeviceDisconnected(const std::string& address);
    void onDataReceived(const std::string& address, const std::vector<uint8_t>& data);
};

} // namespace echo