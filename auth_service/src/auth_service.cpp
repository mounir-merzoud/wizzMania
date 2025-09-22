#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "auth.grpc.pb.h"  // généré par protoc

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using securecloud::auth::AuthService;
using securecloud::auth::LoginRequest;
using securecloud::auth::LoginResponse;
using securecloud::auth::RefreshTokenRequest;
using securecloud::auth::RefreshTokenResponse;
using securecloud::auth::ValidateTokenRequest;
using securecloud::auth::ValidateTokenResponse;
using securecloud::auth::RevokeTokensRequest;
using securecloud::auth::RevokeTokensResponse;

// Implémentation du service
class AuthServiceImpl final : public AuthService::Service {
public:
    Status Login(ServerContext* context, const LoginRequest* request, LoginResponse* response) override {
        std::cout << "Login request: " << request->username() << std::endl;

        // ⚠️ Ici tu mets ta vraie logique d’authentification (DB, hash password, MFA, etc.)
        if (request->username() == "admin" && request->password() == "secure") {
            response->set_access_token("JWT_FAKE_ACCESS");
            response->set_refresh_token("JWT_FAKE_REFRESH");
            response->set_expires_in(3600); // 1h
            return Status::OK;
        }

        return Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid credentials");
    }

    Status RefreshToken(ServerContext* context, const RefreshTokenRequest* request, RefreshTokenResponse* response) override {
        std::cout << "Refresh request with token: " << request->refresh_token() << std::endl;

        // ⚠️ Ici tu valides le refresh_token
        response->set_access_token("JWT_NEW_FAKE_ACCESS");
        response->set_expires_in(3600);
        return Status::OK;
    }

    Status ValidateToken(ServerContext* context, const ValidateTokenRequest* request, ValidateTokenResponse* response) override {
        std::cout << "Validate token: " << request->access_token() << std::endl;

        // ⚠️ Ici tu vérifies le JWT
        if (request->access_token() == "JWT_FAKE_ACCESS") {
            response->set_valid(true);
            response->set_user_id("user-123");
            response->add_permissions("read:messages");
            response->add_permissions("send:files");
            return Status::OK;
        }

        response->set_valid(false);
        return Status::OK;
    }

    Status RevokeTokens(ServerContext* context, const RevokeTokensRequest* request, RevokeTokensResponse* response) override {
        std::cout << "Revoke tokens for user: " << request->user_id() << std::endl;

        // ⚠️ Ici tu révoques les tokens dans ton système
        response->set_success(true);
        return Status::OK;
    }
};

// Fonction principale du serveur
void RunServer() {
    std::string server_address("0.0.0.0:50051");  // port gRPC par défaut
    AuthServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials()); // ⚠️ Mets TLS en prod
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "AuthService listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
