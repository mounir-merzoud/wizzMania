#include "gateway.grpc.pb.h"
#include "auth.grpc.pb.h"
#include "GatewayServiceImpl.cpp" // ou bien inclure son header si tu l’as séparé
#include <grpcpp/grpcpp.h>
#include <iostream>

using grpc::Server;
using grpc::ServerBuilder;

int main(int argc, char** argv) {
    std::string server_address("0.0.0.0:50052"); // Gateway écoute sur 50052

    // Stub vers AuthService (qui tourne sur 50051)
    auto authChannel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    auto authStub = securecloud::auth::AuthService::NewStub(authChannel);

    // Création de la Gateway avec uniquement Auth
    GatewayServiceImpl service(std::move(authStub));

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "GatewayService listening on " << server_address << std::endl;

    server->Wait();
    return 0;
}
