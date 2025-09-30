#include "UserIdentity.h"
#include <random>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>

// For now, we'll use simple random generation
// TODO: Integrate libsodium for proper Ed25519 keypairs

namespace echo {

// Random username word lists
namespace UsernameLists {
    const std::vector<std::string> adjectives = {
        "Swift", "Quiet", "Bright", "Dark", "Silent", "Loud", "Quick", "Slow",
        "Happy", "Sad", "Brave", "Shy", "Wild", "Calm", "Bold", "Gentle",
        "Fierce", "Kind", "Wise", "Young", "Ancient", "Modern", "Classic", "Cool",
        "Warm", "Cold", "Hot", "Fresh", "Old", "New", "Blue", "Red",
        "Green", "Golden", "Silver", "Copper", "Iron", "Steel", "Stone", "Crystal",
        "Shadow", "Light", "Storm", "Cloud", "Sky", "Ocean", "River", "Mountain"
    };
    
    const std::vector<std::string> nouns = {
        "Fox", "Wolf", "Bear", "Eagle", "Hawk", "Owl", "Raven", "Crow",
        "Tiger", "Lion", "Panther", "Leopard", "Cheetah", "Jaguar", "Lynx", "Cat",
        "Dragon", "Phoenix", "Griffin", "Unicorn", "Pegasus", "Sphinx", "Hydra", "Kraken",
        "Warrior", "Knight", "Ranger", "Mage", "Rogue", "Hunter", "Scout", "Guard",
        "Star", "Moon", "Sun", "Comet", "Nova", "Galaxy", "Nebula", "Quasar",
        "Thunder", "Lightning", "Flame", "Frost", "Wind", "Earth", "Water", "Fire"
    };
}

UserIdentity::UserIdentity() {
    // Initialize with zeros
    publicKey_.fill(0);
    privateKey_.fill(0);
}

UserIdentity::~UserIdentity() {
    // Clear sensitive data
    privateKey_.fill(0);
}

UserIdentity UserIdentity::generate() {
    UserIdentity identity;
    identity.username_ = generateRandomUsername();
    identity.generateKeypair();
    identity.updateFingerprint();
    return identity;
}

std::string UserIdentity::generateRandomUsername() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    std::uniform_int_distribution<> adjDist(0, UsernameLists::adjectives.size() - 1);
    std::uniform_int_distribution<> nounDist(0, UsernameLists::nouns.size() - 1);
    
    std::string adjective = UsernameLists::adjectives[adjDist(gen)];
    std::string noun = UsernameLists::nouns[nounDist(gen)];
    
    return adjective + noun;
}

void UserIdentity::generateKeypair() {
    // TODO: Use libsodium crypto_sign_keypair() for proper Ed25519
    // For now, generate random bytes (INSECURE - for testing only!)
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < publicKey_.size(); ++i) {
        publicKey_[i] = static_cast<uint8_t>(dis(gen));
    }
    
    for (size_t i = 0; i < privateKey_.size(); ++i) {
        privateKey_[i] = static_cast<uint8_t>(dis(gen));
    }
}

void UserIdentity::updateFingerprint() {
    // TODO: Use proper SHA-256 from libsodium or OpenSSL
    // For now, create a simple hex representation of first 16 bytes
    
    std::stringstream ss;
    for (size_t i = 0; i < 16; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(publicKey_[i]);
    }
    fingerprint_ = ss.str();
}

void UserIdentity::setUsername(const std::string& username) {
    username_ = username;
}

bool UserIdentity::saveToFile(const std::string& filepath) const {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write username length and username
    uint32_t usernameLen = static_cast<uint32_t>(username_.size());
    file.write(reinterpret_cast<const char*>(&usernameLen), sizeof(usernameLen));
    file.write(username_.c_str(), usernameLen);
    
    // Write public key
    file.write(reinterpret_cast<const char*>(publicKey_.data()), publicKey_.size());
    
    // Write private key
    file.write(reinterpret_cast<const char*>(privateKey_.data()), privateKey_.size());
    
    return file.good();
}

bool UserIdentity::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read username
    uint32_t usernameLen = 0;
    file.read(reinterpret_cast<char*>(&usernameLen), sizeof(usernameLen));
    
    if (usernameLen > 0 && usernameLen < 256) {  // Sanity check
        std::vector<char> buffer(usernameLen);
        file.read(buffer.data(), usernameLen);
        username_ = std::string(buffer.begin(), buffer.end());
    }
    
    // Read public key
    file.read(reinterpret_cast<char*>(publicKey_.data()), publicKey_.size());
    
    // Read private key
    file.read(reinterpret_cast<char*>(privateKey_.data()), privateKey_.size());
    
    if (file.good()) {
        updateFingerprint();
        return true;
    }
    
    return false;
}

} // namespace echo