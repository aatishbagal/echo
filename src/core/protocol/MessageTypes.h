#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>

namespace echo {

enum class MessageType : uint8_t {
    DISCOVER = 0x01,
    ANNOUNCE = 0x02,
    TEXT_MESSAGE = 0x03,
    GLOBAL_MESSAGE = 0x04,
    ACK = 0x05,
    PING = 0x06,
    PONG = 0x07,
    FILE_REQUEST = 0x08,
    FILE_DATA = 0x09,
    USER_STATUS = 0x0A,
    CHANNEL_JOIN = 0x0B,
    CHANNEL_LEAVE = 0x0C,
    PRIVATE_MESSAGE = 0x0D
};

enum class ChatMode {
    NONE,
    GLOBAL,
    PERSONAL
};

struct MessageHeader {
    MessageType type;
    uint8_t version = 1;
    uint16_t length;
    uint32_t messageId;
    uint32_t timestamp;
    uint8_t ttl = 7;
    
    static constexpr size_t SIZE = 13;
    
    std::vector<uint8_t> serialize() const;
    static MessageHeader deserialize(const std::vector<uint8_t>& data);
};

struct TextMessage {
    std::string senderUsername;
    std::string senderFingerprint;
    std::string recipientUsername;
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    bool isGlobal = false;
    
    std::vector<uint8_t> serialize() const;
    static TextMessage deserialize(const std::vector<uint8_t>& data);
};

struct AnnounceMessage {
    std::string username;
    std::string fingerprint;
    std::string osType;
    uint16_t protocolVersion = 1;
    
    std::vector<uint8_t> serialize() const;
    static AnnounceMessage deserialize(const std::vector<uint8_t>& data);
};

struct Message {
    MessageHeader header;
    std::vector<uint8_t> payload;
    std::string sourceAddress;
    int16_t rssi = 0;
    std::chrono::steady_clock::time_point receivedAt;
    
    std::vector<uint8_t> serialize() const;
    static Message deserialize(const std::vector<uint8_t>& data);
};

class MessageFactory {
public:
    static Message createTextMessage(const std::string& content, 
                                    const std::string& senderUsername,
                                    const std::string& senderFingerprint,
                                    const std::string& recipientUsername = "",
                                    bool isGlobal = false);
    
    static Message createAnnounceMessage(const std::string& username,
                                        const std::string& fingerprint,
                                        const std::string& osType);
    
    static Message createPingMessage();
    static Message createPongMessage();
    
    static uint32_t generateMessageId();
    
private:
    static std::atomic<uint32_t> messageIdCounter_;
};

} // namespace echo