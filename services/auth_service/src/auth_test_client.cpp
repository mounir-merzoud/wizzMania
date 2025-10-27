#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>
#include "auth.grpc.pb.h"

using securecloud::auth::AuthService;
using securecloud::auth::LoginRequest;
using securecloud::auth::LoginResponse;

namespace {
// Simple helper to run a login request and surface gRPC status information.
bool TryLogin(AuthService::Stub& stub,
              const std::string& username,
              const std::string& password,
              LoginResponse* response,
              grpc::Status* status) {
    LoginRequest request;
    request.set_username(username);
    request.set_password(password);

    grpc::ClientContext context;
    *status = stub.Login(&context, request, response);
    return status->ok();
}
}

int main(int argc, char** argv) {
    const std::string target = (argc > 1) ? argv[1] : "localhost:50051";
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    auto stub = AuthService::NewStub(channel);

    LoginResponse success_response;
    grpc::Status success_status;
    if (!TryLogin(*stub, "admin", "secure", &success_response, &success_status)) {
        std::cerr << "[AuthTest] Expected success but received error: "
                  << success_status.error_message() << "\n";
        return 1;
    }

    if (success_response.access_token().empty() || success_response.refresh_token().empty()) {
        std::cerr << "[AuthTest] Missing tokens in successful response\n";
        return 1;
    }

    LoginResponse failure_response;
    grpc::Status failure_status;
    TryLogin(*stub, "admin", "wrong_password", &failure_response, &failure_status);
    if (failure_status.error_code() != grpc::StatusCode::UNAUTHENTICATED) {
        std::cerr << "[AuthTest] Expected UNAUTHENTICATED error but got: "
                  << failure_status.error_message() << " (code="
                  << static_cast<int>(failure_status.error_code()) << ")\n";
        return 1;
    }

    std::cout << "[AuthTest] Login success scenario OK.\n";
    std::cout << "[AuthTest] Invalid credentials correctly rejected.\n";
    return 0;
}
