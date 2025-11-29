#include "WifiDirect.h"
#include <chrono>
#include <cstring>
#include <algorithm>
#include <iostream>

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
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
    if (verbose_) std::cout << "[WIFI] start username=" << username_ << " port=" << tcpPort_ << std::endl;
    udpTxThread_ = std::thread([this]() { runUdpTx(); });
    udpRxThread_ = std::thread([this]() { runUdpRx(); });
    tcpServerThread_ = std::thread([this]() { runTcpServer(); });
    return true;
}

void WifiDirect::stop() {
    if (!running_) return;
    running_ = false;
    if (verbose_) std::cout << "[WIFI] stop" << std::endl;
    try { if (udpTxThread_.joinable()) udpTxThread_.join(); } catch (...) {}
    try { if (udpRxThread_.joinable()) udpRxThread_.join(); } catch (...) {}
    try { if (tcpServerThread_.joinable()) tcpServerThread_.join(); } catch (...) {}
}

void WifiDirect::setOnData(std::function<void(const std::string&, const std::vector<uint8_t>&)> cb) { onData_ = std::move(cb); }

std::string WifiDirect::getLocalIp() const {
#ifdef __linux__
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return "";
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(80);
    inet_aton("8.8.8.8", &addr.sin_addr);
    connect(s, (sockaddr*)&addr, sizeof(addr));
    sockaddr_in name{}; socklen_t len = sizeof(name);
    if (getsockname(s, (sockaddr*)&name, &len) < 0) { close(s); return ""; }
    std::string ip = inet_ntoa(name.sin_addr);
    close(s);
    return ip;
#elif defined(_WIN32)
    WSADATA wsa; if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) { return ""; }
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { WSACleanup(); return ""; }
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);
    connect(s, (sockaddr*)&addr, sizeof(addr));
    sockaddr_in name{}; int len = sizeof(name);
    if (getsockname(s, (sockaddr*)&name, &len) == SOCKET_ERROR) { closesocket(s); WSACleanup(); return ""; }
    char ipstr[INET_ADDRSTRLEN] = {0}; inet_ntop(AF_INET, &name.sin_addr, ipstr, INET_ADDRSTRLEN);
    std::string ip = ipstr[0] ? ipstr : "";
    closesocket(s);
    WSACleanup();
    return ip;
#else
    return "";
#endif
}

bool WifiDirect::sendTo(const std::string& username, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = peers_.find(username);
    if (it == peers_.end()) {
    if (verbose_) std::cout << "[WIFI] sendTo no peer " << username << std::endl;
        return false;
    }
    bool ok = sendTcp(it->second.ip, it->second.port, data);
    if (verbose_) std::cout << (ok ? "[WIFI] sendTo ok " : "[WIFI] sendTo fail ") << username << " " << it->second.ip << ":" << it->second.port << " bytes=" << data.size() << std::endl;
    return ok;
}

bool WifiDirect::sendBroadcast(const std::vector<uint8_t>& data) {
    std::vector<std::pair<std::string,uint16_t>> targets;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto& kv : peers_) targets.emplace_back(kv.second.ip, kv.second.port);
    }
    if (verbose_) std::cout << "[WIFI] broadcast peers=" << targets.size() << " bytes=" << data.size() << std::endl;
    bool any = false;
    for (auto& t : targets) any |= sendTcp(t.first, t.second, data);
    return any;
}

std::vector<std::pair<std::string,std::string>> WifiDirect::listPeers() {
    std::vector<std::pair<std::string,std::string>> out;
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& kv : peers_) out.emplace_back(kv.first, kv.second.ip + ":" + std::to_string(kv.second.port));
    return out;
}

void WifiDirect::runUdpTx() {
#ifdef __linux__
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { if (verbose_) std::cout << "[WIFI] udp tx socket fail" << std::endl; while (running_) std::this_thread::sleep_for(std::chrono::seconds(1)); return; }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(48270); addr.sin_addr.s_addr = INADDR_BROADCAST;
    if (verbose_) std::cout << "[WIFI] UDP TX broadcasting to 255.255.255.255:48270" << std::endl;
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
        ssize_t sent = sendto(s, buf.data(), buf.size(), 0, (sockaddr*)&addr, sizeof(addr));
        if (verbose_) std::cout << "[WIFI] TX broadcast " << u << " (" << sent << "/" << buf.size() << " bytes)" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    close(s);
#elif defined(_WIN32)
    WSADATA wsa; if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) { if (verbose_) std::cout << "[WIFI] WSAStartup fail" << std::endl; return; }
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { if (verbose_) std::cout << "[WIFI] udp tx socket fail" << std::endl; WSACleanup(); return; }
    BOOL yes = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&yes, sizeof(yes));
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
        sendto(s, (const char*)buf.data(), (int)buf.size(), 0, (sockaddr*)&addr, sizeof(addr));
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    closesocket(s);
    WSACleanup();
#else
    if (verbose_) std::cout << "[WIFI] udp tx disabled on this OS" << std::endl;
    while (running_) std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
}

void WifiDirect::runUdpRx() {
#ifdef __linux__
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { if (verbose_) std::cout << "[WIFI] udp rx socket fail" << std::endl; while (running_) std::this_thread::sleep_for(std::chrono::seconds(1)); return; }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    timeval tv{}; tv.tv_sec = 1; tv.tv_usec = 0; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(48270); addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) { if (verbose_) std::cout << "[WIFI] udp bind fail" << std::endl; close(s); while (running_) std::this_thread::sleep_for(std::chrono::seconds(1)); return; }
    if (verbose_) std::cout << "[WIFI] UDP RX listening on 0.0.0.0:48270" << std::endl;
    std::vector<uint8_t> buf(512);
    while (running_) {
        sockaddr_in src{}; socklen_t sl = sizeof(src);
        ssize_t n = recvfrom(s, buf.data(), buf.size(), 0, (sockaddr*)&src, &sl);
        if (n <= 0) continue;
        if (verbose_) { std::string srcIp = inet_ntoa(src.sin_addr); std::cout << "[WIFI] RX packet from " << srcIp << " size=" << n << std::endl; }
        if (buf[0] != 1) { if (verbose_) std::cout << "[WIFI] Invalid version: " << (int)buf[0] << std::endl; continue; }
        size_t i = 1;
        if (i >= (size_t)n) continue;
        uint8_t ulen = buf[i++];
        if (i + ulen > (size_t)n) { if (verbose_) std::cout << "[WIFI] Invalid username length" << std::endl; continue; }
        std::string u((char*)&buf[i], (char*)&buf[i+ulen]); i += ulen;
        if (i >= (size_t)n) continue;
        uint8_t flen = buf[i++];
        if (i + flen + 2 > (size_t)n) { if (verbose_) std::cout << "[WIFI] Invalid fingerprint length" << std::endl; continue; }
        std::string f((char*)&buf[i], (char*)&buf[i+flen]); i += flen;
        uint16_t port = ((uint16_t)buf[i] << 8) | buf[i+1];
        std::string ip = inet_ntoa(src.sin_addr);
        if (u == username_) { if (verbose_) std::cout << "[WIFI] Ignoring own broadcast" << std::endl; continue; }
        Peer p; p.ip = ip; p.port = port; p.lastSeen = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(mtx_);
            peers_[u] = p;
        }
    std::cout << "[WIFI] ✓ Discovered peer: " << u << " at " << ip << ":" << port << std::endl;
    }
    close(s);
#elif defined(_WIN32)
    WSADATA wsa; if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) { if (verbose_) std::cout << "[WIFI] WSAStartup fail" << std::endl; return; }
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { if (verbose_) std::cout << "[WIFI] udp rx socket fail" << std::endl; WSACleanup(); return; }
    BOOL yes = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    DWORD tv = 1000; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(48270); addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) { if (verbose_) std::cout << "[WIFI] udp bind fail" << std::endl; closesocket(s); WSACleanup(); return; }
    std::vector<uint8_t> buf(512);
    while (running_) {
        sockaddr_in src{}; int sl = sizeof(src);
        int n = recvfrom(s, (char*)buf.data(), (int)buf.size(), 0, (sockaddr*)&src, &sl);
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
        char ipstr[INET_ADDRSTRLEN] = {0}; inet_ntop(AF_INET, &src.sin_addr, ipstr, INET_ADDRSTRLEN);
        std::string ip = ipstr[0] ? ipstr : "";
        if (u == username_) continue;
        Peer p; p.ip = ip; p.port = port; p.lastSeen = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(mtx_);
            peers_[u] = p;
        }
        if (verbose_) std::cout << "[WIFI] peer " << u << " " << ip << ":" << port << std::endl;
    }
    closesocket(s);
    WSACleanup();
#else
    if (verbose_) std::cout << "[WIFI] udp rx disabled on this OS" << std::endl;
    while (running_) std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
}

void WifiDirect::runTcpServer() {
#ifdef __linux__
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { if (verbose_) std::cout << "[WIFI] tcp socket fail" << std::endl; while (running_) std::this_thread::sleep_for(std::chrono::seconds(1)); return; }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(tcpPort_); addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) { if (verbose_) std::cout << "[WIFI] tcp bind fail" << std::endl; close(s); while (running_) std::this_thread::sleep_for(std::chrono::seconds(1)); return; }
    listen(s, 4);
    int flags = fcntl(s, F_GETFL, 0); if (flags != -1) fcntl(s, F_SETFL, flags | O_NONBLOCK);
    if (verbose_) std::cout << "[WIFI] tcp listen port=" << tcpPort_ << std::endl;
    while (running_) {
        sockaddr_in caddr{}; socklen_t cl = sizeof(caddr);
        int c = accept(s, (sockaddr*)&caddr, &cl);
        if (c < 0) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }
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
                if (verbose_) std::cout << "[WIFI] rx bytes=" << buf.size() << std::endl;
            }
            close(c);
        }).detach();
    }
    close(s);
#elif defined(_WIN32)
    WSADATA wsa; if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) { if (verbose_) std::cout << "[WIFI] WSAStartup fail" << std::endl; return; }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { if (verbose_) std::cout << "[WIFI] tcp socket fail" << std::endl; WSACleanup(); return; }
    BOOL yes = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(tcpPort_); addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) { if (verbose_) std::cout << "[WIFI] tcp bind fail" << std::endl; closesocket(s); WSACleanup(); return; }
    listen(s, 4);
    u_long mode = 1; ioctlsocket(s, FIONBIO, &mode);
    if (verbose_) std::cout << "[WIFI] tcp listen port=" << tcpPort_ << std::endl;
    while (running_) {
        sockaddr_in caddr{}; int cl = sizeof(caddr);
    SOCKET c = accept(s, (sockaddr*)&caddr, &cl);
    if (c == INVALID_SOCKET) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }
        std::thread([this,c]() {
            std::vector<uint8_t> lenbuf(4);
            while (true) {
                int r = recv(c, (char*)lenbuf.data(), 4, 0);
                if (r != 4) break;
                uint32_t len = ((uint32_t)lenbuf[0] << 24) | ((uint32_t)lenbuf[1] << 16) | ((uint32_t)lenbuf[2] << 8) | (uint32_t)lenbuf[3];
                if (len == 0 || len > 65536) break;
                std::vector<uint8_t> buf(len);
                int rr = 0; int need = (int)len;
                while (rr < need) { int n = recv(c, (char*)buf.data()+rr, need-rr, 0); if (n <= 0) { rr = -1; break; } rr += n; }
                if (rr != need) break;
                auto cb = onData_;
                if (cb) cb("wifi", buf);
                if (verbose_) std::cout << "[WIFI] rx bytes=" << buf.size() << std::endl;
            }
            closesocket(c);
        }).detach();
    }
    closesocket(s);
    WSACleanup();
#else
    std::cout << "[WIFI] tcp server disabled on this OS" << std::endl;
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
#elif defined(_WIN32)
    WSADATA wsa; if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) { return false; }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { WSACleanup(); return false; }
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port); inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) { closesocket(s); WSACleanup(); return false; }
    uint32_t len = (uint32_t)data.size();
    uint8_t lenbuf[4] = { (uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8), (uint8_t)len };
    if (send(s, (const char*)lenbuf, 4, 0) != 4) { closesocket(s); WSACleanup(); return false; }
    size_t off = 0; while (off < data.size()) { int n = send(s, (const char*)data.data() + off, (int)(data.size() - off), 0); if (n <= 0) { closesocket(s); WSACleanup(); return false; } off += (size_t)n; }
    closesocket(s);
    WSACleanup();
    return true;
#else
    (void)ip; (void)port; (void)data; return false;
#endif
}

}