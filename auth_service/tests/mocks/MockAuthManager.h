#pragma once

#include <gmock/gmock.h>
#include "core/AuthManager.h"

/**
 * @brief Mock de AuthManager pour isoler la logique JWT dans les tests.
 */
class MockAuthManager : public AuthManager {
public:
    MOCK_METHOD(std::string, generateTokenWithRole,
                (const std::string& userId,
                 const std::string& email,
                 const std::string& role,
                 const std::vector<std::string>& permissions), (override));

    MOCK_METHOD(TokenInfo, decodeToken,
                (const std::string& token), (override));
};
