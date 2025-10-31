#pragma once

#include <grpcpp/grpcpp.h>
#include "auth.grpc.pb.h"
#include "core/AuthManager.h"
#include "db/Database.h"

using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

class AuthServiceImpl final : public securecloud::auth::AuthService::Service {
public:
    AuthServiceImpl(std::shared_ptr<AuthManager> authManager,
                    std::shared_ptr<Database> database);

    Status RegisterUser(ServerContext* context,
                        const securecloud::auth::RegisterRequest* request,
                        securecloud::auth::RegisterResponse* response) override;

    Status Login(ServerContext* context,
                 const securecloud::auth::LoginRequest* request,
                 securecloud::auth::LoginResponse* response) override;

    Status ValidateToken(ServerContext* context,
                         const securecloud::auth::ValidateTokenRequest* request,
                         securecloud::auth::ValidateTokenResponse* response) override;

private:
    std::shared_ptr<AuthManager> authManager_;
    std::shared_ptr<Database> database_;
};
