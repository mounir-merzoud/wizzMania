#include <iostream>
#include <mutex>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "messaging.grpc.pb.h"
#include "messaging.pb.h"

using namespace securecloud::messaging;

struct StoredMessage {
    EncryptedMessage msg;
};

class MessagingServiceImpl final : public MessagingService::Service {
private:
    std::mutex m_;
    std::vector<StoredMessage> messages_;
    struct ActiveStream {
        grpc::ServerReaderWriter<EncryptedMessage, EncryptedMessage>* stream;
        std::atomic<bool> alive{true};
    };
    std::vector<ActiveStream*> active_streams_;

    std::string genMessageId() {
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "m_" + std::to_string(now);
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

public:
    grpc::Status SendMessage(grpc::ServerContext*,
                             const EncryptedMessage* request,
                             SendAck* response) override {
        EncryptedMessage stored = *request;
        if (stored.message_id().empty()) stored.set_message_id(genMessageId());
        if (stored.timestamp_unix() == 0) stored.set_timestamp_unix(std::time(nullptr));
        {
            std::lock_guard<std::mutex> lk(m_);
            messages_.push_back({stored});
        }
        broadcast(stored);
        response->set_message_id(stored.message_id());
        response->set_accepted(true);
        return grpc::Status::OK;
    }

    grpc::Status GetHistory(grpc::ServerContext*,
                            const HistoryRequest* req,
                            HistoryResponse* resp) override {
        std::lock_guard<std::mutex> lk(m_);
        int added = 0;
        for (auto it = messages_.rbegin(); it != messages_.rend(); ++it) {
            if (!req->conversation_id().empty() &&
                it->msg.conversation_id() != req->conversation_id()) continue;
            *resp->add_messages() = it->msg;
            if (req->limit() > 0 && ++added >= req->limit()) break;
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
            if (incoming.message_id().empty()) incoming.set_message_id(genMessageId());
            if (incoming.timestamp_unix() == 0) incoming.set_timestamp_unix(std::time(nullptr));
            {
                std::lock_guard<std::mutex> lk(m_);
                messages_.push_back({incoming});
            }
            broadcast(incoming);
        }
        self.alive = false;
        return grpc::Status::OK;
    }
};

int main(int argc, char** argv) {
    std::string addr = "0.0.0.0:7002";
    if (argc > 1) addr = argv[1];
    MessagingServiceImpl service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    std::cout << "[messaging-service] listening on " << addr << std::endl;
    server->Wait();
    return 0;
}