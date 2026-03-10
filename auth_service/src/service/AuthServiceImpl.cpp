#include "service/AuthServiceImpl.h"
#include <iostream>
#include <stdexcept>
#include <pqxx/pqxx>
#include "core/PasswordHasher.h"
#include <random>

namespace {
std::optional<std::string> get_bearer_from_metadata(const grpc::ServerContext* ctx) {
    if (!ctx) return std::nullopt;
    const auto& md = ctx->client_metadata();

    auto it = md.find("authorization");
    if (it == md.end()) {
        it = md.find("Authorization");
        if (it == md.end()) return std::nullopt;
    }

    std::string value(it->second.data(), it->second.size());
    const std::string prefix = "Bearer ";
    if (value.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }
    const auto token = value.substr(prefix.size());
    if (token.empty()) return std::nullopt;
    return token;
}

std::string random_hex(size_t bytes) {
    static const char* kHex = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    std::string out;
    out.reserve(bytes * 2);
    for (size_t i = 0; i < bytes; ++i) {
        const int v = dist(gen);
        out.push_back(kHex[(v >> 4) & 0xF]);
        out.push_back(kHex[v & 0xF]);
    }
    return out;
}

std::optional<int> parse_int(const std::string& s) {
    if (s.empty()) return std::nullopt;
    try {
        size_t idx = 0;
        int v = std::stoi(s, &idx);
        if (idx != s.size()) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}
}

AuthServiceImpl::AuthServiceImpl(std::shared_ptr<AuthManager> authManager,
                                 std::shared_ptr<Database> database)
    : authManager_(std::move(authManager)), database_(std::move(database)) {}

// ----------------------------
// Inscription utilisateur
// ----------------------------
Status AuthServiceImpl::RegisterUser(ServerContext* context,
                                     const securecloud::auth::RegisterRequest* request,
                                     securecloud::auth::RegisterResponse* response) {
    try {
    (void)context;
        std::string fullName = request->full_name();
        std::string email = request->email();
        std::string password = request->password();

        // Admin-only: require a valid admin token.
        if (!request->admin_token().empty()) {
            if (!validateAdminToken(request->admin_token())) {
                response->set_success(false);
                response->set_message("Accès refusé (admin requis).");
                return Status(StatusCode::PERMISSION_DENIED, "Admin token required");
            }
        } else {
            response->set_success(false);
            response->set_message("Token admin manquant.");
            return Status(StatusCode::PERMISSION_DENIED, "Missing admin token");
        }

        std::cout << "[AuthService] RegisterUser: email=" << email << std::endl;

        // Validation basique des champs requis
        if (fullName.empty() || email.empty() || password.empty()) {
            response->set_success(false);
            response->set_message("Champs requis manquants.");
            return Status(StatusCode::INVALID_ARGUMENT, "Missing required fields");
        }

        // Vérifie si l'utilisateur existe déjà
        if (database_->userExists(email)) {
            std::cout << "[AuthService] RegisterUser: already exists" << std::endl;
            response->set_success(false);
            response->set_message("Utilisateur déjà existant.");
            return Status(StatusCode::ALREADY_EXISTS, "User already exists");
        }

        // Hash mot de passe
        std::string hashedPassword = PasswordHasher::hashPassword(password);

        // Enregistre utilisateur avec rôle par défaut
        std::string roleName = !request->role_name().empty() ? request->role_name() : "user";
        try {
            database_->registerUserWithRole(fullName, email, hashedPassword, roleName);
        } catch (const pqxx::sql_error& se) {
            std::cerr << "[AuthService] RegisterUser SQL error: " << se.what() << std::endl;
            response->set_success(false);
            response->set_message("Erreur SQL lors de l'inscription.");
            return Status(StatusCode::INTERNAL, "DB error");
        } catch (const std::exception& e) {
            std::cerr << "[AuthService] RegisterUser error: " << e.what() << std::endl;
            response->set_success(false);
            response->set_message("Erreur lors de l'inscription.");
            return Status(StatusCode::INTERNAL, e.what());
        }

        response->set_success(true);
        response->set_message("Utilisateur créé avec succès.");
        std::cout << "[AuthService] RegisterUser: success" << std::endl;
        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] RegisterUser exception: " << e.what() << std::endl;
        return Status(StatusCode::INTERNAL, e.what());
    }
}

// ----------------------------
// Connexion utilisateur
// ----------------------------
Status AuthServiceImpl::Login(ServerContext* context,
                              const securecloud::auth::LoginRequest* request,
                              securecloud::auth::LoginResponse* response) {
    try {
    (void)context;
        // Dans ce service, on considère que username == email
        std::string email = request->username();
        std::string password = request->password();

        std::cout << "[AuthService] Login: email=" << email << std::endl;

        auto userOpt = database_->getUserByEmail(email);
        if (!userOpt.has_value()) {
            std::cout << "[AuthService] Login: user not found" << std::endl;
            return Status(StatusCode::NOT_FOUND, "Utilisateur introuvable");
        }

        auto user = userOpt.value();
        const bool password_ok = PasswordHasher::verifyPassword(password, user.password_hash);
        if (!password_ok) {
            std::cout << "[AuthService] Login: wrong password" << std::endl;
            return Status(StatusCode::UNAUTHENTICATED, "Mot de passe incorrect");
        }

        // Transparent migration: if the account still uses legacy SHA-256, upgrade to PBKDF2.
        if (PasswordHasher::isLegacyHashFormat(user.password_hash)) {
            try {
                const std::string new_hash = PasswordHasher::hashPassword(password);
                if (!database_->updateUserPasswordHash(user.id, new_hash)) {
                    std::cerr << "[AuthService] Login: failed to upgrade legacy password hash for user_id="
                              << user.id << std::endl;
                } else {
                    std::cout << "[AuthService] Login: upgraded legacy password hash for user_id="
                              << user.id << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[AuthService] Login: password hash upgrade exception: " << e.what() << std::endl;
            }
        }

        // Récupérer les permissions de l'utilisateur
        std::vector<std::string> permissions = database_->getUserPermissions(user.id);
        
        // Session id (jti): rotate on each login
        const auto previousJti = database_->getUserTokenJti(user.id);
        if (previousJti.has_value() && !previousJti->empty()) {
            (void)database_->revokeTokenJti(*previousJti);
        }
        const std::string jti = random_hex(16);
        (void)database_->setUserTokenJti(user.id, jti);

        // Générer un access token enrichi + un refresh token
        const int accessTtlSeconds = 3600; // 1h
        const int refreshTtlSeconds = 30 * 86400; // 30j
        std::string token = authManager_->generateAccessToken(
            std::to_string(user.id),
            user.email,
            user.role_name,
            permissions,
            jti,
            accessTtlSeconds
        );
        std::string refresh = authManager_->generateRefreshToken(
            std::to_string(user.id),
            jti,
            refreshTtlSeconds
        );

        // Remplir la réponse selon le proto enrichi
        response->set_access_token(token);
        response->set_refresh_token(refresh);
        response->set_expires_in(accessTtlSeconds);
        response->set_role(user.role_name);
        
        // Ajouter les permissions à la réponse
        for (const auto& perm : permissions) {
            response->add_permissions(perm);
        }
        
        std::cout << "[AuthService] Login: success, role=" << user.role_name 
                  << ", permissions=" << permissions.size() << std::endl;
        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] Login exception: " << e.what() << std::endl;
        return Status(StatusCode::INTERNAL, e.what());
    }
}

// ----------------------------
// Validation de token
// ----------------------------
Status AuthServiceImpl::ValidateToken(ServerContext* context,
                                      const securecloud::auth::ValidateTokenRequest* request,
                                      securecloud::auth::ValidateTokenResponse* response) {
    try {
    (void)context;
        const std::string token = request->access_token();
        std::cout << "[AuthService] ValidateToken: verifying" << std::endl;
        const auto info = authManager_->decodeToken(token);
        bool valid = info.valid;
        if (valid && !info.jti.empty()) {
            if (database_->isTokenRevoked(info.jti)) {
                valid = false;
            } else {
                const auto uid = parse_int(info.userId);
                if (uid.has_value()) {
                    const auto currentJti = database_->getUserTokenJti(*uid);
                    if (currentJti.has_value() && !currentJti->empty() && *currentJti != info.jti) {
                        valid = false;
                    }
                }
            }
        }

        response->set_valid(valid);
        if (valid) {
            response->set_user_id(info.userId);
            response->set_email(info.email);
            response->set_role(info.role);
            for (const auto& perm : info.permissions) {
                response->add_permissions(perm);
            }
        }
        std::cout << "[AuthService] ValidateToken: valid=" << (info.valid ? "true" : "false") << std::endl;
        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] ValidateToken exception: " << e.what() << std::endl;
        return Status(StatusCode::INTERNAL, e.what());
    }
}

// ----------------------------
// Users directory
// ----------------------------
Status AuthServiceImpl::ListUsers(ServerContext*,
                                 const securecloud::auth::ListUsersRequest* request,
                                 securecloud::auth::ListUsersResponse* response) {
    try {
        const std::string token = request->access_token();
        const auto info = authManager_->decodeToken(token);
        if (!info.valid) {
            return Status(StatusCode::UNAUTHENTICATED, "Invalid token");
        }

        const auto users = database_->listUsers();
        for (const auto& u : users) {
            if (!request->include_self() && std::to_string(u.id) == info.userId) {
                continue;
            }
            auto* out = response->add_users();
            out->set_user_id(std::to_string(u.id));
            out->set_full_name(u.full_name);
            out->set_email(u.email);
            out->set_role(u.role_name);
        }
        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] ListUsers exception: " << e.what() << std::endl;
        return Status(StatusCode::INTERNAL, e.what());
    }
}

bool AuthServiceImpl::validateAdminToken(const std::string& token) {
    const auto info = authManager_->decodeToken(token);
    return info.valid && info.role == "admin";
}

// Méthodes RBAC - implémentations temporaires pour compilation
Status AuthServiceImpl::RefreshToken(ServerContext* context,
                                   const securecloud::auth::RefreshTokenRequest* request,
                                   securecloud::auth::RefreshTokenResponse* response) {
    (void)context;
    try {
        const std::string refreshToken = request ? request->refresh_token() : std::string();
        if (refreshToken.empty()) {
            return Status(StatusCode::INVALID_ARGUMENT, "Missing refresh_token");
        }

        const auto info = authManager_->decodeToken(refreshToken);
        if (!info.valid) {
            return Status(StatusCode::UNAUTHENTICATED, "Invalid refresh token");
        }
        if (info.tokenType != "refresh") {
            return Status(StatusCode::UNAUTHENTICATED, "Not a refresh token");
        }
        const auto uid = parse_int(info.userId);
        if (!uid.has_value()) {
            return Status(StatusCode::UNAUTHENTICATED, "Invalid subject");
        }
        if (info.jti.empty()) {
            return Status(StatusCode::UNAUTHENTICATED, "Missing jti");
        }
        if (database_->isTokenRevoked(info.jti)) {
            return Status(StatusCode::UNAUTHENTICATED, "Refresh token revoked");
        }
        const auto currentJti = database_->getUserTokenJti(*uid);
        if (!currentJti.has_value() || currentJti->empty() || *currentJti != info.jti) {
            return Status(StatusCode::UNAUTHENTICATED, "Stale refresh token");
        }

        const auto userOpt = database_->getUserById(*uid);
        if (!userOpt.has_value()) {
            return Status(StatusCode::NOT_FOUND, "User not found");
        }
        const auto user = *userOpt;
        const std::vector<std::string> permissions = database_->getUserPermissions(user.id);

        const int accessTtlSeconds = 3600;
        const std::string access = authManager_->generateAccessToken(
            std::to_string(user.id),
            user.email,
            user.role_name,
            permissions,
            info.jti,
            accessTtlSeconds
        );
        if (access.empty()) {
            return Status(StatusCode::INTERNAL, "Failed to generate access token");
        }

        response->set_access_token(access);
        response->set_expires_in(accessTtlSeconds);
        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] RefreshToken exception: " << e.what() << std::endl;
        return Status(StatusCode::INTERNAL, e.what());
    }
}

Status AuthServiceImpl::RevokeTokens(ServerContext* context,
                                    const securecloud::auth::RevokeTokensRequest* request,
                                    securecloud::auth::RevokeTokensResponse* response) {
    try {
        if (!request || !response) {
            return Status(StatusCode::INTERNAL, "Internal error");
        }
        if (!context) {
            response->set_success(false);
            return Status(StatusCode::INTERNAL, "Missing context");
        }

        const auto bearerOpt = get_bearer_from_metadata(context);
        if (!bearerOpt.has_value()) {
            response->set_success(false);
            return Status(StatusCode::PERMISSION_DENIED, "Missing authorization metadata");
        }

        const auto caller = authManager_->decodeToken(*bearerOpt);
        if (!caller.valid) {
            response->set_success(false);
            return Status(StatusCode::PERMISSION_DENIED, "Invalid access token");
        }
        if (caller.tokenType != "access" && !caller.tokenType.empty()) {
            response->set_success(false);
            return Status(StatusCode::PERMISSION_DENIED, "Not an access token");
        }

        const std::string targetUserId = request->user_id();
        if (targetUserId.empty()) {
            response->set_success(false);
            return Status(StatusCode::INVALID_ARGUMENT, "Missing user_id");
        }

        // Only admin can revoke others; normal user can revoke self.
        if (caller.role != "admin" && caller.userId != targetUserId) {
            response->set_success(false);
            return Status(StatusCode::PERMISSION_DENIED, "Forbidden");
        }

        const auto uid = parse_int(targetUserId);
        if (!uid.has_value()) {
            response->set_success(false);
            return Status(StatusCode::INVALID_ARGUMENT, "Invalid user_id");
        }

        const auto currentJti = database_->getUserTokenJti(*uid);
        if (currentJti.has_value() && !currentJti->empty()) {
            (void)database_->revokeTokenJti(*currentJti);
        }
        (void)database_->setUserTokenJti(*uid, std::string()); // clear
        response->set_success(true);
        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] RevokeTokens exception: " << e.what() << std::endl;
        return Status(StatusCode::INTERNAL, e.what());
    }
}

Status AuthServiceImpl::DeleteUser(ServerContext* context,
                                  const securecloud::auth::DeleteUserRequest* request,
                                  securecloud::auth::DeleteUserResponse* response) {
    try {
        (void)context;
        if (!request || !response) {
            return Status(StatusCode::INTERNAL, "Internal error");
        }

        if (request->admin_token().empty() || !validateAdminToken(request->admin_token())) {
            response->set_success(false);
            response->set_message("Accès refusé (admin requis). ");
            return Status(StatusCode::PERMISSION_DENIED, "Admin token required");
        }

        const std::string targetUserId = request->user_id();
        if (targetUserId.empty()) {
            response->set_success(false);
            response->set_message("user_id requis.");
            return Status(StatusCode::INVALID_ARGUMENT, "Missing user_id");
        }

        // Avoid shooting oneself in the foot (UI/session).
        const auto caller = authManager_->decodeToken(request->admin_token());
        if (caller.valid && caller.userId == targetUserId) {
            response->set_success(false);
            response->set_message("Suppression de soi-même interdite.");
            return Status(StatusCode::INVALID_ARGUMENT, "Cannot delete self");
        }

        const auto uid = parse_int(targetUserId);
        if (!uid.has_value()) {
            response->set_success(false);
            response->set_message("user_id invalide.");
            return Status(StatusCode::INVALID_ARGUMENT, "Invalid user_id");
        }

        const bool ok = database_->deleteUserById(*uid);
        if (!ok) {
            response->set_success(false);
            response->set_message("Utilisateur introuvable.");
            return Status(StatusCode::NOT_FOUND, "User not found");
        }

        response->set_success(true);
        response->set_message("Utilisateur supprimé.");
        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] DeleteUser exception: " << e.what() << std::endl;
        if (response) {
            response->set_success(false);
            response->set_message("Erreur interne.");
        }
        return Status(StatusCode::INTERNAL, e.what());
    }
}

Status AuthServiceImpl::AssignRole(ServerContext* context,
                                 const securecloud::auth::AssignRoleRequest* request,
                                 securecloud::auth::AssignRoleResponse* response) {
    (void)context;
    try {
        if (request->admin_token().empty() || !validateAdminToken(request->admin_token())) {
            response->set_success(false);
            response->set_message("Accès refusé (admin requis). ");
            return Status(StatusCode::PERMISSION_DENIED, "Admin token required");
        }

        const std::string user_email = request->user_email();
        const std::string role_name = request->role_name();
        if (user_email.empty() || role_name.empty()) {
            response->set_success(false);
            response->set_message("Champs requis manquants (user_email/role_name).");
            return Status(StatusCode::INVALID_ARGUMENT, "Missing required fields");
        }

        auto userOpt = database_->getUserByEmail(user_email);
        if (!userOpt.has_value()) {
            response->set_success(false);
            response->set_message("Utilisateur introuvable.");
            return Status(StatusCode::NOT_FOUND, "User not found");
        }

        auto roleIdOpt = database_->getRoleIdByName(role_name);
        if (!roleIdOpt.has_value()) {
            response->set_success(false);
            response->set_message("Rôle introuvable.");
            return Status(StatusCode::NOT_FOUND, "Role not found");
        }

        const bool ok = database_->assignRoleToUser(userOpt->id, *roleIdOpt);
        response->set_success(ok);
        response->set_message(ok ? "Rôle assigné avec succès." : "Erreur lors de l'assignation du rôle.");
        return ok ? Status::OK : Status(StatusCode::INTERNAL, "Failed to assign role");
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] AssignRole exception: " << e.what() << std::endl;
        response->set_success(false);
        response->set_message("Erreur interne.");
        return Status(StatusCode::INTERNAL, e.what());
    }
}

Status AuthServiceImpl::CreateRole(ServerContext* context,
                                 const securecloud::auth::CreateRoleRequest* request,
                                 securecloud::auth::CreateRoleResponse* response) {
    (void)context;
    try {
        if (request->admin_token().empty() || !validateAdminToken(request->admin_token())) {
            response->set_success(false);
            response->set_message("Accès refusé (admin requis). ");
            return Status(StatusCode::PERMISSION_DENIED, "Admin token required");
        }

        const std::string role_name = request->role_name();
        const std::string description = request->description();
        if (role_name.empty()) {
            response->set_success(false);
            response->set_message("role_name requis.");
            return Status(StatusCode::INVALID_ARGUMENT, "Missing role_name");
        }

        if (!database_->createRole(role_name, description)) {
            response->set_success(false);
            response->set_message("Erreur DB lors de la création du rôle.");
            return Status(StatusCode::INTERNAL, "DB error");
        }

        auto roleIdOpt = database_->getRoleIdByName(role_name);
        if (!roleIdOpt.has_value()) {
            response->set_success(false);
            response->set_message("Rôle créé mais introuvable ensuite (incohérence DB).");
            return Status(StatusCode::INTERNAL, "Role lookup failed");
        }

        const int role_id = *roleIdOpt;
        for (const auto& perm : request->permissions()) {
            if (perm.empty()) continue;
            if (!database_->addPermissionToRole(role_id, perm)) {
                response->set_success(false);
                response->set_message("Erreur lors de l'ajout d'une permission au rôle.");
                return Status(StatusCode::INTERNAL, "Failed to add permission");
            }
        }

        response->set_success(true);
        response->set_message("Rôle créé avec succès.");
        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] CreateRole exception: " << e.what() << std::endl;
        response->set_success(false);
        response->set_message("Erreur interne.");
        return Status(StatusCode::INTERNAL, e.what());
    }
}

Status AuthServiceImpl::ListRoles(ServerContext* context,
                                const securecloud::auth::ListRolesRequest* request,
                                securecloud::auth::ListRolesResponse* response) {
    (void)context;
    try {
        if (request->admin_token().empty() || !validateAdminToken(request->admin_token())) {
            return Status(StatusCode::PERMISSION_DENIED, "Admin token required");
        }

        const auto roles = database_->listRoles();
        for (const auto& r : roles) {
            auto* out = response->add_roles();
            out->set_name(r.name);
            out->set_description(r.description);
            for (const auto& perm : r.permissions) {
                out->add_permissions(perm);
            }
        }

        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] ListRoles exception: " << e.what() << std::endl;
        return Status(StatusCode::INTERNAL, e.what());
    }
}

Status AuthServiceImpl::GetUserPermissions(ServerContext* context,
                                          const securecloud::auth::GetUserPermissionsRequest* request,
                                          securecloud::auth::GetUserPermissionsResponse* response) {
    (void)context;
    try {
        if (request->admin_token().empty() || !validateAdminToken(request->admin_token())) {
            return Status(StatusCode::PERMISSION_DENIED, "Admin token required");
        }

        const std::string user_email = request->user_email();
        if (user_email.empty()) {
            return Status(StatusCode::INVALID_ARGUMENT, "Missing user_email");
        }

        auto userOpt = database_->getUserByEmail(user_email);
        if (!userOpt.has_value()) {
            return Status(StatusCode::NOT_FOUND, "User not found");
        }

        response->set_role(userOpt->role_name);
        const auto perms = database_->getUserPermissions(userOpt->id);
        for (const auto& p : perms) {
            response->add_permissions(p);
        }

        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] GetUserPermissions exception: " << e.what() << std::endl;
        return Status(StatusCode::INTERNAL, e.what());
    }
}
