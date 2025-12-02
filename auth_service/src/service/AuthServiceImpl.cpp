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

        // Enregistre utilisateur
        try {
            database_->registerUser(fullName, email, hashedPassword);
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

        std::string token = authManager_->generateToken(std::to_string(user.id));

        // Remplir la réponse selon le proto
        response->set_access_token(token);
        response->set_refresh_token(""); // non implémenté pour l’instant
        response->set_expires_in(86400);  // 24h (en secondes)
        std::cout << "[AuthService] Login: success" << std::endl;
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
