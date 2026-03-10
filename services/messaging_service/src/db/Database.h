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

struct DbConversationRow {
    int id_conversations = 0;
    std::string title;
    std::string type;
    long long last_timestamp_unix = 0;
};

class Database {
public:
    explicit Database(std::string connStr);

    int ensureConversationId(const std::string& conversationKey);

    int insertMessage(const std::string& conversationKey,
                      std::optional<int> senderId,
                      const std::string& encryptedContentB64);

    std::vector<DbMessageRow> getHistory(const std::string& conversationKey, int limit);

    int createGroupConversation(const std::string& title);
    bool addParticipant(int conversationId, int userId);
    bool isParticipant(int conversationId, int userId);
    std::vector<DbConversationRow> listConversationsForUser(int userId, int limit);

    bool deleteConversationById(int conversationId);

private:
    void ensureConnectedLocked();
    void reconnectLocked();

    std::string connStr_;
    pqxx::connection conn_;
    std::mutex m_;
};
