#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace echo {

enum class CommandType {
    NONE,
    CHAT,
    JOIN,
    EXIT,
    MSG,
    WHO,
    NICK,
    DEVICES,
    ECHO_DEVICES,
    HELP,
    SCAN,
    STOP,
    WHOAMI,
    QUIT,
    STATUS,
    CLEAR
};

struct ParsedCommand {
    CommandType type = CommandType::NONE;
    std::string rawCommand;
    std::vector<std::string> arguments;
    std::string target;
    std::string message;
    bool isValid = false;
};

class IRCParser {
public:
    IRCParser();
    
    ParsedCommand parse(const std::string& input);
    
    bool isValidUsername(const std::string& username) const;
    bool isValidChannel(const std::string& channel) const;
    
    std::string extractUsername(const std::string& target) const;
    std::string extractChannel(const std::string& target) const;
    
    static std::string formatPrivateMessage(const std::string& from, const std::string& message);
    static std::string formatGlobalMessage(const std::string& from, const std::string& message);
    static std::string formatSystemMessage(const std::string& message);
    
    std::vector<std::string> getCommandSuggestions(const std::string& partial) const;
    
private:
    std::unordered_map<std::string, CommandType> commandMap_;
    
    void initializeCommands();
    std::vector<std::string> tokenize(const std::string& input) const;
    std::string trim(const std::string& str) const;
};

} // namespace echo