#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>

#include "core/bluetooth/BluetoothManager.h"
#include "core/crypto/UserIdentity.h"
#include "ui/ConsoleUI.h"

int main(int argc, char* argv[]) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    
    std::cout << "Echo - BitChat Compatible Desktop Messaging" << std::endl;
    std::cout << "============================================" << std::endl;
    
    try {
        // Load or create user identity
        std::string identityPath = "echo_identity.dat";
        echo::UserIdentity identity;
        
        if (std::filesystem::exists(identityPath)) {
            std::cout << "Loading existing identity..." << std::endl;
            if (identity.loadFromFile(identityPath)) {
                std::cout << "Identity loaded successfully" << std::endl;
            } else {
                std::cout << "Failed to load identity, generating new one..." << std::endl;
                identity = echo::UserIdentity::generate();
                identity.saveToFile(identityPath);
            }
        } else {
            std::cout << "Generating new identity..." << std::endl;
            identity = echo::UserIdentity::generate();
            if (identity.saveToFile(identityPath)) {
                std::cout << "Identity saved to " << identityPath << std::endl;
            }
        }
        
        // Display identity
        std::cout << "\nYour Echo Identity:" << std::endl;
        std::cout << "  Username: " << identity.getUsername() << std::endl;
        std::cout << "  Fingerprint: " << identity.getFingerprint() << std::endl;
        std::cout << std::endl;
        
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
        
        // Start advertising Echo presence
        std::cout << "\nStarting Echo advertising..." << std::endl;
        if (bluetoothManager->startEchoAdvertising(identity.getUsername(), identity.getFingerprint())) {
            std::cout << "Now visible to other Echo devices" << std::endl;
        } else {
            std::cout << "Warning: Could not start advertising (scanning will still work)" << std::endl;
        }
        std::cout << std::endl;
        
        // Start the application main loop
        consoleUI->run(*bluetoothManager, identity);
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Echo shutting down..." << std::endl;
    return 0;
}