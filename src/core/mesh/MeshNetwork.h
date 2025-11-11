#pragma once

#include "../protocol/MessageTypes.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <chrono>

namespace echo {

class MeshNetwork {
public:
    MeshNetwork();
    ~MeshNetwork();
    
    void setLocalUsername(const std::string& username);
    void setLocalFingerprint(const std::string& fingerprint);
    
    bool processIncomingMessage(const Message& msg, const std::string& sourceAddress);
    
    Message prepareMessageForRouting(const Message& msg);
    
    bool shouldForwardMessage(const Message& msg) const;
    
    void addPeer(const std::string& address, const std::string& username);
    void removePeer(const std::string& address);
    std::vector<std::string> getActivePeers() const;
    
    using MessageCallback = std::function<void(const Message&, const std::string& sourceAddress)>;
    void setMessageCallback(MessageCallback callback);
    
    using ForwardCallback = std::function<void(const Message&, const std::vector<std::string>& excludeAddresses)>;
    void setForwardCallback(ForwardCallback callback);
    
    void cleanupOldMessages();
    
private:
    std::string localUsername_;
    std::string localFingerprint_;
    
    struct SeenMessage {
        uint32_t messageId;
        std::chrono::steady_clock::time_point seenAt;
        std::string originalSource;
    };
    
    std::unordered_set<uint32_t> seenMessageIds_;
    std::vector<SeenMessage> seenMessages_;
    
    struct PeerInfo {
        std::string address;
        std::string username;
        std::chrono::steady_clock::time_point lastSeen;
    };
    
    std::unordered_map<std::string, PeerInfo> peers_;
    
    mutable std::mutex meshMutex_;
    
    MessageCallback messageCallback_;
    ForwardCallback forwardCallback_;
    
    bool isMessageSeen(uint32_t messageId) const;
    void markMessageAsSeen(uint32_t messageId, const std::string& source);
    
    static constexpr size_t MAX_SEEN_MESSAGES = 1000;
    static constexpr int MESSAGE_TIMEOUT_SECONDS = 300;
};

} // namespace echo
