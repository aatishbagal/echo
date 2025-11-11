#include "CompressedMessage.h"
#include <cstring>
#include <algorithm>
#include <functional>

namespace echo {

std::vector<uint8_t> CompressedMessage::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(31);
    
    // Type (1 byte)
    data.push_back(static_cast<uint8_t>(type));
    
    // User ID (4 bytes, big-endian)
    data.push_back((userID >> 24) & 0xFF);
    data.push_back((userID >> 16) & 0xFF);
    data.push_back((userID >> 8) & 0xFF);
    data.push_back(userID & 0xFF);
    
    // Message ID (2 bytes, big-endian)
    data.push_back((messageID >> 8) & 0xFF);
    data.push_back(messageID & 0xFF);
    
    // Fragment count (1 byte)
    data.push_back(fragmentCount);
    
    // Fragment index (1 byte)
    data.push_back(fragmentIndex);
    
    // Payload (up to 22 bytes)
    data.insert(data.end(), payload.begin(), payload.end());
    
    return data;
}

CompressedMessage CompressedMessage::deserialize(const std::vector<uint8_t>& data) {
    CompressedMessage msg;
    
    if (data.size() < 9) {
        throw std::runtime_error("Invalid compressed message: too short");
    }
    
    size_t offset = 0;
    
    // Type
    msg.type = static_cast<Type>(data[offset++]);
    
    // User ID
    msg.userID = (static_cast<uint32_t>(data[offset]) << 24) |
                 (static_cast<uint32_t>(data[offset + 1]) << 16) |
                 (static_cast<uint32_t>(data[offset + 2]) << 8) |
                 static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Message ID
    msg.messageID = (static_cast<uint16_t>(data[offset]) << 8) |
                    static_cast<uint16_t>(data[offset + 1]);
    offset += 2;
    
    // Fragment count
    msg.fragmentCount = data[offset++];
    
    // Fragment index
    msg.fragmentIndex = data[offset++];
    
    // Payload
    msg.payload.assign(data.begin() + offset, data.end());
    
    return msg;
}

uint32_t MessageFragmenter::hashUsername(const std::string& username) {
    // Simple FNV-1a hash for 32-bit user ID
    uint32_t hash = 2166136261u;
    for (char c : username) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

std::vector<CompressedMessage> MessageFragmenter::fragment(
    const std::string& message,
    const std::string& username,
    uint16_t messageID
) {
    std::vector<CompressedMessage> fragments;
    
    if (message.length() > MAX_MESSAGE_SIZE) {
        throw std::runtime_error("Message too long (max " + 
                                std::to_string(MAX_MESSAGE_SIZE) + " bytes)");
    }
    
    uint32_t userID = hashUsername(username);
    
    // Calculate number of fragments needed
    size_t totalFragments = (message.length() + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE;
    
    if (totalFragments > 255) {
        throw std::runtime_error("Too many fragments required");
    }
    
    // Create fragments
    for (size_t i = 0; i < totalFragments; ++i) {
        CompressedMessage frag;
        frag.type = CompressedMessage::Type::TEXT_FRAGMENT;
        frag.userID = userID;
        frag.messageID = messageID;
        frag.fragmentCount = static_cast<uint8_t>(totalFragments);
        frag.fragmentIndex = static_cast<uint8_t>(i);
        
        // Extract payload for this fragment
        size_t offset = i * MAX_PAYLOAD_SIZE;
        size_t length = std::min(MAX_PAYLOAD_SIZE, message.length() - offset);
        
        frag.payload.assign(
            message.begin() + offset,
            message.begin() + offset + length
        );
        
        fragments.push_back(frag);
    }
    
    return fragments;
}

std::string MessageFragmenter::reassemble(const std::vector<CompressedMessage>& fragments) {
    if (fragments.empty()) {
        return "";
    }
    
    // Sort fragments by index
    std::vector<CompressedMessage> sorted = fragments;
    std::sort(sorted.begin(), sorted.end(),
        [](const CompressedMessage& a, const CompressedMessage& b) {
            return a.fragmentIndex < b.fragmentIndex;
        });
    
    // Verify we have all fragments
    uint8_t expectedCount = sorted[0].fragmentCount;
    if (sorted.size() != expectedCount) {
        throw std::runtime_error("Missing fragments");
    }
    
    // Reassemble message
    std::string message;
    for (const auto& frag : sorted) {
        message.append(frag.payload.begin(), frag.payload.end());
    }
    
    return message;
}

}
