#include "MeshNetwork.h"
#include <algorithm>
#include <iostream>

namespace echo {

MeshNetwork::MeshNetwork() {
}

MeshNetwork::~MeshNetwork() {
}

void MeshNetwork::setLocalUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(meshMutex_);
    localUsername_ = username;
}

void MeshNetwork::setLocalFingerprint(const std::string& fingerprint) {
    std::lock_guard<std::mutex> lock(meshMutex_);
    localFingerprint_ = fingerprint;
}

bool MeshNetwork::processIncomingMessage(const Message& msg, const std::string& sourceAddress) {
    std::lock_guard<std::mutex> lock(meshMutex_);
    
    if (isMessageSeen(msg.header.messageId)) {
        return false;
    }
    
    markMessageAsSeen(msg.header.messageId, sourceAddress);
    
    if (msg.header.ttl == 0) {
        return false;
    }
    
    if (messageCallback_) {
        messageCallback_(msg, sourceAddress);
    }
    
    if (shouldForwardMessage(msg) && msg.header.ttl > 1) {
        Message forwardMsg = msg;
        forwardMsg.header.ttl--;
        
        if (forwardCallback_) {
            std::vector<std::string> exclude = {sourceAddress};
            forwardCallback_(forwardMsg, exclude);
        }
    }
    
    return true;
}

Message MeshNetwork::prepareMessageForRouting(const Message& msg) {
    Message routedMsg = msg;
    
    if (routedMsg.header.ttl == 0) {
        routedMsg.header.ttl = 7;
    }
    
    return routedMsg;
}

bool MeshNetwork::shouldForwardMessage(const Message& msg) const {
    switch (msg.header.type) {
        case MessageType::GLOBAL_MESSAGE:
        case MessageType::ANNOUNCE:
        case MessageType::DISCOVER:
            return true;
            
        case MessageType::PRIVATE_MESSAGE:
        case MessageType::TEXT_MESSAGE:
            return false;
            
        case MessageType::FILE_START:
        case MessageType::FILE_CHUNK:
        case MessageType::FILE_END:
            return false;
            
        default:
            return false;
    }
}

void MeshNetwork::addPeer(const std::string& address, const std::string& username) {
    std::lock_guard<std::mutex> lock(meshMutex_);
    
    PeerInfo info;
    info.address = address;
    info.username = username;
    info.lastSeen = std::chrono::steady_clock::now();
    
    peers_[address] = info;
    
    std::cout << "[Mesh] Added peer: " << username << " (" << address << ")" << std::endl;
}

void MeshNetwork::removePeer(const std::string& address) {
    std::lock_guard<std::mutex> lock(meshMutex_);
    
    auto it = peers_.find(address);
    if (it != peers_.end()) {
        std::cout << "[Mesh] Removed peer: " << it->second.username << " (" << address << ")" << std::endl;
        peers_.erase(it);
    }
}

std::vector<std::string> MeshNetwork::getActivePeers() const {
    std::lock_guard<std::mutex> lock(meshMutex_);
    
    std::vector<std::string> activePeers;
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& [address, info] : peers_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - info.lastSeen).count();
        if (elapsed < 60) {
            activePeers.push_back(address);
        }
    }
    
    return activePeers;
}

void MeshNetwork::setMessageCallback(MessageCallback callback) {
    messageCallback_ = std::move(callback);
}

void MeshNetwork::setForwardCallback(ForwardCallback callback) {
    forwardCallback_ = std::move(callback);
}

void MeshNetwork::cleanupOldMessages() {
    std::lock_guard<std::mutex> lock(meshMutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    seenMessages_.erase(
        std::remove_if(seenMessages_.begin(), seenMessages_.end(),
            [now](const SeenMessage& seen) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - seen.seenAt).count();
                return elapsed > MESSAGE_TIMEOUT_SECONDS;
            }),
        seenMessages_.end()
    );
    
    seenMessageIds_.clear();
    for (const auto& seen : seenMessages_) {
        seenMessageIds_.insert(seen.messageId);
    }
    
    if (seenMessages_.size() > MAX_SEEN_MESSAGES) {
        size_t toRemove = seenMessages_.size() - MAX_SEEN_MESSAGES;
        seenMessages_.erase(seenMessages_.begin(), seenMessages_.begin() + toRemove);
    }
}

bool MeshNetwork::isMessageSeen(uint32_t messageId) const {
    return seenMessageIds_.find(messageId) != seenMessageIds_.end();
}

void MeshNetwork::markMessageAsSeen(uint32_t messageId, const std::string& source) {
    SeenMessage seen;
    seen.messageId = messageId;
    seen.seenAt = std::chrono::steady_clock::now();
    seen.originalSource = source;
    
    seenMessages_.push_back(seen);
    seenMessageIds_.insert(messageId);
}

} // namespace echo
