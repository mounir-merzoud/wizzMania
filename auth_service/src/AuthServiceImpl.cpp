#include "AuthServiceImpl.h"
#include <grpcpp/grpcpp.h>   // ✅ indispensable
#include <iostream>

using grpc::Status;
using grpc::StatusCode;
using grpc::ServerContext;   // ✅ ajout manquant

using namespace securecloud::auth;

AuthServiceImpl::AuthServiceImpl(std::shared_ptr<AuthManager> manager)
    : manager_(std::move(manager)) {}

Status AuthServiceImpl::Login(ServerContext* context, const LoginRequest* request, LoginResponse* response) {
    std::cout << "Login request: " << request->username() << std::endl;

    if (request->username() == "admin" && request->password() == "secure") {
        auto token = manager_->GenerateAccessToken("user-123");
        response->set_access_token(token);
        response->set_refresh_token("REFRESH_TOKEN_PLACEHOLDER");
        response->set_expires_in(3600);
        return Status::OK;
    }
    return {StatusCode::UNAUTHENTICATED, "Invalid credentials"};
}

Status AuthServiceImpl::RefreshToken(ServerContext* context, const RefreshTokenRequest* request, RefreshTokenResponse* response) {
    std::cout << "Refresh request with token: " << request->refresh_token() << std::endl;

    auto token = manager_->GenerateAccessToken("user-123");
    response->set_access_token(token);
    response->set_expires_in(3600);
    return Status::OK;
}

Status AuthServiceImpl::ValidateToken(ServerContext* context, const ValidateTokenRequest* request, ValidateTokenResponse* response) {
    std::cout << "Validate token: " << request->access_token() << std::endl;

    std::string userId;
    if (manager_->ValidateToken(request->access_token(), userId)) {
        response->set_valid(true);
        response->set_user_id(userId);
        response->add_permissions("read:messages");
        response->add_permissions("send:files");
    } else {
        response->set_valid(false);
    }
    return Status::OK;
}

Status AuthServiceImpl::RevokeTokens(ServerContext* context, const RevokeTokensRequest* request, RevokeTokensResponse* response) {
    std::cout << "Revoke tokens for user: " << request->user_id() << std::endl;
    response->set_success(true);
    return Status::OK;
}
