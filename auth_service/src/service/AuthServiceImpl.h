#pragma once
#include <grpcpp/grpcpp.h>
#include "../build/generated/auth.grpc.pb.h"
#include "../db/Database.h"
#include "../core/AuthManager.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

class AuthServiceImpl final : public securecloud::auth::AuthService::Service {
private:
    std::shared_ptr<Database> database_;
    std::shared_ptr<AuthManager> authManager_;

public:
    AuthServiceImpl(std::shared_ptr<AuthManager> auth, std::shared_ptr<Database> db);

    Status RegisterUser(ServerContext* context,
                       const securecloud::auth::RegisterRequest* request,
                       securecloud::auth::RegisterResponse* response) override;

    Status Login(ServerContext* context,
                const securecloud::auth::LoginRequest* request,
                securecloud::auth::LoginResponse* response) override;

    Status ValidateToken(ServerContext* context,
                        const securecloud::auth::ValidateTokenRequest* request,
                        securecloud::auth::ValidateTokenResponse* response) override;

    Status RefreshToken(ServerContext* context,
                       const securecloud::auth::RefreshTokenRequest* request,
                       securecloud::auth::RefreshTokenResponse* response) override;

    // Méthodes RBAC
    Status AssignRole(ServerContext* context,
                     const securecloud::auth::AssignRoleRequest* request,
                     securecloud::auth::AssignRoleResponse* response) override;

    Status CreateRole(ServerContext* context,
                     const securecloud::auth::CreateRoleRequest* request,
                     securecloud::auth::CreateRoleResponse* response) override;

    Status ListRoles(ServerContext* context,
                    const securecloud::auth::ListRolesRequest* request,
                    securecloud::auth::ListRolesResponse* response) override;

    Status GetUserPermissions(ServerContext* context,
                             const securecloud::auth::GetUserPermissionsRequest* request,
                             securecloud::auth::GetUserPermissionsResponse* response) override;

private:
    // Méthodes utilitaires
    bool validateAdminToken(const std::string& token);
};
