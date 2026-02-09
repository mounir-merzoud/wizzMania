#pragma once

#include <pqxx/pqxx>

#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct DbMessageRow {
    int id_messages = 0;
    std::string conversation_key;
    std::optional<int> sender_id;
    std::string encrypted_content_b64;
    long long created_at_unix = 0;
};

class Database {
public:
    explicit Database(std::string connStr);

    int ensureConversationId(const std::string& conversationKey);

    int insertMessage(const std::string& conversationKey,
                      std::optional<int> senderId,
                      const std::string& encryptedContentB64);

    std::vector<DbMessageRow> getHistory(const std::string& conversationKey, int limit);

private:
    void ensureConnectedLocked();
    void reconnectLocked();

    std::string connStr_;
    pqxx::connection conn_;
    std::mutex m_;
};
