#pragma once
#include <string>

class AuthManager {
public:
    std::string GenerateAccessToken(const std::string& userId);
    bool ValidateToken(const std::string& token, std::string& userIdOut);
};
