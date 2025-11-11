#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace echo {

// Compressed message format for BLE advertising (max 31 bytes)
// Format: [Type:1][UserID:4][MsgID:2][FragCount:1][FragIndex:1][Payload:22]
// Total: 31 bytes
struct CompressedMessage {
    enum class Type : uint8_t {
        TEXT_FRAGMENT = 0x01,
        ANNOUNCEMENT = 0x02,
        ACK = 0x03
    };
    
    Type type;
    uint32_t userID;           // 4-byte hash of username
    uint16_t messageID;        // Rolling message counter
    uint8_t fragmentCount;     // Total fragments (1-255)
    uint8_t fragmentIndex;     // Current fragment (0-254)
    std::vector<uint8_t> payload;  // Max 22 bytes of actual data
    
    std::vector<uint8_t> serialize() const;
    static CompressedMessage deserialize(const std::vector<uint8_t>& data);
};

// Helper class to fragment long messages into BLE-sized chunks
class MessageFragmenter {
public:
    // Fragment a message into multiple BLE advertisements
    static std::vector<CompressedMessage> fragment(
        const std::string& message,
        const std::string& username,
        uint16_t messageID
    );
    
    // Reassemble fragments back into original message
    static std::string reassemble(const std::vector<CompressedMessage>& fragments);
    
    // Generate 4-byte user ID hash from username
    static uint32_t hashUsername(const std::string& username);
    
    // Maximum payload size per fragment (31 - header overhead)
    static constexpr size_t MAX_PAYLOAD_SIZE = 22;
    
    // Maximum message length before fragmentation (22 * 255 fragments)
    static constexpr size_t MAX_MESSAGE_SIZE = 5610;
};

}
