#include "WifiDirect.h"
#include <chrono>
#include <cstring>
#include <algorithm>

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace echo {

WifiDirect::WifiDirect() {}
WifiDirect::~WifiDirect() { stop(); }

bool WifiDirect::start(const std::string& username, const std::string& fingerprint, uint16_t tcpPort) {
    username_ = username;
    fingerprint_ = fingerprint;
    tcpPort_ = tcpPort;
    if (running_) return true;
    running_ = true;
    udpTxThread_ = std::thread([this]() { runUdpTx(); });
    udpRxThread_ = std::thread([this]() { runUdpRx(); });
    tcpServerThread_ = std::thread([this]() { runTcpServer(); });
    return true;
}

void WifiDirect::stop() {
    if (!running_) return;
    running_ = false;
    try { if (udpTxThread_.joinable()) udpTxThread_.join(); } catch (...) {}
    try { if (udpRxThread_.joinable()) udpRxThread_.join(); } catch (...) {}
    try { if (tcpServerThread_.joinable()) tcpServerThread_.join(); } catch (...) {}
}

void WifiDirect::setOnData(std::function<void(const std::string&, const std::vector<uint8_t>&)> cb) { onData_ = std::move(cb); }

bool WifiDirect::sendTo(const std::string& username, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = peers_.find(username);
    if (it == peers_.end()) return false;
    return sendTcp(it->second.ip, it->second.port, data);
}

bool WifiDirect::sendBroadcast(const std::vector<uint8_t>& data) {
    std::vector<std::pair<std::string,uint16_t>> targets;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto& kv : peers_) targets.emplace_back(kv.second.ip, kv.second.port);
    }
    bool any = false;
    for (auto& t : targets) any |= sendTcp(t.first, t.second, data);
    return any;
}

void WifiDirect::runUdpTx() {
#ifdef __linux__
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { while (running_) std::this_thread::sleep_for(std::chrono::seconds(1)); return; }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(48270); addr.sin_addr.s_addr = INADDR_BROADCAST;
    while (running_) {
        std::string u = username_;
        std::string f = fingerprint_;
        uint16_t port = tcpPort_;
        std::vector<uint8_t> buf;
        buf.push_back(1);
        buf.push_back((uint8_t)u.size());
        buf.insert(buf.end(), u.begin(), u.end());
        buf.push_back((uint8_t)f.size());
        buf.insert(buf.end(), f.begin(), f.end());
        buf.push_back((uint8_t)(port >> 8));
        buf.push_back((uint8_t)(port & 0xFF));
        sendto(s, buf.data(), buf.size(), 0, (sockaddr*)&addr, sizeof(addr));
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    close(s);
#else
    while (running_) std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
}

void WifiDirect::runUdpRx() {
#ifdef __linux__
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { while (running_) std::this_thread::sleep_for(std::chrono::seconds(1)); return; }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(48270); addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) { close(s); while (running_) std::this_thread::sleep_for(std::chrono::seconds(1)); return; }
    std::vector<uint8_t> buf(512);
    while (running_) {
        sockaddr_in src{}; socklen_t sl = sizeof(src);
        ssize_t n = recvfrom(s, buf.data(), buf.size(), 0, (sockaddr*)&src, &sl);
        if (n <= 0) continue;
        if (buf[0] != 1) continue;
        size_t i = 1;
        if (i >= (size_t)n) continue;
        uint8_t ulen = buf[i++];
        if (i + ulen > (size_t)n) continue;
        std::string u((char*)&buf[i], (char*)&buf[i+ulen]); i += ulen;
        if (i >= (size_t)n) continue;
        uint8_t flen = buf[i++];
        if (i + flen + 2 > (size_t)n) continue;
        std::string f((char*)&buf[i], (char*)&buf[i+flen]); i += flen;
        uint16_t port = ((uint16_t)buf[i] << 8) | buf[i+1];
        std::string ip = inet_ntoa(src.sin_addr);
        if (u == username_) continue;
        Peer p; p.ip = ip; p.port = port; p.lastSeen = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(mtx_);
            peers_[u] = p;
        }
    }
    close(s);
#else
    while (running_) std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
}

void WifiDirect::runTcpServer() {
#ifdef __linux__
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { while (running_) std::this_thread::sleep_for(std::chrono::seconds(1)); return; }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(tcpPort_); addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) { close(s); while (running_) std::this_thread::sleep_for(std::chrono::seconds(1)); return; }
    listen(s, 4);
    while (running_) {
        sockaddr_in caddr{}; socklen_t cl = sizeof(caddr);
        int c = accept(s, (sockaddr*)&caddr, &cl);
        if (c < 0) continue;
        std::thread([this,c]() {
            std::vector<uint8_t> lenbuf(4);
            while (true) {
                ssize_t r = recv(c, lenbuf.data(), 4, MSG_WAITALL);
                if (r != 4) break;
                uint32_t len = ((uint32_t)lenbuf[0] << 24) | ((uint32_t)lenbuf[1] << 16) | ((uint32_t)lenbuf[2] << 8) | (uint32_t)lenbuf[3];
                if (len == 0 || len > 65536) break;
                std::vector<uint8_t> buf(len);
                ssize_t rr = recv(c, buf.data(), len, MSG_WAITALL);
                if (rr != (ssize_t)len) break;
                auto cb = onData_;
                if (cb) cb("wifi", buf);
            }
            close(c);
        }).detach();
    }
    close(s);
#else
    while (running_) std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
}

bool WifiDirect::sendTcp(const std::string& ip, uint16_t port, const std::vector<uint8_t>& data) {
#ifdef __linux__
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port); inet_aton(ip.c_str(), &addr.sin_addr);
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) < 0) { close(s); return false; }
    uint32_t len = (uint32_t)data.size();
    uint8_t lenbuf[4] = { (uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8), (uint8_t)len };
    if (send(s, lenbuf, 4, 0) != 4) { close(s); return false; }
    size_t off = 0; while (off < data.size()) { ssize_t n = send(s, data.data() + off, data.size() - off, 0); if (n <= 0) { close(s); return false; } off += (size_t)n; }
    close(s);
    return true;
#else
    (void)ip; (void)port; (void)data; return false;
#endif
}

}
