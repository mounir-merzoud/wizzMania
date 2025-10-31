#include <grpcpp/grpcpp.h>
#include <cstdlib>
#include <iostream>

#include "FileServiceImpl.h"

using grpc::Server;
using grpc::ServerBuilder;

namespace {
constexpr const char* kDefaultAddress = "0.0.0.0:50052";
constexpr const char* kDefaultStorageRoot = "storage";
}

int main(int argc, char** argv) {
    std::string server_address = kDefaultAddress;
    if (argc > 1) {
        server_address = argv[1];
    }

    const char* storage_env = std::getenv("FILE_SERVICE_STORAGE");
    std::string storage_root = storage_env ? storage_env : kDefaultStorageRoot;

    try {
        FileServiceImpl service(storage_root);

        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);

        std::unique_ptr<Server> server(builder.BuildAndStart());
        std::cout << "[file-service] listening on " << server_address
                  << " (storage root: " << storage_root << ")" << std::endl;
        server->Wait();
    } catch (const std::exception& ex) {
        std::cerr << "file-service failed to start: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
