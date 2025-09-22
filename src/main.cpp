#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include "core/bluetooth/BluetoothManager.h"
#include "ui/ConsoleUI.h"

int main(int argc, char* argv[]) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    
    std::cout << "Echo - BitChat Compatible Desktop Messaging" << std::endl;
    std::cout << "============================================" << std::endl;
    
    try {
        // Initialize Bluetooth manager
        auto bluetoothManager = std::make_unique<echo::BluetoothManager>();
        
        // Initialize console UI
        auto consoleUI = std::make_unique<echo::ConsoleUI>();
        
        // Check if Bluetooth is available
        if (!bluetoothManager->isBluetoothAvailable()) {
            std::cerr << "Error: Bluetooth is not available on this system" << std::endl;
            return 1;
        }
        
        std::cout << "Bluetooth initialized successfully" << std::endl;
        
        // Start the application main loop
        consoleUI->run(*bluetoothManager);
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Echo shutting down..." << std::endl;
    return 0;
}