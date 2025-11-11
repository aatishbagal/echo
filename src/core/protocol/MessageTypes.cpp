#include "MessageTypes.h"
#include <cstring>
#include <stdexcept>
#include <random>
#include <atomic>

namespace echo {

std::atomic<uint32_t> MessageFactory::messageIdCounter_(0);
std::atomic<uint32_t> MessageFactory::transferIdCounter_(0);

std::vector<uint8_t> MessageHeader::serialize() const {
    std::vector<uint8_t> data(SIZE);
    data[0] = static_cast<uint8_t>(type);
    data[1] = version;
    data[2] = (length >> 8) & 0xFF;
    data[3] = length & 0xFF;
    data[4] = (messageId >> 24) & 0xFF;
    data[5] = (messageId >> 16) & 0xFF;
    data[6] = (messageId >> 8) & 0xFF;
    data[7] = messageId & 0xFF;
    data[8] = (timestamp >> 24) & 0xFF;
    data[9] = (timestamp >> 16) & 0xFF;
    data[10] = (timestamp >> 8) & 0xFF;
    data[11] = timestamp & 0xFF;
    data[12] = ttl;
    return data;
}

MessageHeader MessageHeader::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < SIZE) {
        throw std::runtime_error("Invalid message header size");
    }
    
    MessageHeader header;
    header.type = static_cast<MessageType>(data[0]);
    header.version = data[1];
    header.length = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    header.messageId = (static_cast<uint32_t>(data[4]) << 24) |
                      (static_cast<uint32_t>(data[5]) << 16) |
                      (static_cast<uint32_t>(data[6]) << 8) |
                      data[7];
    header.timestamp = (static_cast<uint32_t>(data[8]) << 24) |
                      (static_cast<uint32_t>(data[9]) << 16) |
                      (static_cast<uint32_t>(data[10]) << 8) |
                      data[11];
    header.ttl = data[12];
    return header;
}

std::vector<uint8_t> TextMessage::serialize() const {
    std::vector<uint8_t> data;
    
    auto appendString = [&data](const std::string& str) {
        uint16_t len = static_cast<uint16_t>(str.length());
        data.push_back((len >> 8) & 0xFF);
        data.push_back(len & 0xFF);
        data.insert(data.end(), str.begin(), str.end());
    };
    
    appendString(senderUsername);
    appendString(senderFingerprint);
    appendString(recipientUsername);
    appendString(content);
    
    auto time = std::chrono::system_clock::to_time_t(timestamp);
    uint32_t timeValue = static_cast<uint32_t>(time);
    data.push_back((timeValue >> 24) & 0xFF);
    data.push_back((timeValue >> 16) & 0xFF);
    data.push_back((timeValue >> 8) & 0xFF);
    data.push_back(timeValue & 0xFF);
    
    data.push_back(isGlobal ? 1 : 0);
    
    return data;
}

TextMessage TextMessage::deserialize(const std::vector<uint8_t>& data) {
    TextMessage msg;
    size_t offset = 0;
    
    auto readString = [&data, &offset]() -> std::string {
        if (offset + 2 > data.size()) {
            throw std::runtime_error("Invalid message data");
        }
        uint16_t len = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        
        if (offset + len > data.size()) {
            throw std::runtime_error("Invalid string length");
        }
        std::string str(data.begin() + offset, data.begin() + offset + len);
        offset += len;
        return str;
    };
    
    msg.senderUsername = readString();
    msg.senderFingerprint = readString();
    msg.recipientUsername = readString();
    msg.content = readString();
    
    if (offset + 5 > data.size()) {
        throw std::runtime_error("Invalid message data");
    }
    
    uint32_t timeValue = (static_cast<uint32_t>(data[offset]) << 24) |
                        (static_cast<uint32_t>(data[offset + 1]) << 16) |
                        (static_cast<uint32_t>(data[offset + 2]) << 8) |
                        data[offset + 3];
    offset += 4;
    
    msg.timestamp = std::chrono::system_clock::from_time_t(timeValue);
    msg.isGlobal = data[offset] != 0;
    
    return msg;
}

std::vector<uint8_t> AnnounceMessage::serialize() const {
    std::vector<uint8_t> data;
    
    auto appendString = [&data](const std::string& str) {
        uint16_t len = static_cast<uint16_t>(str.length());
        data.push_back((len >> 8) & 0xFF);
        data.push_back(len & 0xFF);
        data.insert(data.end(), str.begin(), str.end());
    };
    
    appendString(username);
    appendString(fingerprint);
    appendString(osType);
    
    data.push_back((protocolVersion >> 8) & 0xFF);
    data.push_back(protocolVersion & 0xFF);
    
    return data;
}

AnnounceMessage AnnounceMessage::deserialize(const std::vector<uint8_t>& data) {
    AnnounceMessage msg;
    size_t offset = 0;
    
    auto readString = [&data, &offset]() -> std::string {
        if (offset + 2 > data.size()) {
            throw std::runtime_error("Invalid message data");
        }
        uint16_t len = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        
        if (offset + len > data.size()) {
            throw std::runtime_error("Invalid string length");
        }
        std::string str(data.begin() + offset, data.begin() + offset + len);
        offset += len;
        return str;
    };
    
    msg.username = readString();
    msg.fingerprint = readString();
    msg.osType = readString();
    
    if (offset + 2 > data.size()) {
        throw std::runtime_error("Invalid message data");
    }
    
    msg.protocolVersion = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    
    return msg;
}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> data;
    
    auto headerData = header.serialize();
    data.insert(data.end(), headerData.begin(), headerData.end());
    
    data.insert(data.end(), payload.begin(), payload.end());
    
    return data;
}

Message Message::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < MessageHeader::SIZE) {
        throw std::runtime_error("Message too small");
    }
    
    Message msg;
    
    std::vector<uint8_t> headerData(data.begin(), data.begin() + MessageHeader::SIZE);
    msg.header = MessageHeader::deserialize(headerData);
    
    if (data.size() > MessageHeader::SIZE) {
        msg.payload = std::vector<uint8_t>(data.begin() + MessageHeader::SIZE, data.end());
    }
    
    return msg;
}

Message MessageFactory::createTextMessage(const std::string& content,
                                         const std::string& senderUsername,
                                         const std::string& senderFingerprint,
                                         const std::string& recipientUsername,
                                         bool isGlobal) {
    TextMessage textMsg;
    textMsg.senderUsername = senderUsername;
    textMsg.senderFingerprint = senderFingerprint;
    textMsg.recipientUsername = recipientUsername;
    textMsg.content = content;
    textMsg.timestamp = std::chrono::system_clock::now();
    textMsg.isGlobal = isGlobal;
    
    Message msg;
    msg.header.type = isGlobal ? MessageType::GLOBAL_MESSAGE : MessageType::PRIVATE_MESSAGE;
    msg.header.version = 1;
    msg.header.messageId = generateMessageId();
    msg.header.timestamp = static_cast<uint32_t>(std::chrono::system_clock::to_time_t(textMsg.timestamp));
    msg.header.ttl = 7;
    
    msg.payload = textMsg.serialize();
    msg.header.length = static_cast<uint16_t>(msg.payload.size());
    
    return msg;
}

Message MessageFactory::createAnnounceMessage(const std::string& username,
                                             const std::string& fingerprint,
                                             const std::string& osType) {
    AnnounceMessage announceMsg;
    announceMsg.username = username;
    announceMsg.fingerprint = fingerprint;
    announceMsg.osType = osType;
    announceMsg.protocolVersion = 1;
    
    Message msg;
    msg.header.type = MessageType::ANNOUNCE;
    msg.header.version = 1;
    msg.header.messageId = generateMessageId();
    msg.header.timestamp = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    msg.header.ttl = 7;
    
    msg.payload = announceMsg.serialize();
    msg.header.length = static_cast<uint16_t>(msg.payload.size());
    
    return msg;
}

Message MessageFactory::createPingMessage() {
    Message msg;
    msg.header.type = MessageType::PING;
    msg.header.version = 1;
    msg.header.messageId = generateMessageId();
    msg.header.timestamp = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    msg.header.ttl = 7;
    msg.header.length = 0;
    
    return msg;
}

Message MessageFactory::createPongMessage() {
    Message msg;
    msg.header.type = MessageType::PONG;
    msg.header.version = 1;
    msg.header.messageId = generateMessageId();
    msg.header.timestamp = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    msg.header.ttl = 7;
    msg.header.length = 0;
    
    return msg;
}

uint32_t MessageFactory::generateMessageId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;
    
    return dis(gen);
}

uint32_t MessageFactory::generateTransferId() {
    return ++transferIdCounter_;
}

std::vector<uint8_t> FileStartMessage::serialize() const {
    std::vector<uint8_t> data;
    
    auto appendString = [&data](const std::string& str) {
        uint16_t len = static_cast<uint16_t>(str.length());
        data.push_back((len >> 8) & 0xFF);
        data.push_back(len & 0xFF);
        data.insert(data.end(), str.begin(), str.end());
    };
    
    appendString(filename);
    
    data.push_back((fileSize >> 24) & 0xFF);
    data.push_back((fileSize >> 16) & 0xFF);
    data.push_back((fileSize >> 8) & 0xFF);
    data.push_back(fileSize & 0xFF);
    
    data.push_back((transferId >> 24) & 0xFF);
    data.push_back((transferId >> 16) & 0xFF);
    data.push_back((transferId >> 8) & 0xFF);
    data.push_back(transferId & 0xFF);
    
    data.push_back((totalChunks >> 8) & 0xFF);
    data.push_back(totalChunks & 0xFF);
    
    appendString(senderUsername);
    appendString(recipientUsername);
    
    return data;
}

FileStartMessage FileStartMessage::deserialize(const std::vector<uint8_t>& data) {
    FileStartMessage msg;
    size_t offset = 0;
    
    auto readString = [&data, &offset]() -> std::string {
        if (offset + 2 > data.size()) {
            throw std::runtime_error("Invalid data");
        }
        uint16_t len = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        
        if (offset + len > data.size()) {
            throw std::runtime_error("Invalid string length");
        }
        std::string str(data.begin() + offset, data.begin() + offset + len);
        offset += len;
        return str;
    };
    
    msg.filename = readString();
    
    if (offset + 10 > data.size()) {
        throw std::runtime_error("Invalid data");
    }
    
    msg.fileSize = (static_cast<uint32_t>(data[offset]) << 24) |
                   (static_cast<uint32_t>(data[offset + 1]) << 16) |
                   (static_cast<uint32_t>(data[offset + 2]) << 8) |
                   data[offset + 3];
    offset += 4;
    
    msg.transferId = (static_cast<uint32_t>(data[offset]) << 24) |
                     (static_cast<uint32_t>(data[offset + 1]) << 16) |
                     (static_cast<uint32_t>(data[offset + 2]) << 8) |
                     data[offset + 3];
    offset += 4;
    
    msg.totalChunks = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    
    msg.senderUsername = readString();
    msg.recipientUsername = readString();
    
    return msg;
}

std::vector<uint8_t> FileChunkMessage::serialize() const {
    std::vector<uint8_t> result;
    
    result.push_back((transferId >> 24) & 0xFF);
    result.push_back((transferId >> 16) & 0xFF);
    result.push_back((transferId >> 8) & 0xFF);
    result.push_back(transferId & 0xFF);
    
    result.push_back((chunkIndex >> 8) & 0xFF);
    result.push_back(chunkIndex & 0xFF);
    
    uint16_t dataLen = static_cast<uint16_t>(data.size());
    result.push_back((dataLen >> 8) & 0xFF);
    result.push_back(dataLen & 0xFF);
    
    result.insert(result.end(), data.begin(), data.end());
    
    return result;
}

FileChunkMessage FileChunkMessage::deserialize(const std::vector<uint8_t>& data) {
    FileChunkMessage msg;
    
    if (data.size() < 8) {
        throw std::runtime_error("Invalid chunk data");
    }
    
    size_t offset = 0;
    
    msg.transferId = (static_cast<uint32_t>(data[offset]) << 24) |
                     (static_cast<uint32_t>(data[offset + 1]) << 16) |
                     (static_cast<uint32_t>(data[offset + 2]) << 8) |
                     data[offset + 3];
    offset += 4;
    
    msg.chunkIndex = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    
    uint16_t dataLen = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    
    if (offset + dataLen > data.size()) {
        throw std::runtime_error("Invalid chunk data length");
    }
    
    msg.data = std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + dataLen);
    
    return msg;
}

std::vector<uint8_t> FileEndMessage::serialize() const {
    std::vector<uint8_t> result;
    
    result.push_back((transferId >> 24) & 0xFF);
    result.push_back((transferId >> 16) & 0xFF);
    result.push_back((transferId >> 8) & 0xFF);
    result.push_back(transferId & 0xFF);
    
    result.push_back((totalChunks >> 8) & 0xFF);
    result.push_back(totalChunks & 0xFF);
    
    uint16_t checksumLen = static_cast<uint16_t>(checksum.size());
    result.push_back((checksumLen >> 8) & 0xFF);
    result.push_back(checksumLen & 0xFF);
    
    result.insert(result.end(), checksum.begin(), checksum.end());
    
    return result;
}

FileEndMessage FileEndMessage::deserialize(const std::vector<uint8_t>& data) {
    FileEndMessage msg;
    
    if (data.size() < 8) {
        throw std::runtime_error("Invalid end message data");
    }
    
    size_t offset = 0;
    
    msg.transferId = (static_cast<uint32_t>(data[offset]) << 24) |
                     (static_cast<uint32_t>(data[offset + 1]) << 16) |
                     (static_cast<uint32_t>(data[offset + 2]) << 8) |
                     data[offset + 3];
    offset += 4;
    
    msg.totalChunks = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    
    uint16_t checksumLen = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    
    if (offset + checksumLen > data.size()) {
        throw std::runtime_error("Invalid checksum length");
    }
    
    msg.checksum = std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + checksumLen);
    
    return msg;
}

Message MessageFactory::createFileStartMessage(const std::string& filename,
                                              uint32_t fileSize,
                                              uint16_t totalChunks,
                                              const std::string& senderUsername,
                                              const std::string& recipientUsername) {
    FileStartMessage fileMsg;
    fileMsg.filename = filename;
    fileMsg.fileSize = fileSize;
    fileMsg.transferId = generateTransferId();
    fileMsg.totalChunks = totalChunks;
    fileMsg.senderUsername = senderUsername;
    fileMsg.recipientUsername = recipientUsername;
    
    Message msg;
    msg.header.type = MessageType::FILE_START;
    msg.header.version = 1;
    msg.header.messageId = generateMessageId();
    msg.header.timestamp = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    msg.header.ttl = 7;
    
    msg.payload = fileMsg.serialize();
    msg.header.length = static_cast<uint16_t>(msg.payload.size());
    
    return msg;
}

Message MessageFactory::createFileChunkMessage(uint32_t transferId,
                                              uint16_t chunkIndex,
                                              const std::vector<uint8_t>& data) {
    FileChunkMessage chunkMsg;
    chunkMsg.transferId = transferId;
    chunkMsg.chunkIndex = chunkIndex;
    chunkMsg.data = data;
    
    Message msg;
    msg.header.type = MessageType::FILE_CHUNK;
    msg.header.version = 1;
    msg.header.messageId = generateMessageId();
    msg.header.timestamp = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    msg.header.ttl = 3;
    
    msg.payload = chunkMsg.serialize();
    msg.header.length = static_cast<uint16_t>(msg.payload.size());
    
    return msg;
}

Message MessageFactory::createFileEndMessage(uint32_t transferId,
                                            uint16_t totalChunks,
                                            const std::vector<uint8_t>& checksum) {
    FileEndMessage endMsg;
    endMsg.transferId = transferId;
    endMsg.totalChunks = totalChunks;
    endMsg.checksum = checksum;
    
    Message msg;
    msg.header.type = MessageType::FILE_END;
    msg.header.version = 1;
    msg.header.messageId = generateMessageId();
    msg.header.timestamp = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    msg.header.ttl = 7;
    
    msg.payload = endMsg.serialize();
    msg.header.length = static_cast<uint16_t>(msg.payload.size());
    
    return msg;
}

} // namespace echo