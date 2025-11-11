#include "FileTransfer.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>

namespace echo {

FileTransferManager::FileTransferManager() {
}

FileTransferManager::~FileTransferManager() {
}

void FileTransferManager::setSendChunkCallback(SendChunkCallback callback) {
    sendChunkCallback_ = std::move(callback);
}

void FileTransferManager::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

void FileTransferManager::setCompletionCallback(CompletionCallback callback) {
    completionCallback_ = std::move(callback);
}

bool FileTransferManager::startFileSend(const std::string& filepath,
                                       const std::string& recipientUsername,
                                       const std::string& recipientAddress,
                                       const std::string& senderUsername) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[FileTransfer] Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    file.close();
    
    auto encodedData = encodeBase64(fileData);
    
    uint16_t totalChunks = static_cast<uint16_t>((encodedData.size() + CHUNK_SIZE - 1) / CHUNK_SIZE);
    
    size_t lastSlash = filepath.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ? filepath.substr(lastSlash + 1) : filepath;
    
    auto startMsg = MessageFactory::createFileStartMessage(
        filename,
        static_cast<uint32_t>(encodedData.size()),
        totalChunks,
        senderUsername,
        recipientUsername
    );
    
    auto startPayload = FileStartMessage::deserialize(startMsg.payload);
    uint32_t transferId = startPayload.transferId;
    
    if (!sendChunkCallback_ || !sendChunkCallback_(startMsg, recipientAddress)) {
        std::cerr << "[FileTransfer] Failed to send FILE_START" << std::endl;
        return false;
    }
    
    std::cout << "[FileTransfer] Sending file: " << filename << " (" << fileSize << " bytes)" << std::endl;
    std::cout << "[FileTransfer] Encoded size: " << encodedData.size() << " bytes, Chunks: " << totalChunks << std::endl;
    
    for (uint16_t i = 0; i < totalChunks; ++i) {
        size_t offset = i * CHUNK_SIZE;
        size_t chunkSize = std::min(CHUNK_SIZE, encodedData.size() - offset);
        
        std::vector<uint8_t> chunkData(encodedData.begin() + offset, 
                                      encodedData.begin() + offset + chunkSize);
        
        auto chunkMsg = MessageFactory::createFileChunkMessage(transferId, i, chunkData);
        
        if (!sendChunkCallback_(chunkMsg, recipientAddress)) {
            std::cerr << "[FileTransfer] Failed to send chunk " << i << std::endl;
            return false;
        }
        
        if (progressCallback_) {
            progressCallback_(transferId, i + 1, totalChunks);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    std::vector<std::vector<uint8_t>> allChunks;
    for (uint16_t i = 0; i < totalChunks; ++i) {
        size_t offset = i * CHUNK_SIZE;
        size_t chunkSize = std::min(CHUNK_SIZE, encodedData.size() - offset);
        allChunks.push_back(std::vector<uint8_t>(encodedData.begin() + offset, 
                                                encodedData.begin() + offset + chunkSize));
    }
    auto checksum = calculateChecksum(allChunks);
    
    auto endMsg = MessageFactory::createFileEndMessage(transferId, totalChunks, checksum);
    
    if (!sendChunkCallback_(endMsg, recipientAddress)) {
        std::cerr << "[FileTransfer] Failed to send FILE_END" << std::endl;
        return false;
    }
    
    std::cout << "[FileTransfer] File transfer complete: " << filename << std::endl;
    
    if (completionCallback_) {
        completionCallback_(transferId, filepath, true);
    }
    
    return true;
}

bool FileTransferManager::processFileStartMessage(const FileStartMessage& msg, const std::string& sourceAddress) {
    std::lock_guard<std::mutex> lock(transferMutex_);
    
    FileInfo info;
    info.filename = msg.filename;
    info.fileSize = msg.fileSize;
    info.transferId = msg.transferId;
    info.totalChunks = msg.totalChunks;
    info.senderUsername = msg.senderUsername;
    info.recipientUsername = msg.recipientUsername;
    info.startTime = std::chrono::steady_clock::now();
    info.receivedChunks.resize(msg.totalChunks, false);
    info.chunks.resize(msg.totalChunks);
    
    activeReceives_[msg.transferId] = info;
    
    std::cout << "[FileTransfer] Receiving file: " << msg.filename << std::endl;
    std::cout << "[FileTransfer] From: " << msg.senderUsername << " (" << sourceAddress << ")" << std::endl;
    std::cout << "[FileTransfer] Size: " << msg.fileSize << " bytes, Chunks: " << msg.totalChunks << std::endl;
    
    return true;
}

bool FileTransferManager::processFileChunkMessage(const FileChunkMessage& msg) {
    std::lock_guard<std::mutex> lock(transferMutex_);
    
    auto it = activeReceives_.find(msg.transferId);
    if (it == activeReceives_.end()) {
        std::cerr << "[FileTransfer] Unknown transfer ID: " << msg.transferId << std::endl;
        return false;
    }
    
    FileInfo& info = it->second;
    
    if (msg.chunkIndex >= info.totalChunks) {
        std::cerr << "[FileTransfer] Invalid chunk index: " << msg.chunkIndex << std::endl;
        return false;
    }
    
    info.chunks[msg.chunkIndex] = msg.data;
    info.receivedChunks[msg.chunkIndex] = true;
    
    uint16_t received = static_cast<uint16_t>(std::count(info.receivedChunks.begin(), info.receivedChunks.end(), true));
    
    if (progressCallback_) {
        progressCallback_(msg.transferId, received, info.totalChunks);
    }
    
    std::cout << "[FileTransfer] Chunk " << (received) << "/" << info.totalChunks << " received" << std::endl;
    
    return true;
}

bool FileTransferManager::processFileEndMessage(const FileEndMessage& msg) {
    std::lock_guard<std::mutex> lock(transferMutex_);
    
    auto it = activeReceives_.find(msg.transferId);
    if (it == activeReceives_.end()) {
        std::cerr << "[FileTransfer] Unknown transfer ID for END: " << msg.transferId << std::endl;
        return false;
    }
    
    FileInfo& info = it->second;
    
    uint16_t received = static_cast<uint16_t>(std::count(info.receivedChunks.begin(), info.receivedChunks.end(), true));
    if (received != info.totalChunks) {
        std::cerr << "[FileTransfer] Incomplete transfer: " << received << "/" << info.totalChunks << std::endl;
        
        if (completionCallback_) {
            completionCallback_(msg.transferId, info.filename, false);
        }
        activeReceives_.erase(it);
        return false;
    }
    
    auto calculatedChecksum = calculateChecksum(info.chunks);
    if (calculatedChecksum != msg.checksum) {
        std::cerr << "[FileTransfer] Checksum mismatch!" << std::endl;
        
        if (completionCallback_) {
            completionCallback_(msg.transferId, info.filename, false);
        }
        activeReceives_.erase(it);
        return false;
    }
    
    bool success = saveReceivedFile(info);
    
    std::cout << "[FileTransfer] Transfer complete: " << info.filename 
             << (success ? " (saved)" : " (failed)") << std::endl;
    
    if (completionCallback_) {
        completionCallback_(msg.transferId, info.filename, success);
    }
    
    activeReceives_.erase(it);
    return success;
}

void FileTransferManager::cleanupStaleTransfers() {
    std::lock_guard<std::mutex> lock(transferMutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = activeReceives_.begin(); it != activeReceives_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.startTime).count();
        if (elapsed > TRANSFER_TIMEOUT_SECONDS) {
            std::cout << "[FileTransfer] Cleaning up stale transfer: " << it->second.filename << std::endl;
            it = activeReceives_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<uint8_t> FileTransferManager::encodeBase64(const std::vector<uint8_t>& data) {
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::vector<uint8_t> encoded;
    int val = 0;
    int valb = -6;
    
    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (encoded.size() % 4) {
        encoded.push_back('=');
    }
    
    return encoded;
}

std::vector<uint8_t> FileTransferManager::decodeBase64(const std::vector<uint8_t>& data) {
    static const uint8_t base64_decode[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,62,0,0,0,63,52,53,54,55,56,57,58,59,60,61,0,0,0,0,0,0,
        0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,
        0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,0,0,0,0,0
    };
    
    std::vector<uint8_t> decoded;
    int val = 0;
    int valb = -8;
    
    for (uint8_t c : data) {
        if (c == '=') break;
        if (c > 127) continue;
        
        val = (val << 6) + base64_decode[c];
        valb += 6;
        
        if (valb >= 0) {
            decoded.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    
    return decoded;
}

std::vector<uint8_t> FileTransferManager::calculateChecksum(const std::vector<std::vector<uint8_t>>& chunks) {
    uint32_t sum = 0;
    for (const auto& chunk : chunks) {
        for (uint8_t byte : chunk) {
            sum += byte;
        }
    }
    
    std::vector<uint8_t> checksum(4);
    checksum[0] = (sum >> 24) & 0xFF;
    checksum[1] = (sum >> 16) & 0xFF;
    checksum[2] = (sum >> 8) & 0xFF;
    checksum[3] = sum & 0xFF;
    
    return checksum;
}

bool FileTransferManager::saveReceivedFile(const FileInfo& info) {
    std::vector<uint8_t> encodedData;
    for (const auto& chunk : info.chunks) {
        encodedData.insert(encodedData.end(), chunk.begin(), chunk.end());
    }
    
    auto decodedData = decodeBase64(encodedData);
    
    std::string savePath = "downloads/" + info.filename;
    
    std::ofstream file(savePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[FileTransfer] Failed to create file: " << savePath << std::endl;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(decodedData.data()), decodedData.size());
    file.close();
    
    std::cout << "[FileTransfer] File saved: " << savePath << " (" << decodedData.size() << " bytes)" << std::endl;
    
    return true;
}

} // namespace echo
