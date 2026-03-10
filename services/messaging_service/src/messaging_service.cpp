#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "db/Database.h"
#include "utils/Base64.h"
#include "utils/EnvLoader.h"
#include <grpcpp/grpcpp.h>
#include "messaging.grpc.pb.h"
#include "messaging.pb.h"

using namespace securecloud::messaging;

class MessagingServiceImpl final : public MessagingService::Service {
private:
    std::mutex m_;
    struct ActiveStream {
        grpc::ServerReaderWriter<EncryptedMessage, EncryptedMessage>* stream;
        std::atomic<bool> alive{true};
    };
    std::vector<ActiveStream*> active_streams_;

    Database& db_;

    static bool parse_room_id(const std::string& conversationId, int* outConvId) {
        if (!outConvId) return false;
        const std::string prefix = "room:";
        if (conversationId.rfind(prefix, 0) != 0) return false;
        const auto rest = conversationId.substr(prefix.size());
        if (rest.empty()) return false;
        try {
            size_t idx = 0;
            const int v = std::stoi(rest, &idx);
            if (idx != rest.size()) return false;
            *outConvId = v;
            return true;
        } catch (...) {
            return false;
        }
    }

    static std::optional<int> parse_int(const std::string& s) {
        if (s.empty()) return std::nullopt;
        try {
            size_t idx = 0;
            const int v = std::stoi(s, &idx);
            if (idx != s.size()) return std::nullopt;
            return v;
        } catch (...) {
            return std::nullopt;
        }
    }

    static std::string safe_b64_decode(const std::string& b64) {
        try {
            return Base64::decode(b64);
        } catch (...) {
            // Backward/forward compatibility: if not base64, return as-is.
            return b64;
        }
    }

    void broadcast(const EncryptedMessage& msg) {
        std::lock_guard<std::mutex> lk(m_);
        // Purge streams morts
        for (auto it = active_streams_.begin(); it != active_streams_.end();) {
            if (!(*it)->alive) it = active_streams_.erase(it);
            else { ++it; }
        }
        for (auto* s : active_streams_) {
            if (s->alive) {
                s->stream->Write(msg); // best-effort
            }
        }
    }

    grpc::Status persist_and_broadcast(EncryptedMessage* msg) {
        if (!msg) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Internal error");
        }
        if (msg->conversation_id().empty()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Missing conversation_id");
        }

        if (msg->timestamp_unix() == 0) {
            msg->set_timestamp_unix(std::time(nullptr));
        }

        const auto senderId = parse_int(msg->sender_id());
        const std::string ciphertextB64 = Base64::encode(msg->ciphertext());
        int msgId = 0;
        try {
            msgId = db_.insertMessage(msg->conversation_id(), senderId, ciphertextB64);
        } catch (const std::exception& e) {
            return grpc::Status(grpc::StatusCode::INTERNAL, std::string("DB error: ") + e.what());
        }
        msg->set_message_id("db_" + std::to_string(msgId));

        broadcast(*msg);
        return grpc::Status::OK;
    }

public:
    explicit MessagingServiceImpl(Database& db) : db_(db) {}

    grpc::Status SendMessage(grpc::ServerContext*,
                             const EncryptedMessage* request,
                             SendAck* response) override {
        if (!request || !response) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Internal error");
        }

        EncryptedMessage stored = *request;
        const auto st = persist_and_broadcast(&stored);
        if (!st.ok()) return st;

        response->set_message_id(stored.message_id());
        response->set_accepted(true);
        return grpc::Status::OK;
    }

    grpc::Status GetHistory(grpc::ServerContext*,
                            const HistoryRequest* req,
                            HistoryResponse* resp) override {
        if (!req || !resp) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Internal error");
        }

        // Access control (rooms): if requester_id is provided and conversation_id is a room:<id>,
        // require that requester is a participant.
        if (!req->requester_id().empty() && !req->conversation_id().empty()) {
            const auto requesterId = parse_int(req->requester_id());
            int roomConvId = 0;
            if (requesterId.has_value() && parse_room_id(req->conversation_id(), &roomConvId)) {
                if (!db_.isParticipant(roomConvId, *requesterId)) {
                    return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Not a participant");
                }
            }
        }

        std::vector<DbMessageRow> rows;
        try {
            rows = db_.getHistory(req->conversation_id(), req->limit());
        } catch (const std::exception& e) {
            return grpc::Status(grpc::StatusCode::INTERNAL, std::string("DB error: ") + e.what());
        }
        for (const auto& row : rows) {
            auto* m = resp->add_messages();
            m->set_message_id("db_" + std::to_string(row.id_messages));
            m->set_conversation_id(req->conversation_id().empty() ? row.conversation_key : req->conversation_id());
            if (row.sender_id.has_value()) {
                m->set_sender_id(std::to_string(*row.sender_id));
            }
            m->set_ciphertext(safe_b64_decode(row.encrypted_content_b64));
            m->set_timestamp_unix(static_cast<long long>(row.created_at_unix));
        }

        return grpc::Status::OK;
    }

    grpc::Status CreateConversation(grpc::ServerContext*,
                                   const CreateConversationRequest* req,
                                   CreateConversationResponse* resp) override {
        if (!req || !resp) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Internal error");
        }

        const auto creatorId = parse_int(req->creator_id());
        if (!creatorId.has_value()) {
            resp->set_success(false);
            resp->set_message("Invalid creator_id");
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid creator_id");
        }
        if (req->title().empty()) {
            resp->set_success(false);
            resp->set_message("Missing title");
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Missing title");
        }

        int convId = 0;
        try {
            convId = db_.createGroupConversation(req->title());
        } catch (const std::exception& e) {
            resp->set_success(false);
            resp->set_message(std::string("DB error: ") + e.what());
            return grpc::Status(grpc::StatusCode::INTERNAL, "DB error");
        }

        // Always add creator.
        db_.addParticipant(convId, *creatorId);

        // Add provided participants.
        for (const auto& pid : req->participant_ids()) {
            const auto u = parse_int(pid);
            if (!u.has_value()) continue;
            (void)db_.addParticipant(convId, *u);
        }

        resp->set_success(true);
        resp->set_message("OK");
        resp->set_conversation_id("room:" + std::to_string(convId));
        return grpc::Status::OK;
    }

    grpc::Status AddParticipants(grpc::ServerContext*,
                                const AddParticipantsRequest* req,
                                AddParticipantsResponse* resp) override {
        if (!req || !resp) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Internal error");
        }

        int convId = 0;
        if (!parse_room_id(req->conversation_id(), &convId)) {
            resp->set_success(false);
            resp->set_message("Invalid conversation_id (expected room:<id>)");
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid conversation_id");
        }

        bool ok = true;
        for (const auto& pid : req->participant_ids()) {
            const auto u = parse_int(pid);
            if (!u.has_value()) {
                ok = false;
                continue;
            }
            ok = db_.addParticipant(convId, *u) && ok;
        }

        resp->set_success(ok);
        resp->set_message(ok ? "OK" : "Some participants could not be added");
        return ok ? grpc::Status::OK : grpc::Status(grpc::StatusCode::INTERNAL, "Failed to add participants");
    }

    grpc::Status ListConversations(grpc::ServerContext*,
                                  const ListConversationsRequest* req,
                                  ListConversationsResponse* resp) override {
        if (!req || !resp) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Internal error");
        }
        const auto userId = parse_int(req->user_id());
        if (!userId.has_value()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid user_id");
        }

        const int limit = req->limit() <= 0 ? 200 : std::max(1, std::min(1000, req->limit()));

        std::vector<DbConversationRow> rows;
        try {
            rows = db_.listConversationsForUser(*userId, limit);
        } catch (const std::exception& e) {
            return grpc::Status(grpc::StatusCode::INTERNAL, std::string("DB error: ") + e.what());
        }

        for (const auto& c : rows) {
            auto* out = resp->add_conversations();
            out->set_conversation_id("room:" + std::to_string(c.id_conversations));
            out->set_title(c.title);
            out->set_type(c.type);
            out->set_last_timestamp_unix(c.last_timestamp_unix);
        }

        return grpc::Status::OK;
    }

    grpc::Status DeleteConversation(grpc::ServerContext*,
                                   const DeleteConversationRequest* req,
                                   DeleteConversationResponse* resp) override {
        if (!req || !resp) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Internal error");
        }

        int convId = 0;
        if (!parse_room_id(req->conversation_id(), &convId)) {
            resp->set_success(false);
            resp->set_message("Invalid conversation_id (expected room:<id>)");
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid conversation_id");
        }

        try {
            const bool ok = db_.deleteConversationById(convId);
            if (!ok) {
                resp->set_success(false);
                resp->set_message("Room not found");
                return grpc::Status(grpc::StatusCode::NOT_FOUND, "Not found");
            }
            resp->set_success(true);
            resp->set_message("OK");
            return grpc::Status::OK;
        } catch (const std::exception& e) {
            resp->set_success(false);
            resp->set_message(std::string("DB error: ") + e.what());
            return grpc::Status(grpc::StatusCode::INTERNAL, "DB error");
        }
    }

    grpc::Status ChatStream(grpc::ServerContext*,
                            grpc::ServerReaderWriter<EncryptedMessage, EncryptedMessage>* stream) override {
        ActiveStream self{stream};
        {
            std::lock_guard<std::mutex> lk(m_);
            active_streams_.push_back(&self);
        }
        EncryptedMessage incoming;
        while (stream->Read(&incoming)) {
            (void)persist_and_broadcast(&incoming);
        }
        self.alive = false;
        return grpc::Status::OK;
    }
};

static void load_env_best_effort() {
#if defined(_WIN32)
    char exePath[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::string fullPath(exePath);
        const size_t lastSlash = fullPath.find_last_of("\\/");
        const std::string exeDir = (lastSlash == std::string::npos) ? std::string(".") : fullPath.substr(0, lastSlash);

        // Best-effort probing from the executable directory.
        EnvLoader::loadEnvFile(exeDir + "\\.env");
        EnvLoader::loadEnvFile(exeDir + "\\..\\.env");
        EnvLoader::loadEnvFile(exeDir + "\\..\\..\\.env");
        EnvLoader::loadEnvFile(exeDir + "\\..\\..\\..\\.env");
        EnvLoader::loadEnvFile(exeDir + "\\..\\..\\..\\..\\.env");
        EnvLoader::loadEnvFile(exeDir + "\\..\\..\\..\\..\\..\\.env");
    }

    // Fallback: current working directory.
    EnvLoader::loadEnvFile(".env");
#else
    EnvLoader::loadEnvFile("../../../.env");
    EnvLoader::loadEnvFile("../../../../.env");
#endif
}

// Quand le fichier est inclus dans les tests unitaires, on ne compile pas main().
#ifndef MESSAGING_SERVICE_IMPL_ONLY
int main(int argc, char** argv) {
    std::string addr = "0.0.0.0:7002";
    if (argc > 1 && argv[1] && std::string(argv[1]).size() > 0) {
        addr = argv[1];
    }

    load_env_best_effort();

    std::string pgConn = EnvLoader::get("POSTGRES_CONN");
    std::string dbHost = EnvLoader::get("DB_HOST");
    std::string dbName = EnvLoader::get("DB_NAME");
    std::string dbUser = EnvLoader::get("DB_USER");
    std::string dbPass = EnvLoader::get("DB_PASSWORD");

    std::string connStr;
    if (!pgConn.empty()) {
        connStr = pgConn;
    } else {
        if (dbHost.empty()) dbHost = "127.0.0.1";
        if (dbName.empty()) dbName = "securecloud";
        if (dbUser.empty()) dbUser = "secureuser";
        if (dbPass.empty()) dbPass = "securepass";
        connStr = "host=" + dbHost +
                  " dbname=" + dbName +
                  " user=" + dbUser +
                  " password=" + dbPass;
    }

    {
        std::string masked = connStr;
        auto pwPos = masked.find("password=");
        if (pwPos != std::string::npos) {
            auto end = masked.find(' ', pwPos);
            if (end == std::string::npos) end = masked.size();
            masked.replace(pwPos, end - pwPos, "password=***");
        }
        if (masked.rfind("postgresql://", 0) == 0 || masked.rfind("postgres://", 0) == 0) {
            // don't try to parse URL; just avoid dumping secrets in logs
            std::cout << "[messaging-service] Using POSTGRES_CONN (url, masked in logs)" << std::endl;
        } else {
            std::cout << "[messaging-service] ConnStr(masked): " << masked << std::endl;
        }
    }

    std::unique_ptr<Database> database;
    try {
        database = std::make_unique<Database>(connStr);
    } catch (const std::exception& e) {
        std::cerr << "[messaging-service] DB connection failed: " << e.what() << std::endl;
        return 1;
    }

    MessagingServiceImpl service(*database);
    grpc::ServerBuilder builder;
    int selectedPort = 0;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials(), &selectedPort);
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    if (!server || selectedPort == 0) {
        std::cerr << "[messaging-service] Failed to bind/listen on " << addr
                  << " (is the port already in use?)" << std::endl;
        return 2;
    }
    std::cout << "[messaging-service] listening on " << addr << std::endl;
    server->Wait();
    return 0;
}
#endif // MESSAGING_SERVICE_IMPL_ONLY