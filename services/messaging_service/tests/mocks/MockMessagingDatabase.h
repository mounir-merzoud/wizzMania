#pragma once

#include <gmock/gmock.h>
#include "db/Database.h"

/**
 * @brief Mock de Database pour isoler MessagingServiceImpl de PostgreSQL.
 */
class MockMessagingDatabase : public Database {
public:
    MockMessagingDatabase() : Database("") {}

    MOCK_METHOD(int, ensureConversationId,
                (const std::string& conversationKey), (override));

    MOCK_METHOD(int, insertMessage,
                (const std::string& conversationKey,
                 std::optional<int> senderId,
                 const std::string& encryptedContentB64), (override));

    MOCK_METHOD(std::vector<DbMessageRow>, getHistory,
                (const std::string& conversationKey, int limit), (override));
};
