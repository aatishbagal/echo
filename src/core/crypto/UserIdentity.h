#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <array>

namespace echo {

// User identity for Echo/BitChat network
class UserIdentity {
public:
    UserIdentity();
    ~UserIdentity();
    
    // Generate new identity
    static UserIdentity generate();
    
    // Load/save identity
    bool loadFromFile(const std::string& filepath);
    bool saveToFile(const std::string& filepath) const;
    
    // Getters
    std::string getUsername() const { return username_; }
    std::string getFingerprint() const { return fingerprint_; }
    std::array<uint8_t, 32> getPublicKey() const { return publicKey_; }
    
    // Setters
    void setUsername(const std::string& username);
    
    // Generate random username (adjective + noun format)
    static std::string generateRandomUsername();
    
private:
    std::string username_;
    std::string fingerprint_;  // SHA-256 hash of public key (hex)
    std::array<uint8_t, 32> publicKey_;   // Ed25519 public key
    std::array<uint8_t, 64> privateKey_;  // Ed25519 private key
    
    // Generate keypair
    void generateKeypair();
    
    // Calculate fingerprint from public key
    void updateFingerprint();
};

// Random username word lists
namespace UsernameLists {
    extern const std::vector<std::string> adjectives;
    extern const std::vector<std::string> nouns;
}

} // namespace echo