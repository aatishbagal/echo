#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

namespace echo {

class WifiDirect {
public:
    WifiDirect();
    ~WifiDirect();
    bool start(const std::string& username, const std::string& fingerprint, uint16_t tcpPort = 48271);
    void stop();
    void setOnData(std::function<void(const std::string&, const std::vector<uint8_t>&)> cb);
    bool sendTo(const std::string& username, const std::vector<uint8_t>& data);
    bool sendBroadcast(const std::vector<uint8_t>& data);
    std::vector<std::pair<std::string,std::string>> listPeers();
    void setVerbose(bool enabled) { verbose_ = enabled; }

private:
    struct Peer { std::string ip; uint16_t port; std::chrono::steady_clock::time_point lastSeen; };
    std::unordered_map<std::string, Peer> peers_;
    std::mutex mtx_;
    std::string username_;
    std::string fingerprint_;
    uint16_t tcpPort_ = 48271;
    std::function<void(const std::string&, const std::vector<uint8_t>&)> onData_;
    std::atomic<bool> running_{false};
    std::atomic<bool> verbose_{false};
    std::thread udpTxThread_;
    std::thread udpRxThread_;
    std::thread tcpServerThread_;

    void runUdpTx();
    void runUdpRx();
    void runTcpServer();
    bool sendTcp(const std::string& ip, uint16_t port, const std::vector<uint8_t>& data);
};

}
