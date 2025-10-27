#pragma once

#include <grpcpp/grpcpp.h>
#include "auth.grpc.pb.h"
#include "AuthManager.h"

class AuthServiceImpl final : public securecloud::auth::AuthService::Service {
public:
    explicit AuthServiceImpl(std::shared_ptr<AuthManager> manager);

    grpc::Status Login(grpc::ServerContext* context,
                       const securecloud::auth::LoginRequest* request,
                       securecloud::auth::LoginResponse* response) override;

    grpc::Status RefreshToken(grpc::ServerContext* context,
                              const securecloud::auth::RefreshTokenRequest* request,
                              securecloud::auth::RefreshTokenResponse* response) override;

    grpc::Status ValidateToken(grpc::ServerContext* context,
                               const securecloud::auth::ValidateTokenRequest* request,
                               securecloud::auth::ValidateTokenResponse* response) override;

    grpc::Status RevokeTokens(grpc::ServerContext* context,
                              const securecloud::auth::RevokeTokensRequest* request,
                              securecloud::auth::RevokeTokensResponse* response) override;

private:
    std::shared_ptr<AuthManager> manager_;
};
