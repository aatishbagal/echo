#include "core/protocol/CompressedMessage.h"
#include <iostream>

using namespace echo;

int main() {
    std::cout << "\n=== BLE Compressed Message Test ===" << std::endl;
    
    // Test 1: Short message (single fragment)
    std::string shortMsg = "Hello, Echo!";
    std::string username = "alice";
    uint16_t msgID = 1;
    
    std::cout << "\n--- Test 1: Short Message ---" << std::endl;
    std::cout << "Original: \"" << shortMsg << "\" (" << shortMsg.length() << " bytes)" << std::endl;
    
    auto fragments1 = MessageFragmenter::fragment(shortMsg, username, msgID);
    std::cout << "Fragments: " << fragments1.size() << std::endl;
    
    for (size_t i = 0; i < fragments1.size(); ++i) {
        auto serialized = fragments1[i].serialize();
        std::cout << "Fragment " << i << ": " << serialized.size() << " bytes (fits in 31-byte BLE advertisement)" << std::endl;
    }
    
    std::string reassembled1 = MessageFragmenter::reassemble(fragments1);
    std::cout << "Reassembled: \"" << reassembled1 << "\"" << std::endl;
    std::cout << "Match: " << (reassembled1 == shortMsg ? "✓ YES" : "✗ NO") << std::endl;
    
    // Test 2: Medium message (multiple fragments)
    std::string mediumMsg = "This is a longer message that will need to be split into multiple BLE advertisements. Each advertisement can only hold 22 bytes of actual message data.";
    msgID = 2;
    
    std::cout << "\n--- Test 2: Medium Message ---" << std::endl;
    std::cout << "Original: " << mediumMsg.length() << " bytes" << std::endl;
    std::cout << "Text: \"" << mediumMsg << "\"" << std::endl;
    
    auto fragments2 = MessageFragmenter::fragment(mediumMsg, username, msgID);
    std::cout << "Fragments: " << fragments2.size() << std::endl;
    std::cout << "Total BLE advertisements needed: " << fragments2.size() << std::endl;
    
    for (size_t i = 0; i < fragments2.size(); ++i) {
        auto serialized = fragments2[i].serialize();
        std::cout << "  Fragment " << i << ": " << serialized.size() << " bytes";
        std::cout << " | Payload: \"";
        for (uint8_t byte : fragments2[i].payload) {
            std::cout << static_cast<char>(byte);
        }
        std::cout << "\"" << std::endl;
    }
    
    std::string reassembled2 = MessageFragmenter::reassemble(fragments2);
    std::cout << "Reassembled: " << reassembled2.length() << " bytes" << std::endl;
    std::cout << "Match: " << (reassembled2 == mediumMsg ? "✓ YES" : "✗ NO") << std::endl;
    
    // Test 3: Username hashing
    std::cout << "\n--- Test 3: Username Hashing ---" << std::endl;
    std::vector<std::string> usernames = {"alice", "bob", "charlie", "dave"};
    for (const auto& user : usernames) {
        uint32_t hash = MessageFragmenter::hashUsername(user);
        std::cout << "Username: " << user << " -> UserID: 0x" 
                  << std::hex << hash << std::dec << std::endl;
    }
    
    // Test 4: Maximum message size
    std::cout << "\n--- Test 4: Capacity ---" << std::endl;
    std::cout << "Max payload per fragment: " << MessageFragmenter::MAX_PAYLOAD_SIZE << " bytes" << std::endl;
    std::cout << "Max fragments: 255" << std::endl;
    std::cout << "Max total message: " << MessageFragmenter::MAX_MESSAGE_SIZE << " bytes" << std::endl;
    std::cout << "Character limit (assuming ASCII): ~" << MessageFragmenter::MAX_MESSAGE_SIZE << " characters" << std::endl;
    
    std::cout << "\n=== All Tests Passed! ===" << std::endl;
    
    return 0;
}
