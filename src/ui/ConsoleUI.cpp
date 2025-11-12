#include "ConsoleUI.h"
#include "core/crypto/UserIdentity.h"
#include "core/network/WifiDirect.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#endif

namespace echo {

ConsoleUI::ConsoleUI() 
    : running_(false), currentChatMode_(ChatMode::NONE) {
}

ConsoleUI::~ConsoleUI() {
    running_ = false;
}

void ConsoleUI::run(BluetoothManager& bluetoothManager, UserIdentity& identity) {
    running_ = true;
    wifi_ = std::make_unique<echo::WifiDirect>();
    wifi_->setOnData([this](const std::string& /*src*/, const std::vector<uint8_t>& data) {
        onDataReceived("wifi", data);
    });
    wifi_->start(identity.getUsername(), identity.getFingerprint());
    
    bluetoothManager.setDeviceDiscoveredCallback(
        [this](const DiscoveredDevice& device) {
            onDeviceDiscovered(device);
        });
    
    bluetoothManager.setDeviceConnectedCallback(
        [this](const std::string& address) {
            onDeviceConnected(address);
        });
    
    bluetoothManager.setDeviceDisconnectedCallback(
        [this](const std::string& address) {
            onDeviceDisconnected(address);
        });
    
    bluetoothManager.setDataReceivedCallback(
        [this](const std::string& address, const std::vector<uint8_t>& data) {
            onDataReceived(address, data);
        });
    
    printHelp();
    
    std::string input;
    while (running_ && std::getline(std::cin, input)) {
        if (input.empty()) continue;
        
        input.erase(input.begin(), std::find_if(input.begin(), input.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        input.erase(std::find_if(input.rbegin(), input.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), input.end());
        
        if (currentChatMode_ != ChatMode::NONE) {
            handleChatMode(input, bluetoothManager, identity);
        } else {
            if (input == "quit" || input == "exit") {
                running_ = false;
                break;
            }
            handleCommand(input, bluetoothManager, identity);
        }
    }
    if (wifi_) { wifi_->stop(); wifi_.reset(); }
}

void ConsoleUI::printHelp() const {
    std::cout << "\n=== Echo Console Commands ===" << std::endl;
    std::cout << "scan              - Start scanning for devices" << std::endl;
    std::cout << "stop              - Stop scanning" << std::endl;
    std::cout << "connect <addr|@user> - Connect to a device by BLE address or username" << std::endl;
    std::cout << "services <addr|@user> - List GATT services/characteristics for a connected device" << std::endl;
    std::cout << "devices           - List all discovered devices" << std::endl;
    std::cout << "echo              - List only Echo devices" << std::endl;
    std::cout << "/chat @username   - Start personal chat" << std::endl;
    std::cout << "/join #global     - Join global chat" << std::endl;
    std::cout << "/msg @user text   - Send quick message" << std::endl;
    std::cout << "/who              - List online Echo users" << std::endl;
    std::cout << "whoami            - Show your identity" << std::endl;
    std::cout << "/nick <name>      - Change your username" << std::endl;
    std::cout << "clear             - Clear screen" << std::endl;
    std::cout << "help              - Show this help" << std::endl;
    std::cout << "quit/exit         - Exit application" << std::endl;
    std::cout << "==============================\n" << std::endl;
}

void ConsoleUI::printChatHelp() const {
    std::cout << "\n=== Chat Mode Commands ===" << std::endl;
    std::cout << "/exit             - Exit chat mode" << std::endl;
    std::cout << "/who              - List participants" << std::endl;
    std::cout << "/status           - Show current chat info" << std::endl;
    std::cout << "/help             - Show this help" << std::endl;
    std::cout << "Type messages and press Enter to send" << std::endl;
    std::cout << "==========================\n" << std::endl;
}

void ConsoleUI::handleCommand(const std::string& command, BluetoothManager& bluetoothManager, UserIdentity& identity) {
    auto cmd = commandParser_.parse(command);
    
    if (!cmd.isValid && !command.empty()) {
        std::istringstream iss(command);
        std::string simpleCmd;
        iss >> simpleCmd;
        
        if (simpleCmd == "scan") cmd.type = CommandType::SCAN;
        else if (simpleCmd == "stop") cmd.type = CommandType::STOP;
        else if (simpleCmd == "devices") cmd.type = CommandType::DEVICES;
        else if (simpleCmd == "echo") cmd.type = CommandType::ECHO_DEVICES;
        else if (simpleCmd == "services") {
            cmd.type = CommandType::STATUS;
            if (iss >> simpleCmd) {
                cmd.target = simpleCmd;
            }
        }
    else if (simpleCmd == "whoami") cmd.type = CommandType::WHOAMI;
    else if (simpleCmd == "wifi") { cmd.type = CommandType::STATUS; cmd.target = "__wifi_peers"; }
        else if (simpleCmd == "help") cmd.type = CommandType::HELP;
        else if (simpleCmd == "clear" || simpleCmd == "cls") cmd.type = CommandType::CLEAR;
        else if (simpleCmd == "quit" || simpleCmd == "exit") cmd.type = CommandType::QUIT;
        else {
            std::cout << "Unknown command: " << simpleCmd << ". Type 'help' for available commands." << std::endl;
            std::cout << getPrompt();
            return;
        }
        cmd.isValid = true;
    }
    
    switch (cmd.type) {
        case CommandType::SCAN:
            if (bluetoothManager.startScanning()) {
                std::cout << "Started scanning for devices..." << std::endl;
            } else {
                std::cout << "Failed to start scanning" << std::endl;
            }
            break;
        case CommandType::CONNECT:
            if (!cmd.target.empty()) {
                bool ok = connectByTarget(cmd.target, bluetoothManager);
                if (!ok) {
                    std::cout << "Failed to connect. Use 'devices' or 'echo' to list targets." << std::endl;
                }
            } else {
                std::cout << "Usage: connect <address|@username>" << std::endl;
            }
            break;
            
        case CommandType::STOP:
            bluetoothManager.stopScanning();
            std::cout << "Stopped scanning" << std::endl;
            break;
            
        case CommandType::DEVICES:
            printDevices(bluetoothManager);
            break;
            
        case CommandType::ECHO_DEVICES:
            printEchoDevices(bluetoothManager);
            break;
            
        case CommandType::CHAT:
            if (!cmd.target.empty()) {
                enterPersonalChat(cmd.target, bluetoothManager);
            } else {
                std::cout << "Usage: /chat @username" << std::endl;
            }
            break;
            
        case CommandType::JOIN:
            if (cmd.target.empty() || cmd.target == "#global" || cmd.target == "global") {
                enterGlobalChat(bluetoothManager);
            } else {
                std::cout << "Currently only #global channel is supported" << std::endl;
            }
            break;
            
        case CommandType::MSG:
            if (!cmd.target.empty() && !cmd.message.empty()) {
                currentChatTarget_ = cmd.target;
                sendMessage(cmd.message, bluetoothManager, identity);
                displayMessage("You -> " + cmd.target, cmd.message, true);
            } else {
                std::cout << "Usage: /msg @username message" << std::endl;
            }
            break;
            
        case CommandType::WHO:
            printEchoDevices(bluetoothManager);
            break;
            
        case CommandType::WHOAMI:
            std::cout << "\nYour Echo Identity:" << std::endl;
            std::cout << "  Username: " << identity.getUsername() << std::endl;
            std::cout << "  Fingerprint: " << identity.getFingerprint() << std::endl;
            std::cout << std::endl;
            break;
            
        case CommandType::NICK:
            if (!cmd.target.empty()) {
                identity.setUsername(cmd.target);
                identity.saveToFile("echo_identity.dat");
                std::cout << "Username changed to: " << cmd.target << std::endl;
                std::cout << "Note: Restart Echo for the new name to be advertised" << std::endl;
            } else {
                std::cout << "Usage: /nick <new_username>" << std::endl;
            }
            break;
            
        case CommandType::CLEAR:
            clearScreen();
            break;
            
        case CommandType::HELP:
            printHelp();
            return;
            
        case CommandType::STATUS:
            if (!cmd.target.empty()) {
                if (cmd.target == "__wifi_peers") {
                    if (wifi_) {
                        auto peers = wifi_->listPeers();
                        if (peers.empty()) {
                            std::cout << "No Wi-Fi peers discovered" << std::endl;
                        } else {
                            std::cout << "Wi-Fi peers (username -> ip:port):" << std::endl;
                            for (auto& p : peers) {
                                std::cout << "  " << p.first << " -> " << p.second << std::endl;
                            }
                        }
                    } else {
                        std::cout << "Wi-Fi module not initialized" << std::endl;
                    }
                } else {
                    std::string addr = cmd.target;
                    if (addr[0] == '@') addr = findAddressByUsername(addr.substr(1), bluetoothManager);
                    if (!addr.empty()) bluetoothManager.debugPrintServices(addr);
                    else std::cout << "Target not found" << std::endl;
                }
            } else {
                if (currentChatMode_ == ChatMode::GLOBAL) {
                    std::cout << "In global chat (#global)" << std::endl;
                } else if (currentChatMode_ == ChatMode::PERSONAL) {
                    std::cout << "In personal chat with: " << currentChatTarget_ << std::endl;
                } else {
                    std::cout << "Not in chat mode" << std::endl;
                }
            }
            break;
        default:
            break;
    }
    
    std::cout << getPrompt();
}

void ConsoleUI::handleChatMode(const std::string& input, BluetoothManager& bluetoothManager, UserIdentity& identity) {
    if (input == "/exit") {
        exitChatMode();
        return;
    }
    
    if (input == "/help") {
        printChatHelp();
        std::cout << getPrompt();
        return;
    }
    
    if (input == "/who") {
        if (currentChatMode_ == ChatMode::GLOBAL) {
            printEchoDevices(bluetoothManager);
        } else {
            std::cout << "Chatting with: " << currentChatTarget_ << std::endl;
        }
        std::cout << getPrompt();
        return;
    }
    
    if (input == "/status") {
        if (currentChatMode_ == ChatMode::GLOBAL) {
            std::cout << "In global chat (#global)" << std::endl;
        } else {
            std::cout << "In personal chat with: " << currentChatTarget_ << std::endl;
        }
        std::cout << getPrompt();
        return;
    }
    
    if (!input.empty() && input[0] != '/') {
        sendMessage(input, bluetoothManager, identity);
        
        if (currentChatMode_ == ChatMode::GLOBAL) {
            displayMessage("You", input, false);
        } else {
            displayMessage("You", input, true);
        }
    }
    
    std::cout << getPrompt();
}

void ConsoleUI::enterPersonalChat(const std::string& username, BluetoothManager& bluetoothManager) {
    auto echoDevices = bluetoothManager.getEchoDevices();
    
    bool found = false;
    for (const auto& device : echoDevices) {
        if (device.echoUsername == username) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        std::cout << "User '" << username << "' not found. Run 'echo' to see online users." << std::endl;
        std::cout << getPrompt();
        return;
    }
    
    currentChatMode_ = ChatMode::PERSONAL;
    currentChatTarget_ = username;
    
    clearScreen();
    std::cout << "=== Personal Chat with " << username << " ===" << std::endl;
    std::cout << "Type /exit to leave chat, /help for commands" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    
    std::cout << getPrompt();
}

void ConsoleUI::enterGlobalChat(BluetoothManager& bluetoothManager) {
    currentChatMode_ = ChatMode::GLOBAL;
    currentChatTarget_ = "#global";
    
    clearScreen();
    std::cout << "=== Global Chat (#global) ===" << std::endl;
    std::cout << "Broadcasting to all Echo devices in range" << std::endl;
    std::cout << "Type /exit to leave chat, /help for commands" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    
    auto echoDevices = bluetoothManager.getEchoDevices();
    if (!echoDevices.empty()) {
        std::cout << "Online users: ";
        for (size_t i = 0; i < echoDevices.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << echoDevices[i].echoUsername;
        }
        std::cout << std::endl;
    }
    std::cout << std::string(40, '-') << std::endl;
    
    std::cout << getPrompt();
}

void ConsoleUI::exitChatMode() {
    std::cout << "Exiting chat mode..." << std::endl;
    currentChatMode_ = ChatMode::NONE;
    currentChatTarget_.clear();
    std::cout << getPrompt();
}

void ConsoleUI::sendMessage(const std::string& message, BluetoothManager& bluetoothManager, UserIdentity& identity) {
    bool isGlobal = (currentChatMode_ == ChatMode::GLOBAL);
    
    auto msg = MessageFactory::createTextMessage(
        message,
        identity.getUsername(),
        identity.getFingerprint(),
        currentChatTarget_,
        isGlobal
    );
    
    auto data = msg.serialize();
    
    std::cout << "\n[DEBUG] Message serialized: " << data.size() << " bytes" << std::endl;
    
    if (isGlobal) {
        auto devices = bluetoothManager.getEchoDevices();
        size_t okCount = 0;
        for (const auto& device : devices) {
            if (bluetoothManager.sendData(device.address, data)) okCount++;
        }
        std::cout << "[GLOBAL] BLE sent to " << okCount << "/" << devices.size() << " peers" << std::endl;
        if (wifi_) {
            bool any = wifi_->sendBroadcast(data);
            std::cout << "[GLOBAL] WIFI broadcast " << (any ? "ok" : "no peers") << std::endl;
        }
    } else {
        auto targetAddress = findAddressByUsername(currentChatTarget_, bluetoothManager);
        if (!targetAddress.empty()) {
            std::cout << "[INFO] Attempting to send to " << currentChatTarget_ 
                     << " at " << targetAddress << std::endl;
            bool sent = bluetoothManager.sendData(targetAddress, data);
            if (!sent) {
                if (wifi_) {
                    bool ok = wifi_->sendTo(currentChatTarget_, data);
                    if (!ok) std::cout << "[INFO] WIFI fallback could not find peer '" << currentChatTarget_ << "'" << std::endl;
                }
            }
        } else {
            std::cout << "[ERROR] Could not find address for user: " << currentChatTarget_ << std::endl;
        }
    }
}

void ConsoleUI::displayMessage(const std::string& from, const std::string& message, bool isPrivate) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
#ifdef _WIN32
    std::tm tm;
    localtime_s(&tm, &time_t);
#else
    std::tm tm = *std::localtime(&time_t);
#endif
    
    std::cout << std::endl;
    std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] ";
    
    if (isPrivate) {
        std::cout << "[DM] ";
    } else {
        std::cout << "[#global] ";
    }
    
    std::cout << from << ": " << message << std::endl;
    
    addToHistory(from + ": " + message);
}

void ConsoleUI::addToHistory(const std::string& message) {
    std::lock_guard<std::mutex> lock(historyMutex_);
    messageHistory_.push_back(message);
    
    if (messageHistory_.size() > MAX_HISTORY) {
        messageHistory_.pop_front();
    }
}

void ConsoleUI::clearScreen() const {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

std::string ConsoleUI::getPrompt() const {
    if (currentChatMode_ == ChatMode::GLOBAL) {
        return "[#global]> ";
    } else if (currentChatMode_ == ChatMode::PERSONAL) {
        return "[" + currentChatTarget_ + "]> ";
    } else {
        return "echo> ";
    }
}

void ConsoleUI::printDevices(const BluetoothManager& bluetoothManager) const {
    auto devices = bluetoothManager.getDiscoveredDevices();
    
    if (devices.empty()) {
        std::cout << "No devices discovered. Run 'scan' to search for devices." << std::endl;
        return;
    }
    
    std::vector<const DiscoveredDevice*> echoDevices;
    std::vector<const DiscoveredDevice*> regularDevices;
    
    for (const auto& device : devices) {
        if (device.isEchoDevice) {
            echoDevices.push_back(&device);
        } else {
            regularDevices.push_back(&device);
        }
    }
    
    if (!echoDevices.empty()) {
        std::cout << "\n=== Echo Network Devices ===" << std::endl;
        std::cout << std::left << std::setw(20) << "Username" 
                  << std::setw(12) << "Fingerprint"
                  << std::setw(18) << "Address" 
                  << std::setw(8) << "RSSI"
                  << "Last Seen" << std::endl;
        std::cout << std::string(75, '-') << std::endl;
        
        for (const auto* device : echoDevices) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - device->lastSeen).count();
            
            std::string fingerprint = device->echoFingerprint.substr(0, 8) + "...";
            
            std::cout << std::left << std::setw(20) << device->echoUsername
                      << std::setw(12) << fingerprint
                      << std::setw(18) << device->address
                      << std::setw(8) << device->rssi
                      << elapsed << "s ago" << std::endl;
        }
    }
    
    if (!regularDevices.empty()) {
        std::cout << "\n=== Other Bluetooth Devices ===" << std::endl;
        std::cout << std::left << std::setw(20) << "Name" 
                  << std::setw(18) << "Address" 
                  << std::setw(8) << "RSSI"
                  << "Last Seen" << std::endl;
        std::cout << std::string(65, '-') << std::endl;
        
        for (const auto* device : regularDevices) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - device->lastSeen).count();
            
            std::cout << std::left << std::setw(20) << device->name.substr(0, 19)
                      << std::setw(18) << device->address
                      << std::setw(8) << device->rssi
                      << elapsed << "s ago" << std::endl;
        }
    }
    
    std::cout << std::endl;
}

void ConsoleUI::printEchoDevices(const BluetoothManager& bluetoothManager) const {
    auto devices = bluetoothManager.getEchoDevices();
    
    if (devices.empty()) {
        std::cout << "No Echo devices found. Run 'scan' to search." << std::endl;
        return;
    }
    
    std::cout << "\n=== Online Echo Users ===" << std::endl;
    std::cout << std::left << std::setw(20) << "Username" 
              << std::setw(10) << "OS"
              << std::setw(8) << "Signal"
              << "Status" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    for (const auto& device : devices) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - device.lastSeen).count();
        
        std::string status = elapsed < 10 ? "Active" : 
                           elapsed < 30 ? "Online" : "Away";
        
        std::string signal = std::to_string(device.rssi) + " dBm";
        
        std::cout << std::left << std::setw(20) << device.echoUsername
                  << std::setw(10) << device.osType
                  << std::setw(8) << signal
                  << status << std::endl;
    }
    
    std::cout << "\nTotal: " << devices.size() << " Echo user(s) online" << std::endl;
    std::cout << std::endl;
}

void ConsoleUI::onDeviceDiscovered(const DiscoveredDevice& device) {
    if (currentChatMode_ != ChatMode::NONE) {
        return;
    }
    
    if (device.isEchoDevice) {
        std::cout << "\n[ECHO USER ONLINE] " << device.echoUsername 
                  << " (" << device.osType << ") "
                  << "Signal: " << device.rssi << " dBm" << std::endl;
    }
    std::cout << getPrompt();
    std::cout.flush();
}

void ConsoleUI::onDeviceConnected(const std::string& address) {
    std::cout << "\n[CONNECTED] Device " << address << " connected" << std::endl;
    std::cout << getPrompt();
    std::cout.flush();
}

void ConsoleUI::onDeviceDisconnected(const std::string& address) {
    std::cout << "\n[DISCONNECTED] Device " << address << " disconnected" << std::endl;
    std::cout << getPrompt();
    std::cout.flush();
}

void ConsoleUI::onDataReceived(const std::string& address, const std::vector<uint8_t>& data) {
    try {
        auto msg = Message::deserialize(data);
        processReceivedMessage(msg, address);
    } catch (const std::exception&) {
        if (currentChatMode_ == ChatMode::NONE) {
            std::cout << "\n[DATA] Received " << data.size() << " bytes from " << address << std::endl;
            std::cout << getPrompt();
            std::cout.flush();
        }
    }
}

void ConsoleUI::processReceivedMessage(const Message& msg, const std::string& /* sourceAddress */) {
    if (msg.header.type == MessageType::TEXT_MESSAGE || 
        msg.header.type == MessageType::GLOBAL_MESSAGE ||
        msg.header.type == MessageType::PRIVATE_MESSAGE) {
        
        auto textMsg = TextMessage::deserialize(msg.payload);
        
        if (textMsg.isGlobal && currentChatMode_ == ChatMode::GLOBAL) {
            displayMessage(textMsg.senderUsername, textMsg.content, false);
            std::cout << getPrompt();
            std::cout.flush();
        } else if (!textMsg.isGlobal) {
            if (currentChatMode_ == ChatMode::PERSONAL && 
                currentChatTarget_ == textMsg.senderUsername) {
                displayMessage(textMsg.senderUsername, textMsg.content, true);
            } else {
                std::cout << "\n[NEW MESSAGE from " << textMsg.senderUsername << "]: " 
                         << textMsg.content << std::endl;
            }
            std::cout << getPrompt();
            std::cout.flush();
        }
    }
}

std::string ConsoleUI::findUsernameByAddress(const std::string& address, const BluetoothManager& bluetoothManager) const {
    auto devices = bluetoothManager.getEchoDevices();
    for (const auto& device : devices) {
        if (device.address == address) {
            return device.echoUsername;
        }
    }
    return "";
}

std::string ConsoleUI::findAddressByUsername(const std::string& username, const BluetoothManager& bluetoothManager) const {
    auto devices = bluetoothManager.getEchoDevices();
    for (const auto& device : devices) {
        if (device.echoUsername == username) {
            return device.address;
        }
    }
    return "";
}

bool ConsoleUI::connectByTarget(const std::string& target, BluetoothManager& bluetoothManager) {
    std::string addr = target;
    if (!target.empty() && target[0] == '@') {
        addr = findAddressByUsername(target.substr(1), bluetoothManager);
    }
    if (addr.empty()) return false;
    bool ok = bluetoothManager.connectToDevice(addr);
    if (ok) {
        std::cout << "Connecting to " << addr << "..." << std::endl;
    }
    return ok;
}

} // namespace echo