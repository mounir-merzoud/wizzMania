#include <grpcpp/grpcpp.h>
#include "AuthServiceImpl.h"
#include "AuthManager.h"
#include <iostream>

using grpc::Server;
using grpc::ServerBuilder;

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    auto manager = std::make_shared<AuthManager>();
    AuthServiceImpl service(manager);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials()); // ⚠️ remplacer par TLS en prod
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "✅ AuthService listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
