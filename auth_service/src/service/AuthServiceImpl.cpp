#include "service/AuthServiceImpl.h"
#include <iostream>
#include <stdexcept>
#include <pqxx/pqxx>
#include "core/PasswordHasher.h"

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
        std::string fullName = request->full_name();
        std::string email = request->email();
        std::string password = request->password();

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
        if (!PasswordHasher::verifyPassword(password, user.password_hash)) {
            std::cout << "[AuthService] Login: wrong password" << std::endl;
            return Status(StatusCode::UNAUTHENTICATED, "Mot de passe incorrect");
        }

        // Récupérer les permissions de l'utilisateur
        std::vector<std::string> permissions = database_->getUserPermissions(user.id);
        
        // Générer un token enrichi avec rôle et permissions
        std::string token = authManager_->generateTokenWithRole(
            std::to_string(user.id), 
            user.email, 
            user.role_name, 
            permissions
        );

        // Remplir la réponse selon le proto enrichi
        response->set_access_token(token);
        response->set_refresh_token(""); // non implémenté pour l'instant
        response->set_expires_in(86400);  // 24h (en secondes)
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
        const std::string token = request->access_token();
        std::cout << "[AuthService] ValidateToken: verifying" << std::endl;
        bool ok = authManager_->verifyToken(token);
        response->set_valid(ok);
        // Optionnel: user_id/permissions non renseignés ici
        std::cout << "[AuthService] ValidateToken: valid=" << (ok ? "true" : "false") << std::endl;
        return Status::OK;
    } catch (const std::exception& e) {
        std::cerr << "[AuthService] ValidateToken exception: " << e.what() << std::endl;
        return Status(StatusCode::INTERNAL, e.what());
    }
}

// Méthodes RBAC - implémentations temporaires pour compilation
Status AuthServiceImpl::RefreshToken(ServerContext* context,
                                   const securecloud::auth::RefreshTokenRequest* request,
                                   securecloud::auth::RefreshTokenResponse* response) {
    return Status(StatusCode::UNIMPLEMENTED, "RefreshToken not implemented yet");
}

Status AuthServiceImpl::AssignRole(ServerContext* context,
                                 const securecloud::auth::AssignRoleRequest* request,
                                 securecloud::auth::AssignRoleResponse* response) {
    return Status(StatusCode::UNIMPLEMENTED, "AssignRole not implemented yet");
}

Status AuthServiceImpl::CreateRole(ServerContext* context,
                                 const securecloud::auth::CreateRoleRequest* request,
                                 securecloud::auth::CreateRoleResponse* response) {
    return Status(StatusCode::UNIMPLEMENTED, "CreateRole not implemented yet");
}

Status AuthServiceImpl::ListRoles(ServerContext* context,
                                const securecloud::auth::ListRolesRequest* request,
                                securecloud::auth::ListRolesResponse* response) {
    return Status(StatusCode::UNIMPLEMENTED, "ListRoles not implemented yet");
}

Status AuthServiceImpl::GetUserPermissions(ServerContext* context,
                                          const securecloud::auth::GetUserPermissionsRequest* request,
                                          securecloud::auth::GetUserPermissionsResponse* response) {
    return Status(StatusCode::UNIMPLEMENTED, "GetUserPermissions not implemented yet");
}
