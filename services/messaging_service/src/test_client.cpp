#include <iostream>
#include <thread>
#include <atomic>
#include <grpcpp/grpcpp.h>
#include "messaging.grpc.pb.h"

using namespace securecloud::messaging;

int main(int argc, char** argv) {
    std::string target = (argc > 1) ? argv[1] : "localhost:7002";
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    auto stub = MessagingService::NewStub(channel);

    // SendMessage test
    {
        EncryptedMessage msg;
        msg.set_conversation_id("conv_demo");
        msg.set_sender_id("tester");
        msg.set_ciphertext("HELLO_CIPHERTEXT");
        grpc::ClientContext ctx;
        SendAck ack;
        auto status = stub->SendMessage(&ctx, msg, &ack);
        if (status.ok()) std::cout << "[SendMessage] OK id=" << ack.message_id() << "\n";
        else std::cout << "[SendMessage] FAIL: " << status.error_message() << "\n";
    }

    // History test
    {
        HistoryRequest req;
        req.set_conversation_id("conv_demo");
        req.set_limit(10);
        HistoryResponse resp;
        grpc::ClientContext ctx;
        auto status = stub->GetHistory(&ctx, req, &resp);
        if (status.ok()) {
            std::cout << "[GetHistory] count=" << resp.messages_size() << "\n";
        } else {
            std::cout << "[GetHistory] FAIL: " << status.error_message() << "\n";
        }
    }

    // ChatStream
    std::cout << "[ChatStream] type 'quit' to exit\n";
    grpc::ClientContext sctx;
    auto stream = stub->ChatStream(&sctx);
    std::atomic<bool> running{true};

    std::thread reader([&](){
        EncryptedMessage incoming;
        while (running && stream->Read(&incoming)) {
            std::cout << "[STREAM] " << incoming.message_id()
                      << " from=" << incoming.sender_id()
                      << " text_size=" << incoming.ciphertext().size() << "\n";
        }
    });

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") break;
        EncryptedMessage out;
        out.set_conversation_id("conv_demo");
        out.set_sender_id("tester");
        out.set_ciphertext(line);
        if (!stream->Write(out)) {
            std::cout << "[ChatStream] write closed\n";
            break;
        }
    }
    running = false;
    stream->WritesDone();
    reader.join();
    auto st = stream->Finish();
    std::cout << "[ChatStream] end: " << (st.ok() ? "OK" : st.error_message()) << "\n";
    return 0;
}