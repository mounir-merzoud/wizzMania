#include "AuthManager.h"
#include <jwt-cpp/jwt.h>
#include <chrono>

std::string AuthManager::GenerateAccessToken(const std::string& userId) {
    auto token = jwt::create()
        .set_issuer("securecloud")
        .set_subject(userId)
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(1))
        .sign(jwt::algorithm::hs256{"super_secret_key"});
    return token;
}

bool AuthManager::ValidateToken(const std::string& token, std::string& userIdOut) {
    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{"super_secret_key"})
            .with_issuer("securecloud");
        verifier.verify(decoded);
        userIdOut = decoded.get_subject();
        return true;
    } catch (...) {
        return false;
    }
}
