#pragma once

#include "MessageTypes.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <fstream>
#include <chrono>

namespace echo {

class FileTransferManager {
public:
    FileTransferManager();
    ~FileTransferManager();
    
    struct FileInfo {
        std::string filename;
        uint32_t fileSize;
        uint32_t transferId;
        uint16_t totalChunks;
        std::string senderUsername;
        std::string recipientUsername;
        std::chrono::steady_clock::time_point startTime;
        std::vector<bool> receivedChunks;
        std::vector<std::vector<uint8_t>> chunks;
    };
    
    using SendChunkCallback = std::function<bool(const Message&, const std::string& address)>;
    using ProgressCallback = std::function<void(uint32_t transferId, uint16_t received, uint16_t total)>;
    using CompletionCallback = std::function<void(uint32_t transferId, const std::string& filepath, bool success)>;
    
    void setSendChunkCallback(SendChunkCallback callback);
    void setProgressCallback(ProgressCallback callback);
    void setCompletionCallback(CompletionCallback callback);
    
    bool startFileSend(const std::string& filepath,
                      const std::string& recipientUsername,
                      const std::string& recipientAddress,
                      const std::string& senderUsername);
    
    bool processFileStartMessage(const FileStartMessage& msg, const std::string& sourceAddress);
    bool processFileChunkMessage(const FileChunkMessage& msg);
    bool processFileEndMessage(const FileEndMessage& msg);
    
    void cleanupStaleTransfers();
    
private:
    std::unordered_map<uint32_t, FileInfo> activeReceives_;
    std::unordered_map<uint32_t, FileInfo> activeSends_;
    
    mutable std::mutex transferMutex_;
    
    SendChunkCallback sendChunkCallback_;
    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;
    
    static constexpr size_t CHUNK_SIZE = 512;
    static constexpr int TRANSFER_TIMEOUT_SECONDS = 300;
    
    std::vector<uint8_t> encodeBase64(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decodeBase64(const std::vector<uint8_t>& data);
    std::vector<uint8_t> calculateChecksum(const std::vector<std::vector<uint8_t>>& chunks);
    
    bool saveReceivedFile(const FileInfo& info);
};

} // namespace echo
