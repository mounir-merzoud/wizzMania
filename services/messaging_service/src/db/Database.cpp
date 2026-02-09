#include "Database.h"

#include <stdexcept>

namespace {
bool parse_dm_key(const std::string& key, int* a, int* b) {
    if (!a || !b) return false;
    const std::string prefix = "dm:";
    if (key.rfind(prefix, 0) != 0) return false;
    const auto rest = key.substr(prefix.size());
    const auto pos = rest.find(':');
    if (pos == std::string::npos) return false;
    try {
        size_t idx1 = 0;
        size_t idx2 = 0;
        const int av = std::stoi(rest.substr(0, pos), &idx1);
        const int bv = std::stoi(rest.substr(pos + 1), &idx2);
        if (idx1 != rest.substr(0, pos).size()) return false;
        if (idx2 != rest.substr(pos + 1).size()) return false;
        *a = av;
        *b = bv;
        return true;
    } catch (...) {
        return false;
    }
}
}

Database::Database(std::string connStr) : conn_(std::move(connStr)) {
    if (!conn_.is_open()) {
        throw std::runtime_error("Failed to open PostgreSQL connection");
    }
}

int Database::ensureConversationId(const std::string& conversationKey) {
    std::lock_guard<std::mutex> lk(m_);
    pqxx::work tx(conn_);

    // "title" is used as a stable key for now.
    // This keeps the existing schema intact (no schema migration needed).
    auto r = tx.exec_params(
        "WITH existing AS (\n"
        "  SELECT id_conversations FROM conversations WHERE title = $1 LIMIT 1\n"
        "),\n"
        "inserted AS (\n"
        "  INSERT INTO conversations(title, type)\n"
        "  SELECT $1, 'private'\n"
        "  WHERE NOT EXISTS (SELECT 1 FROM existing)\n"
        "  RETURNING id_conversations\n"
        ")\n"
        "SELECT id_conversations FROM inserted\n"
        "UNION ALL\n"
        "SELECT id_conversations FROM existing\n"
        "LIMIT 1",
        conversationKey);

    if (r.empty()) {
        throw std::runtime_error("Failed to ensure conversation");
    }

    const int convId = r[0][0].as<int>();
    tx.commit();
    return convId;
}

int Database::insertMessage(const std::string& conversationKey,
                            std::optional<int> senderId,
                            const std::string& encryptedContentB64) {
    std::lock_guard<std::mutex> lk(m_);
    pqxx::work tx(conn_);

    auto rConv = tx.exec_params(
        "WITH existing AS (\n"
        "  SELECT id_conversations FROM conversations WHERE title = $1 LIMIT 1\n"
        "),\n"
        "inserted AS (\n"
        "  INSERT INTO conversations(title, type)\n"
        "  SELECT $1, 'private'\n"
        "  WHERE NOT EXISTS (SELECT 1 FROM existing)\n"
        "  RETURNING id_conversations\n"
        ")\n"
        "SELECT id_conversations FROM inserted\n"
        "UNION ALL\n"
        "SELECT id_conversations FROM existing\n"
        "LIMIT 1",
        conversationKey);

    if (rConv.empty()) {
        throw std::runtime_error("Failed to resolve conversation id");
    }
    const int convId = rConv[0][0].as<int>();

    int a = 0, b = 0;
    if (parse_dm_key(conversationKey, &a, &b)) {
        tx.exec_params(
            "INSERT INTO conversation_participant(id_users, id_conversations) VALUES ($1, $2) "
            "ON CONFLICT DO NOTHING",
            a,
            convId);
        tx.exec_params(
            "INSERT INTO conversation_participant(id_users, id_conversations) VALUES ($1, $2) "
            "ON CONFLICT DO NOTHING",
            b,
            convId);
    }

    pqxx::result rMsg;
    if (senderId.has_value()) {
        rMsg = tx.exec_params(
            "INSERT INTO messages(conversation_id, sender_id, encrypted_content)\n"
            "VALUES ($1, $2, $3)\n"
            "RETURNING id_messages",
            convId,
            *senderId,
            encryptedContentB64);
    } else {
        rMsg = tx.exec_params(
            "INSERT INTO messages(conversation_id, sender_id, encrypted_content)\n"
            "VALUES ($1, NULL, $2)\n"
            "RETURNING id_messages",
            convId,
            encryptedContentB64);
    }

    if (rMsg.empty()) {
        throw std::runtime_error("Failed to insert message");
    }

    const int msgId = rMsg[0][0].as<int>();
    tx.commit();
    return msgId;
}

std::vector<DbMessageRow> Database::getHistory(const std::string& conversationKey, int limit) {
    std::lock_guard<std::mutex> lk(m_);
    pqxx::work tx(conn_);

    if (conversationKey.empty()) {
        std::string sql =
            "SELECT m.id_messages, c.title AS conversation_key, m.sender_id, m.encrypted_content, "
            "EXTRACT(EPOCH FROM m.created_at)::bigint AS created_at_unix "
            "FROM messages m "
            "JOIN conversations c ON c.id_conversations = m.conversation_id "
            "ORDER BY m.id_messages DESC";
        if (limit > 0) {
            sql += " LIMIT $1";
        }

        pqxx::result r;
        if (limit > 0) {
            r = tx.exec_params(sql, limit);
        } else {
            r = tx.exec(sql);
        }

        std::vector<DbMessageRow> out;
        out.reserve(r.size());
        for (const auto& row : r) {
            DbMessageRow m;
            m.id_messages = row[0].as<int>();
            m.conversation_key = row[1].as<std::string>();
            if (row[2].is_null()) m.sender_id = std::nullopt;
            else m.sender_id = row[2].as<int>();
            m.encrypted_content_b64 = row[3].as<std::string>();
            m.created_at_unix = row[4].as<long long>();
            out.push_back(std::move(m));
        }

        tx.commit();
        return out;
    }

    auto rConv = tx.exec_params(
        "SELECT id_conversations FROM conversations WHERE title = $1 LIMIT 1",
        conversationKey);

    if (rConv.empty()) {
        tx.commit();
        return {};
    }

    const int convId = rConv[0][0].as<int>();

    std::string sql =
        "SELECT id_messages, sender_id, encrypted_content, "
        "EXTRACT(EPOCH FROM created_at)::bigint AS created_at_unix "
        "FROM messages "
        "WHERE conversation_id = $1 "
        "ORDER BY id_messages DESC";

    if (limit > 0) {
        sql += " LIMIT $2";
    }

    pqxx::result r;
    if (limit > 0) {
        r = tx.exec_params(sql, convId, limit);
    } else {
        r = tx.exec_params(sql, convId);
    }

    std::vector<DbMessageRow> out;
    out.reserve(r.size());

    for (const auto& row : r) {
        DbMessageRow m;
        m.id_messages = row[0].as<int>();
        m.conversation_key = conversationKey;
        if (row[1].is_null()) m.sender_id = std::nullopt;
        else m.sender_id = row[1].as<int>();
        m.encrypted_content_b64 = row[2].as<std::string>();
        m.created_at_unix = row[3].as<long long>();
        out.push_back(std::move(m));
    }

    tx.commit();
    return out;
}
