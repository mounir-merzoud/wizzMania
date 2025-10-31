#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "service/AuthServiceImpl.h"
#include "core/AuthManager.h"
#include "db/Database.h"
#include "utils/EnvLoader.h"
#if defined(_WIN32)
#include <windows.h>
#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

void RunServer() {
    // Charger les variables d'environnement (.env)
    // 1) Dossier courant
    EnvLoader::loadEnvFile(".env");
    // 2) Dossier de l'exécutable -> remonter vers le dossier racine du microservice
#if defined(_WIN32)
    char exePath[MAX_PATH]{0};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        std::string p(exePath);
        auto pos = p.find_last_of("\\/");
        std::string exeDir = (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
        std::cout << "ExeDir=" << exeDir << std::endl;
        // build/bin/Debug -> up 3 => auth_service
        std::string envPath = exeDir + "\\..\\..\\..\\.env";
        std::cout << "TryEnvPath=" << envPath << std::endl;
        EnvLoader::loadEnvFile(envPath);
        // build/bin/Debug -> up 4 => repo root
        std::string rootEnvPath = exeDir + "\\..\\..\\..\\..\\.env";
        std::cout << "TryRootEnvPath=" << rootEnvPath << std::endl;
        EnvLoader::loadEnvFile(rootEnvPath);
    }
#else
    EnvLoader::loadEnvFile("../../../.env"); // auth_service/.env
    EnvLoader::loadEnvFile("../../../../.env"); // repo root .env
#endif
    std::string dbHost = EnvLoader::get("DB_HOST");
    std::string dbName = EnvLoader::get("DB_NAME");
    std::string dbUser = EnvLoader::get("DB_USER");
    std::string dbPass = EnvLoader::get("DB_PASSWORD");
    std::string pgConn = EnvLoader::get("POSTGRES_CONN");
    std::string jwtSecret = EnvLoader::get("JWT_SECRET");
    std::string serverAddress = EnvLoader::get("SERVER_ADDRESS");

    if (serverAddress.empty()) {
        serverAddress = "0.0.0.0:50051"; // fallback
    }

    std::cout << "🚀 Démarrage du serveur gRPC sur " << serverAddress << std::endl;
    std::cout << "DB_HOST=" << dbHost << " DB_NAME=" << dbName << " DB_USER=" << dbUser << std::endl;

    // Construire la chaîne de connexion PostgreSQL
    std::string connStr;
    if (!pgConn.empty()) {
        connStr = pgConn; // Permettre la connexion via POSTGRES_CONN complet
        std::cout << "Using POSTGRES_CONN from env" << std::endl;
        // Log sécurisé de la chaîne (masquer le mot de passe)
        std::string masked = connStr;
        auto pwPos = masked.find("password=");
        if (pwPos != std::string::npos) {
            auto end = masked.find(' ', pwPos);
            if (end == std::string::npos) end = masked.size();
            masked.replace(pwPos, end - pwPos, "password=***");
        }
        std::cout << "ConnStr(POSTGRES_CONN masked)=" << masked << std::endl;
    } else {
        // Valeurs par défaut si non définies
        if (dbHost.empty()) dbHost = "127.0.0.1";
        if (dbName.empty()) dbName = "securecloud";
        if (dbUser.empty()) dbUser = "secureuser";
        if (dbPass.empty()) dbPass = "securepass";
        connStr = "host=" + dbHost +
                  " dbname=" + dbName +
                  " user=" + dbUser +
                  " password=" + dbPass;
        std::cout << "ConnStr(without password)= host=" << dbHost
                  << " dbname=" << dbName
                  << " user=" << dbUser << std::endl;
    }

    // Initialiser la base de données
    auto database = std::make_shared<Database>(connStr);

    // Initialiser AuthManager
    auto authManager = std::make_shared<AuthManager>(jwtSecret);

    // Créer l’implémentation du service
    AuthServiceImpl service(authManager, database);

    // Configurer et lancer le serveur
    ServerBuilder builder;
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "✅ Serveur lancé avec succès." << std::endl;

    server->Wait();
}

int main() {
    try {
        RunServer();
    } catch (const std::exception& e) {
        std::cerr << "❌ Erreur fatale : " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
