#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <array>

namespace echo {

class UserIdentity {
public:
    UserIdentity();
    ~UserIdentity();

    static UserIdentity generate();

    bool loadFromFile(const std::string& filepath);
    bool saveToFile(const std::string& filepath) const;

    std::string getUsername() const { return username_; }
    std::string getFingerprint() const { return fingerprint_; }
    std::array<uint8_t, 32> getPublicKey() const { return publicKey_; }

    void setUsername(const std::string& username);

    static std::string generateRandomUsername();
    
private:
    std::string username_;
    std::string fingerprint_;
    std::array<uint8_t, 32> publicKey_;
    std::array<uint8_t, 64> privateKey_;

    void generateKeypair();

    void updateFingerprint();
};

namespace UsernameLists {
    extern const std::vector<std::string> adjectives;
    extern const std::vector<std::string> nouns;
}

} // namespace echo