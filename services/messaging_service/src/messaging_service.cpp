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