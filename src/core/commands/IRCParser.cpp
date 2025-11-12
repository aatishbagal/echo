#include "IRCParser.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace echo {

IRCParser::IRCParser() {
    initializeCommands();
}

void IRCParser::initializeCommands() {
    commandMap_["/chat"] = CommandType::CHAT;
    commandMap_["/join"] = CommandType::JOIN;
    commandMap_["/exit"] = CommandType::EXIT;
    commandMap_["/msg"] = CommandType::MSG;
    commandMap_["/connect"] = CommandType::CONNECT;
    commandMap_["connect"] = CommandType::CONNECT;
    commandMap_["/who"] = CommandType::WHO;
    commandMap_["/nick"] = CommandType::NICK;
    commandMap_["devices"] = CommandType::DEVICES;
    commandMap_["echo"] = CommandType::ECHO_DEVICES;
    commandMap_["help"] = CommandType::HELP;
    commandMap_["scan"] = CommandType::SCAN;
    commandMap_["stop"] = CommandType::STOP;
    commandMap_["whoami"] = CommandType::WHOAMI;
    commandMap_["quit"] = CommandType::QUIT;
    commandMap_["exit"] = CommandType::QUIT;
    commandMap_["/status"] = CommandType::STATUS;
    commandMap_["clear"] = CommandType::CLEAR;
    commandMap_["cls"] = CommandType::CLEAR;
}

ParsedCommand IRCParser::parse(const std::string& input) {
    ParsedCommand cmd;
    cmd.rawCommand = input;
    
    std::string trimmedInput = trim(input);
    if (trimmedInput.empty()) {
        return cmd;
    }
    
    auto tokens = tokenize(trimmedInput);
    if (tokens.empty()) {
        return cmd;
    }
    
    std::string firstToken = tokens[0];
    std::transform(firstToken.begin(), firstToken.end(), firstToken.begin(), ::tolower);
    
    auto it = commandMap_.find(firstToken);
    if (it != commandMap_.end()) {
        cmd.type = it->second;
        cmd.isValid = true;
        
        for (size_t i = 1; i < tokens.size(); ++i) {
            cmd.arguments.push_back(tokens[i]);
        }
        
        switch (cmd.type) {
            case CommandType::CHAT:
                if (!cmd.arguments.empty()) {
                    cmd.target = extractUsername(cmd.arguments[0]);
                    cmd.isValid = !cmd.target.empty();
                } else {
                    cmd.isValid = false;
                }
                break;
            case CommandType::CONNECT:
                if (!cmd.arguments.empty()) {
                    cmd.target = cmd.arguments[0];
                } else {
                    cmd.isValid = false;
                }
                break;
                
            case CommandType::JOIN:
                if (!cmd.arguments.empty()) {
                    cmd.target = extractChannel(cmd.arguments[0]);
                    if (cmd.target.empty()) {
                        cmd.target = cmd.arguments[0];
                    }
                }
                break;
                
            case CommandType::MSG:
                if (cmd.arguments.size() >= 2) {
                    cmd.target = extractUsername(cmd.arguments[0]);
                    cmd.message = "";
                    for (size_t i = 1; i < cmd.arguments.size(); ++i) {
                        if (i > 1) cmd.message += " ";
                        cmd.message += cmd.arguments[i];
                    }
                } else {
                    cmd.isValid = false;
                }
                break;
                
            case CommandType::NICK:
                if (!cmd.arguments.empty()) {
                    cmd.target = cmd.arguments[0];
                } else {
                    cmd.isValid = false;
                }
                break;
                
            default:
                break;
        }
    } else {
        cmd.type = CommandType::NONE;
        cmd.isValid = false;
    }
    
    return cmd;
}

bool IRCParser::isValidUsername(const std::string& username) const {
    if (username.empty() || username.length() > 32) {
        return false;
    }
    
    return std::all_of(username.begin(), username.end(), [](char c) {
        return std::isalnum(c) || c == '_' || c == '-';
    });
}

bool IRCParser::isValidChannel(const std::string& channel) const {
    if (channel.empty() || channel.length() > 50) {
        return false;
    }
    
    if (channel[0] == '#') {
        std::string name = channel.substr(1);
        return !name.empty() && std::all_of(name.begin(), name.end(), [](char c) {
            return std::isalnum(c) || c == '_' || c == '-';
        });
    }
    
    return false;
}

std::string IRCParser::extractUsername(const std::string& target) const {
    if (target.empty()) {
        return "";
    }
    
    if (target[0] == '@') {
        return target.substr(1);
    }
    
    return target;
}

std::string IRCParser::extractChannel(const std::string& target) const {
    if (target.empty()) {
        return "";
    }
    
    if (target[0] != '#') {
        return "#" + target;
    }
    
    return target;
}

std::string IRCParser::formatPrivateMessage(const std::string& from, const std::string& message) {
    return "[" + from + " -> you]: " + message;
}

std::string IRCParser::formatGlobalMessage(const std::string& from, const std::string& message) {
    return "[#global][" + from + "]: " + message;
}

std::string IRCParser::formatSystemMessage(const std::string& message) {
    return "[System]: " + message;
}

std::vector<std::string> IRCParser::getCommandSuggestions(const std::string& partial) const {
    std::vector<std::string> suggestions;
    std::string lowerPartial = partial;
    std::transform(lowerPartial.begin(), lowerPartial.end(), lowerPartial.begin(), ::tolower);
    
    for (const auto& [cmd, type] : commandMap_) {
        if (cmd.find(lowerPartial) == 0) {
            suggestions.push_back(cmd);
        }
    }
    
    return suggestions;
}

std::vector<std::string> IRCParser::tokenize(const std::string& input) const {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string IRCParser::trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

} // namespace echo