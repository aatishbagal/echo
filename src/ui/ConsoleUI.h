#pragma once

#include "core/bluetooth/BluetoothManager.h"
#include "core/protocol/MessageTypes.h"
#include "core/commands/IRCParser.h"
#include <string>
#include <deque>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <filesystem>

namespace echo {

class UserIdentity;
class WifiDirect;

class ConsoleUI {
public:
    ConsoleUI();
    ~ConsoleUI();
    
    void run(BluetoothManager& bluetoothManager, UserIdentity& identity);
    
private:
    std::atomic<bool> running_;
    IRCParser commandParser_;
    ChatMode currentChatMode_;
    std::string currentChatTarget_;
    std::unique_ptr<WifiDirect> wifi_;
    
    std::deque<std::string> messageHistory_;
    std::mutex historyMutex_;
    static constexpr size_t MAX_HISTORY = 100;
    
    void printHelp() const;
    void printChatHelp() const;
    void handleCommand(const std::string& command, BluetoothManager& bluetoothManager, UserIdentity& identity);
    void handleChatMode(const std::string& input, BluetoothManager& bluetoothManager, UserIdentity& identity);
    
    void enterPersonalChat(const std::string& username, BluetoothManager& bluetoothManager);
    void enterGlobalChat(BluetoothManager& bluetoothManager);
    void exitChatMode();
    
    void sendMessage(const std::string& message, BluetoothManager& bluetoothManager, UserIdentity& identity);
    void displayMessage(const std::string& from, const std::string& message, bool isPrivate);
    
    void addToHistory(const std::string& message);
    void clearScreen() const;
    std::string getPrompt() const;
    
    void printDevices(const BluetoothManager& bluetoothManager) const;
    void printEchoDevices(const BluetoothManager& bluetoothManager) const;
    
    void onDeviceDiscovered(const DiscoveredDevice& device);
    void onDeviceConnected(const std::string& address);
    void onDeviceDisconnected(const std::string& address);
    void onDataReceived(const std::string& address, const std::vector<uint8_t>& data);
    
    void processReceivedMessage(const Message& msg, const std::string& sourceAddress);
    
    std::string findUsernameByAddress(const std::string& address, const BluetoothManager& bluetoothManager) const;
    std::string findAddressByUsername(const std::string& username, const BluetoothManager& bluetoothManager) const;
    bool connectByTarget(const std::string& target, BluetoothManager& bluetoothManager);

    bool handleFileSend(const std::string& path, BluetoothManager& bluetoothManager, UserIdentity& identity);
    void handleFileAccept(const std::string& id);
    void handleFileDecline(const std::string& id);
    std::string base64Encode(const std::vector<uint8_t>& data);
    std::vector<uint8_t> base64Decode(const std::string& encoded);
    std::string generateFileId();
    std::unordered_map<std::string, std::pair<std::string,std::string>> pendingFiles_;
    static constexpr size_t MAX_FILE_BYTES = 32768;
};

} // namespace echo