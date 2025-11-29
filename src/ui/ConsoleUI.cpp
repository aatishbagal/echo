#include "ConsoleUI.h"
#include "core/crypto/UserIdentity.h"
#include "core/network/WifiDirect.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <unordered_set>

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
        [this, &bluetoothManager](const std::string& address) {
            onDeviceConnected(address, bluetoothManager);
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
    std::cout << "/file 'path'      - Send file to #global (size limit)" << std::endl;
    std::cout << "/accept <id>      - Accept a received file" << std::endl;
    std::cout << "/decline <id>     - Decline a received file" << std::endl;
    std::cout << "/who              - List online Echo users" << std::endl;
    std::cout << "whoami            - Show your identity" << std::endl;
    std::cout << "/nick <n>      - Change your username" << std::endl;
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
    if (command.rfind("/file", 0) == 0 && currentChatMode_ != ChatMode::GLOBAL) {
        size_t p1 = command.find('\'');
        size_t p2 = command.find('\'', p1 == std::string::npos ? 0 : p1 + 1);
        if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1 + 1) {
            std::string path = command.substr(p1 + 1, p2 - p1 - 1);
            bool ok = handleFileSend(path, bluetoothManager, identity);
            std::cout << (ok ? "[GLOBAL] sent" : "[GLOBAL] failed") << std::endl;
            std::cout << getPrompt();
            return;
        } else {
            std::cout << "Usage: /file 'full_path'" << std::endl;
            std::cout << getPrompt();
            return;
        }
    }
    if (command.rfind("/accept", 0) == 0 && currentChatMode_ != ChatMode::GLOBAL) {
        std::istringstream iss(command);
        std::string cmd, id; iss >> cmd >> id;
        if (!id.empty()) handleFileAccept(id);
        else std::cout << "Usage: /accept <id>" << std::endl;
        std::cout << getPrompt();
        return;
    }
    if (command.rfind("/decline", 0) == 0 && currentChatMode_ != ChatMode::GLOBAL) {
        std::istringstream iss(command);
        std::string cmd, id; iss >> cmd >> id;
        if (!id.empty()) handleFileDecline(id);
        else std::cout << "Usage: /decline <id>" << std::endl;
        std::cout << getPrompt();
        return;
    }
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
            else if (simpleCmd == "wifi") {
                std::string sub;
                if (iss >> sub) {
                    if (sub == "start") {
                        if (!wifi_) {
                            wifi_ = std::make_unique<echo::WifiDirect>();
                            wifi_->setOnData([this](const std::string& /*src*/, const std::vector<uint8_t>& data) { onDataReceived("wifi", data); });
                            wifi_->start(identity.getUsername(), identity.getFingerprint());
                        }
                        wifi_->setVerbose(true);
                        std::cout << "wifi verbose on" << std::endl;
                        return;
                    } else if (sub == "stop") {
                        if (wifi_) wifi_->setVerbose(false);
                        std::cout << "wifi verbose off" << std::endl;
                        return;
                    } else if (sub == "peers") {
                        cmd.type = CommandType::STATUS; cmd.target = "__wifi_peers";
                    } else {
                        std::cout << "wifi start|stop|peers" << std::endl;
                        return;
                    }
                } else {
                    std::cout << "wifi start|stop|peers" << std::endl;
                    return;
                }
            }
            else if (simpleCmd == "clear" || simpleCmd == "cls") cmd.type = CommandType::CLEAR;
            else if (simpleCmd == "help") cmd.type = CommandType::HELP;
            else if (simpleCmd == "connect") {
                std::string t; if (iss >> t) { cmd.type = CommandType::CONNECT; cmd.target = t; }
            }
        cmd.isValid = (cmd.type != CommandType::NONE);
    }

    switch (cmd.type) {
        case CommandType::SCAN:
            if (!bluetoothManager.isScanning()) {
                bluetoothManager.startScanning();
            } else {
                std::cout << "Already scanning" << std::endl;
            }
            break;

        case CommandType::STOP:
            if (bluetoothManager.isScanning()) {
                bluetoothManager.stopScanning();
            } else {
                std::cout << "Not scanning" << std::endl;
            }
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
            if (cmd.target == "#global" || cmd.target == "global") {
                enterGlobalChat(bluetoothManager);
            } else {
                std::cout << "Unknown channel: " << cmd.target << std::endl;
            }
            break;

        case CommandType::MSG:
            if (!cmd.target.empty() && !cmd.message.empty()) {
                auto msg = MessageFactory::createTextMessage(
                    cmd.message,
                    "You",
                    "",
                    cmd.target,
                    false
                );

                std::cout << "[Sent to " << cmd.target << "]: " << cmd.message << std::endl;
            } else {
                std::cout << "Usage: /msg @user message" << std::endl;
            }
            break;

        case CommandType::WHO:
            printEchoDevices(bluetoothManager);
            break;

        case CommandType::WHOAMI:
            std::cout << "\n=== Your Identity ===" << std::endl;
            std::cout << "Username: " << identity.getUsername() << std::endl;
            std::cout << "Fingerprint: " << identity.getFingerprint() << std::endl;
            std::cout << "=====================\n" << std::endl;
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
            break;

        case CommandType::CONNECT:
            if (!cmd.target.empty()) {
                connectByTarget(cmd.target, bluetoothManager);
            } else {
                std::cout << "Usage: connect <address|@username>" << std::endl;
            }
            break;

        case CommandType::STATUS:
            if (cmd.target == "__wifi_peers") {
                if (wifi_) {
                    auto peers = wifi_->listPeers();
                    if (peers.empty()) {
                        std::cout << "No WiFi peers" << std::endl;
                    } else {
                        std::cout << "\n=== WiFi Peers ===" << std::endl;
                        for (auto& p : peers) {
                            std::cout << p.first << " at " << p.second << std::endl;
                        }
                        std::cout << "==================\n" << std::endl;
                    }
                } else {
                    std::cout << "WiFi not running" << std::endl;
                }
            } else if (!cmd.target.empty()) {
                std::string addr = cmd.target;
                if (!cmd.target.empty() && cmd.target[0] == '@') {
                    addr = findAddressByUsername(cmd.target.substr(1), bluetoothManager);
                }
                if (!addr.empty()) {
                    bluetoothManager.debugPrintServices(addr);
                } else {
                    std::cout << "Device not found" << std::endl;
                }
            }
            break;

        default:
            if (!command.empty()) {
                std::cout << "Unknown command. Type 'help' for available commands." << std::endl;
            }
            break;
    }
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
            auto devices = bluetoothManager.getEchoDevices();
            std::cout << "\n=== Online Users ===" << std::endl;
            if (wifi_) {
                auto peers = wifi_->listPeers();
                for (auto& p : peers) {
                    std::cout << "  " << p.first << " [LAN]" << std::endl;
                }
            }
            for (const auto& device : devices) {
                std::cout << "  " << device.echoUsername << std::endl;
            }
            std::cout << "===================\n" << std::endl;
        } else if (currentChatMode_ == ChatMode::PERSONAL) {
            std::cout << "Chatting with: " << currentChatTarget_ << std::endl;
        }
        std::cout << getPrompt();
        return;
    }

    if (input == "/status") {
        if (currentChatMode_ == ChatMode::GLOBAL) {
            std::cout << "In global chat (#global)" << std::endl;
        } else if (currentChatMode_ == ChatMode::PERSONAL) {
            std::cout << "In personal chat with: " << currentChatTarget_ << std::endl;
        }
        std::cout << getPrompt();
        return;
    }

    if (input.rfind("/file", 0) == 0) {
        if (currentChatMode_ != ChatMode::GLOBAL) {
            std::cout << "File sharing only in #global" << std::endl;
            std::cout << getPrompt();
            return;
        }
        size_t p1 = input.find('\'');
        size_t p2 = input.find('\'', p1 == std::string::npos ? 0 : p1 + 1);
        if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1 + 1) {
            std::string path = input.substr(p1 + 1, p2 - p1 - 1);
            bool ok = handleFileSend(path, bluetoothManager, identity);
            std::cout << (ok ? "[GLOBAL] sent" : "[GLOBAL] failed") << std::endl;
        } else {
            std::cout << "Usage: /file 'full_path'" << std::endl;
        }
        std::cout << getPrompt();
        return;
    }

    if (!input.empty() && input[0] != '/') {
        sendMessage(input, bluetoothManager, identity);
    }

    std::cout << getPrompt();
}

void ConsoleUI::enterPersonalChat(const std::string& username, BluetoothManager& bluetoothManager) {
    (void)bluetoothManager;
    currentChatMode_ = ChatMode::PERSONAL;
    currentChatTarget_ = username;

    std::cout << "\n=== Entering Personal Chat ===" << std::endl;
    std::cout << "Chatting with: " << currentChatTarget_ << std::endl;
    std::cout << "Type /exit to leave, /help for commands" << std::endl;
    std::cout << "==============================\n" << std::endl;

    printChatHelp();
    std::cout << getPrompt();
}

void ConsoleUI::enterGlobalChat(BluetoothManager& bluetoothManager) {
    (void)bluetoothManager;
    currentChatMode_ = ChatMode::GLOBAL;
    currentChatTarget_ = "";

    std::cout << "\n=== Joining Global Chat ===" << std::endl;
    std::cout << "Channel: #global" << std::endl;
    std::cout << "Type /exit to leave, /help for commands" << std::endl;
    std::cout << "===========================\n" << std::endl;

    printChatHelp();
    std::cout << getPrompt();
}

void ConsoleUI::exitChatMode() {
    std::cout << "\nExiting chat mode..." << std::endl;
    currentChatMode_ = ChatMode::NONE;
    currentChatTarget_ = "";
    std::cout << getPrompt();
}

void ConsoleUI::sendMessage(const std::string& message, BluetoothManager& bluetoothManager, UserIdentity& identity) {
    if (currentChatMode_ == ChatMode::GLOBAL) {
        auto msg = MessageFactory::createTextMessage(
            message,
            identity.getUsername(),
            identity.getFingerprint(),
            "",
            true
        );

        auto data = msg.serialize();
        bool sent = false;

        if (wifi_) {
            sent = wifi_->sendBroadcast(data) || sent;
        }

        auto devices = bluetoothManager.getEchoDevices();
        for (const auto& device : devices) {
            sent = bluetoothManager.sendData(device.address, data) || sent;
        }

        displayMessage("You", message, false);

    } else if (currentChatMode_ == ChatMode::PERSONAL) {
        auto msg = MessageFactory::createTextMessage(
            message,
            identity.getUsername(),
            identity.getFingerprint(),
            currentChatTarget_,
            false
        );

        auto data = msg.serialize();
        bool sent = false;

        if (wifi_) {
            sent = wifi_->sendTo(currentChatTarget_, data) || sent;
        }

        std::string address = findAddressByUsername(currentChatTarget_, bluetoothManager);
        if (!address.empty()) {
            sent = bluetoothManager.sendData(address, data) || sent;
        }

        displayMessage("You", message, true);
    }
}

void ConsoleUI::displayMessage(const std::string& from, const std::string& message, bool isPrivate) {
    std::lock_guard<std::mutex> lock(historyMutex_);

    std::string formattedMsg;
    if (isPrivate) {
        formattedMsg = "[" + from + "]: " + message;
    } else {
        formattedMsg = "[#global][" + from + "]: " + message;
    }

    std::cout << formattedMsg << std::endl;
    addToHistory(formattedMsg);
}

void ConsoleUI::addToHistory(const std::string& message) {
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
        return "[@" + currentChatTarget_ + "]> ";
    }
    return "> ";
}

void ConsoleUI::printDevices(const BluetoothManager& bluetoothManager) const {
    auto devices = bluetoothManager.getDiscoveredDevices();

    std::cout << "\n=== Discovered Devices ===" << std::endl;
    if (devices.empty()) {
        std::cout << "No devices found. Start scanning with 'scan'" << std::endl;
    } else {
        for (const auto& device : devices) {
            std::cout << "  " << device.name
                     << " (" << device.address << ")"
                     << " RSSI: " << device.rssi << " dBm";

            if (device.isEchoDevice) {
                std::cout << " [Echo: " << device.echoUsername << "]";
            }

            std::cout << std::endl;
        }
    }
    std::cout << "=========================\n" << std::endl;
}

void ConsoleUI::printEchoDevices(const BluetoothManager& bluetoothManager) const {
    auto devices = bluetoothManager.getEchoDevices();

    std::cout << "\n=== Echo Devices ===" << std::endl;

    if (wifi_) {
        auto peers = wifi_->listPeers();
        for (auto& p : peers) {
            std::cout << "  @" << p.first << " [LAN]" << std::endl;
        }
    }

    if (devices.empty()) {
        if (!wifi_ || wifi_->listPeers().empty()) {
            std::cout << "No Echo devices found" << std::endl;
        }
    } else {
        for (const auto& device : devices) {
            std::cout << "  @" << device.echoUsername
                     << " (" << device.address << ")"
                     << " RSSI: " << device.rssi << " dBm"
                     << std::endl;
        }
    }
    std::cout << "===================\n" << std::endl;
}

void ConsoleUI::onDeviceDiscovered(const DiscoveredDevice& device) {
    (void)device;
}

void ConsoleUI::onDeviceConnected(const std::string& address, const BluetoothManager& bluetoothManager) {
    std::string username = findUsernameByAddress(address, bluetoothManager);
    if (!username.empty()) {
        std::cout << "\n[CONNECTED] " << username << " (" << address << ")" << std::endl;
    } else {
        std::cout << "\n[CONNECTED] " << address << std::endl;
    }
    std::cout << getPrompt();
    std::cout.flush();
}

void ConsoleUI::onDeviceDisconnected(const std::string& address) {
    std::cout << "\n[DISCONNECTED] " << address << std::endl;
    std::cout << getPrompt();
    std::cout.flush();
}

void ConsoleUI::onDataReceived(const std::string& address, const std::vector<uint8_t>& data) {
    try {
        auto msg = Message::deserialize(data);

        static std::unordered_set<uint32_t> seenMessages;
        static std::mutex seenMutex;

        {
            std::lock_guard<std::mutex> lock(seenMutex);
            if (seenMessages.count(msg.header.messageId)) {
                return;
            }
            seenMessages.insert(msg.header.messageId);

            if (seenMessages.size() > 1000) {
                seenMessages.clear();
            }
        }

        processReceivedMessage(msg, address);
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] Failed to parse message: " << e.what() << std::endl;
        std::cout << getPrompt();
        std::cout.flush();
    }
}

void ConsoleUI::processReceivedMessage(const Message& msg, const std::string& sourceAddress) {
    if (msg.header.type == MessageType::GLOBAL_MESSAGE ||
        msg.header.type == MessageType::TEXT_MESSAGE ||
        msg.header.type == MessageType::PRIVATE_MESSAGE) {

        auto textMsg = TextMessage::deserialize(msg.payload);
        if (textMsg.content.rfind("::FILE::", 0) == 0) {
            size_t a = textMsg.content.find("::", 8);
            size_t b = textMsg.content.find("::", a == std::string::npos ? 0 : a + 2);
            size_t c = textMsg.content.find("::", b == std::string::npos ? 0 : b + 2);
            if (a != std::string::npos && b != std::string::npos && c != std::string::npos) {
                std::string id = textMsg.content.substr(8, a - 8);
                std::string filename = textMsg.content.substr(a + 2, b - (a + 2));
                std::string ssize = textMsg.content.substr(b + 2, c - (b + 2));
                std::string b64 = textMsg.content.substr(c + 2);
                pendingFiles_[id] = {filename, b64};
                std::cout << "\n[FILE] from " << textMsg.senderUsername << ": " << filename << " bytes=" << ssize << " id=" << id << std::endl;
                std::cout << "Use /accept " << id << " or /decline " << id << std::endl;
                std::cout << getPrompt();
                std::cout.flush();
                return;
            }
        }

        if (textMsg.isGlobal && currentChatMode_ == ChatMode::GLOBAL) {
            std::string indicator = (sourceAddress == "wifi") ? " [LAN]" : "";
            displayMessage(textMsg.senderUsername + indicator, textMsg.content, false);
            std::cout << getPrompt();
            std::cout.flush();
        } else if (!textMsg.isGlobal) {
            if (currentChatMode_ == ChatMode::PERSONAL &&
                currentChatTarget_ == textMsg.senderUsername) {
                std::string indicator = (sourceAddress == "wifi") ? " [LAN]" : "";
                displayMessage(textMsg.senderUsername + indicator, textMsg.content, true);
            } else {
                std::string indicator = (sourceAddress == "wifi") ? " [LAN]" : "";
                std::cout << "\n[NEW MESSAGE from " << textMsg.senderUsername << indicator << "]: "
                         << textMsg.content << std::endl;
            }
            std::cout << getPrompt();
            std::cout.flush();
        }
    }
}

bool ConsoleUI::handleFileSend(const std::string& path, BluetoothManager& bluetoothManager, UserIdentity& identity) {
    if (currentChatMode_ != ChatMode::GLOBAL) { std::cout << "Not in global chat" << std::endl; return false; }
    std::error_code ec;
    std::filesystem::path p(path);
    if (!std::filesystem::exists(p, ec)) { std::cout << "File not found" << std::endl; return false; }
    if (!std::filesystem::is_regular_file(p, ec)) { std::cout << "Not a regular file" << std::endl; return false; }
    uintmax_t sz = std::filesystem::file_size(p, ec);
    if (ec) { std::cout << "Size error" << std::endl; return false; }
    if (sz == 0) { std::cout << "Empty file" << std::endl; return false; }
    if (sz > MAX_FILE_BYTES) { std::cout << "File too large limit=" << MAX_FILE_BYTES << std::endl; return false; }
    std::vector<uint8_t> buf(sz);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { std::cout << "Open failed" << std::endl; return false; }
    size_t r = fread(buf.data(), 1, buf.size(), f);
    fclose(f);
    if (r != buf.size()) { std::cout << "Read failed" << std::endl; return false; }
    std::string b64 = base64Encode(buf);
    std::string id = generateFileId();
    std::string fname = p.filename().string();
    auto msg = MessageFactory::createFileDataMessage(id, identity.getUsername(), identity.getFingerprint(), fname, (uint32_t)sz, b64, true);
    auto data = msg.serialize();
    bool any = false;
    if (wifi_) any = wifi_->sendBroadcast(data) || any;
    auto devices = bluetoothManager.getEchoDevices();
    for (const auto& d : devices) { any = bluetoothManager.sendData(d.address, data) || any; }
    if (!any) { std::cout << "No recipients" << std::endl; }
    return any;
}

void ConsoleUI::handleFileAccept(const std::string& id) {
    auto it = pendingFiles_.find(id);
    if (it == pendingFiles_.end()) { std::cout << "No such file id" << std::endl; return; }
    std::string filename = it->second.first;
    std::string b64 = it->second.second;
    auto data = base64Decode(b64);
    if (data.empty() || data.size() > MAX_FILE_BYTES) { std::cout << "Invalid file" << std::endl; pendingFiles_.erase(it); return; }
    std::filesystem::path dir = std::filesystem::current_path() / "FileSharing";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    for (auto& ch : filename) { if (ch == '/' || ch == '\\') ch = '_'; }
    std::filesystem::path out = dir / filename;
    FILE* f = fopen(out.string().c_str(), "wb");
    if (!f) { std::cout << "Save failed" << std::endl; return; }
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    std::cout << "Saved " << out.string() << std::endl;
    pendingFiles_.erase(id);
}

void ConsoleUI::handleFileDecline(const std::string& id) {
    auto it = pendingFiles_.find(id);
    if (it != pendingFiles_.end()) pendingFiles_.erase(it);
    std::cout << "Declined " << id << std::endl;
}

std::string ConsoleUI::base64Encode(const std::vector<uint8_t>& data) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0; size_t n = data.size();
    while (i + 2 < n) {
        uint32_t val = (data[i] << 16) | (data[i+1] << 8) | data[i+2];
        out.push_back(tbl[(val >> 18) & 63]);
        out.push_back(tbl[(val >> 12) & 63]);
        out.push_back(tbl[(val >> 6) & 63]);
        out.push_back(tbl[val & 63]);
        i += 3;
    }
    if (i < n) {
        uint32_t val = data[i] << 16;
        if (i + 1 < n) val |= (data[i+1] << 8);
        out.push_back(tbl[(val >> 18) & 63]);
        out.push_back(tbl[(val >> 12) & 63]);
        if (i + 1 < n) {
            out.push_back(tbl[(val >> 6) & 63]);
            out.push_back('=');
        } else {
            out.push_back('=');
            out.push_back('=');
        }
    }
    return out;
}

std::vector<uint8_t> ConsoleUI::base64Decode(const std::string& s) {
    auto val = [](char c) -> int { if (c >= 'A' && c <= 'Z') return c - 'A'; if (c >= 'a' && c <= 'z') return c - 'a' + 26; if (c >= '0' && c <= '9') return c - '0' + 52; if (c == '+') return 62; if (c == '/') return 63; if (c == '=') return -1; return -2; };
    std::vector<uint8_t> out; out.reserve(s.size()/4*3);
    int n = 0; uint32_t buf = 0; int pad = 0;
    for (char c : s) {
        int v = val(c);
        if (v == -2) continue;
        if (v == -1) { v = 0; pad++; }
        buf = (buf << 6) | (uint32_t)v; n += 6;
        if (n >= 24) {
            out.push_back((buf >> 16) & 0xFF);
            if (pad < 2) out.push_back((buf >> 8) & 0xFF);
            if (pad < 1) out.push_back(buf & 0xFF);
            buf = 0; n = 0; pad = 0;
        }
    }
    return out;
}

std::string ConsoleUI::generateFileId() {
    static const char* hexd = "0123456789abcdef";
    uint32_t r1 = MessageFactory::generateMessageId();
    uint32_t r2 = MessageFactory::generateMessageId();
    char buf[17];
    for (int i = 0; i < 8; ++i) buf[i] = hexd[(r1 >> ((7 - i) * 4)) & 0xF];
    for (int i = 0; i < 8; ++i) buf[8 + i] = hexd[(r2 >> ((7 - i) * 4)) & 0xF];
    return std::string(buf, buf + 16);
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

}