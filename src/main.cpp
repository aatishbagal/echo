#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>

#include "core/bluetooth/BluetoothManager.h"
#include "core/crypto/UserIdentity.h"
#include "core/mesh/MeshNetwork.h"
#include "core/protocol/MessageTypes.h"
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
        
        // Initialize mesh network
        auto meshNetwork = std::make_shared<echo::MeshNetwork>();
        meshNetwork->setLocalUsername(identity.getUsername());
        meshNetwork->setLocalFingerprint(identity.getFingerprint());
        
        // Initialize console UI
        auto consoleUI = std::make_unique<echo::ConsoleUI>();
        
        // Wire up mesh network callbacks
        meshNetwork->setMessageCallback([&consoleUI](const echo::Message& msg, const std::string& sourceAddress) {
            std::cout << "\n[Mesh] Received message type " << static_cast<int>(msg.header.type) 
                     << " from " << sourceAddress << std::endl;
            
            if (msg.header.type == echo::MessageType::GLOBAL_MESSAGE ||
                msg.header.type == echo::MessageType::PRIVATE_MESSAGE ||
                msg.header.type == echo::MessageType::TEXT_MESSAGE) {
                try {
                    auto textMsg = echo::TextMessage::deserialize(msg.payload);
                    std::cout << "[" << textMsg.senderUsername << "]: " << textMsg.content << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[Mesh] Failed to parse text message: " << e.what() << std::endl;
                }
            }
        });
        
        bluetoothManager->setMeshNetwork(meshNetwork);
        
        bluetoothManager->setDeviceDiscoveredCallback([&meshNetwork](const echo::DiscoveredDevice& device) {
            if (device.isEchoDevice) {
                meshNetwork->addPeer(device.address, device.echoUsername);
            }
        });
        
        bluetoothManager->setDeviceConnectedCallback([&meshNetwork](const std::string& address) {
            std::cout << "[Mesh] Peer connected: " << address << std::endl;
        });
        
        bluetoothManager->setDeviceDisconnectedCallback([&meshNetwork](const std::string& address) {
            meshNetwork->removePeer(address);
        });
        
        // Check if Bluetooth is available
        if (!bluetoothManager->isBluetoothAvailable()) {
            std::cerr << "Error: Bluetooth is not available on this system" << std::endl;
            return 1;
        }
        
        std::cout << "Bluetooth initialized successfully" << std::endl;
        
        std::thread cleanupThread([&meshNetwork]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(60));
                meshNetwork->cleanupOldMessages();
            }
        });
        cleanupThread.detach();
        
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