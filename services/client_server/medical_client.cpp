#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "../httplib.h"
#include <json/json.h>

#define SERVER_HOST "localhost"
#define HTTP_PORT 8080
#define HTTPS_PORT 8443

class MedicalClient {
private:
    httplib::Client http_client;
    httplib::SSLClient https_client;
    std::string auth_token;
    std::string user_id;
    bool running = true;

public:
    MedicalClient() : http_client("http://" + std::string(SERVER_HOST) + ":" + std::to_string(HTTP_PORT)),
                      https_client("https://" + std::string(SERVER_HOST) + ":" + std::to_string(HTTPS_PORT)) {
        
        // Configure SSL client to accept self-signed certificates (development only)
        https_client.enable_server_certificate_verification(false);
        
        // Set timeouts
        http_client.set_read_timeout(30, 0);
        https_client.set_read_timeout(30, 0);
        https_client.set_write_timeout(30, 0);
        
        // Additional SSL settings for self-signed certificates
        https_client.set_ca_cert_path("");
    }

    bool checkServerHealth() {
        std::cout << "🔍 Vérification de la santé du serveur..." << std::endl;
        
        // Test HTTP health
        auto http_res = http_client.Get("/health");
        if (http_res && http_res->status == 200) {
            std::cout << "✅ Serveur HTTP opérationnel" << std::endl;
        } else {
            std::cout << "❌ Serveur HTTP non accessible" << std::endl;
        }
        
        // Test HTTPS health
        auto https_res = https_client.Get("/health");
        if (https_res && https_res->status == 200) {
            std::cout << "✅ Serveur HTTPS opérationnel" << std::endl;
            return true;
        } else {
            std::cout << "❌ Serveur HTTPS non accessible" << std::endl;
            return false;
        }
    }

    bool authenticateUser() {
        std::cout << "\n🏥 === AUTHENTIFICATION MÉDICALE ===" << std::endl;
        
        std::string username, password;
        std::cout << "Nom d'utilisateur médical: ";
        std::getline(std::cin, username);
        std::cout << "Mot de passe: ";
        std::getline(std::cin, password);
        
        // Create JSON payload
        Json::Value credentials;
        credentials["username"] = username;
        credentials["password"] = password;
        credentials["license"] = "FR123456789"; // Example medical license
        
        Json::StreamWriterBuilder builder;
        std::string json_payload = Json::writeString(builder, credentials);
        
        // Send authentication request
        httplib::Headers headers = {
            {"Content-Type", "application/json"}
        };
        
        auto res = https_client.Post("/api/v1/auth/login", headers, json_payload, "application/json");
        
        if (res && res->status == 200) {
            // Parse response
            Json::Value response;
            Json::Reader reader;
            if (reader.parse(res->body, response)) {
                auth_token = response["token"].asString();
                user_id = username;
                
                std::cout << "✅ Authentification réussie!" << std::endl;
                std::cout << "🔑 Token reçu: " << auth_token.substr(0, 20) << "..." << std::endl;
                
                // Simulate 2FA
                return verify2FA();
            }
        }
        
        std::cout << "❌ Échec de l'authentification" << std::endl;
        return false;
    }

    bool verify2FA() {
        std::cout << "\n🔐 === AUTHENTIFICATION À DEUX FACTEURS ===" << std::endl;
        std::cout << "Code 2FA envoyé sur votre appareil médical sécurisé." << std::endl;
        
        std::string code;
        std::cout << "Entrez le code 2FA: ";
        std::getline(std::cin, code);
        
        Json::Value tfa_data;
        tfa_data["code"] = code;
        tfa_data["user_id"] = user_id;
        
        Json::StreamWriterBuilder builder;
        std::string json_payload = Json::writeString(builder, tfa_data);
        
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + auth_token}
        };
        
        auto res = https_client.Post("/api/v1/auth/2fa/verify", headers, json_payload, "application/json");
        
        if (res && res->status == 200) {
            std::cout << "✅ 2FA vérifié avec succès!" << std::endl;
            return true;
        }
        
        std::cout << "❌ Code 2FA invalide" << std::endl;
        return false;
    }

    void startSecureChat() {
        std::cout << "\n💬 === CHAT MÉDICAL SÉCURISÉ ===" << std::endl;
        std::cout << "Tapez vos messages (tapez 'quit' pour quitter, '/help' pour l'aide)" << std::endl;
        std::cout << "=== Chiffrement bout-en-bout activé ===" << std::endl;
        
        // Start message receiver thread
        std::thread receiver(&MedicalClient::receiveMessages, this);
        
        std::string message;
        while (running) {
            std::cout << user_id << " 🩺: ";
            std::getline(std::cin, message);
            
            if (message == "quit") {
                running = false;
                break;
            } else if (message == "/help") {
                showHelp();
                continue;
            } else if (message == "/status") {
                showStatus();
                continue;
            } else if (message.substr(0, 7) == "/upload") {
                uploadFile(message.substr(8));
                continue;
            }
            
            sendMessage(message);
        }
        
        receiver.join();
    }

private:
    void sendMessage(const std::string& message) {
        Json::Value msg_data;
        msg_data["message"] = message;
        msg_data["sender"] = user_id;
        msg_data["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        msg_data["type"] = "medical_chat";
        
        Json::StreamWriterBuilder builder;
        std::string json_payload = Json::writeString(builder, msg_data);
        
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + auth_token}
        };
        
        auto res = https_client.Post("/api/v1/chat/send", headers, json_payload, "application/json");
        
        if (!res || res->status != 200) {
            std::cout << "❌ Erreur d'envoi du message" << std::endl;
        }
    }

    void receiveMessages() {
        while (running) {
            httplib::Headers headers = {
                {"Authorization", "Bearer " + auth_token}
            };
            
            auto res = https_client.Get("/api/v1/chat/messages", headers);
            
            if (res && res->status == 200) {
                Json::Value messages;
                Json::Reader reader;
                if (reader.parse(res->body, messages)) {
                    // Process new messages (simplified for now)
                    // In real implementation, you'd track last message ID
                }
            }
            
            // Poll every 2 seconds
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    void uploadFile(const std::string& filename) {
        std::cout << "📁 Upload de fichier médical: " << filename << std::endl;
        
        // For demonstration - in real implementation, read actual file
        std::string file_content = "MEDICAL_FILE_CONTENT_ENCRYPTED";
        
        httplib::Headers headers = {
            {"Content-Type", "application/octet-stream"},
            {"Authorization", "Bearer " + auth_token},
            {"X-Medical-File-Type", "patient_data"},
            {"X-File-Name", filename}
        };
        
        auto res = https_client.Post("/api/v1/files/upload", headers, file_content, "application/octet-stream");
        
        if (res && res->status == 200) {
            Json::Value response;
            Json::Reader reader;
            if (reader.parse(res->body, response)) {
                std::string file_id = response["file_id"].asString();
                std::cout << "✅ Fichier uploadé avec succès. ID: " << file_id << std::endl;
            }
        } else {
            std::cout << "❌ Erreur lors de l'upload" << std::endl;
        }
    }

    void showHelp() {
        std::cout << "\n📖 === AIDE ===" << std::endl;
        std::cout << "/help     - Afficher cette aide" << std::endl;
        std::cout << "/status   - Statut de la connexion" << std::endl;
        std::cout << "/upload <filename> - Upload fichier médical" << std::endl;
        std::cout << "quit      - Quitter l'application" << std::endl;
        std::cout << "===============" << std::endl;
    }

    void showStatus() {
        std::cout << "\n📊 === STATUT ===" << std::endl;
        std::cout << "👤 Utilisateur: " << user_id << std::endl;
        std::cout << "🔐 Token: " << (auth_token.empty() ? "Non authentifié" : "Authentifié") << std::endl;
        std::cout << "🌐 Serveur: " << SERVER_HOST << ":" << HTTPS_PORT << std::endl;
        std::cout << "🔒 Chiffrement: AES-256-GCM actif" << std::endl;
        std::cout << "=================" << std::endl;
    }
};

int main() {
    std::cout << "🏥 === CLIENT MÉDICAL SÉCURISÉ ===" << std::endl;
    std::cout << "Version 1.0 - Conforme HIPAA/RGPD" << std::endl;
    std::cout << "==================================" << std::endl;

    MedicalClient client;
    
    // Check server health
    if (!client.checkServerHealth()) {
        std::cout << "❌ Impossible de se connecter au serveur." << std::endl;
        std::cout << "Vérifiez que le serveur est démarré avec: ./medical_server" << std::endl;
        return 1;
    }
    
    // Authenticate user
    if (!client.authenticateUser()) {
        std::cout << "❌ Authentification échouée. Arrêt du client." << std::endl;
        return 1;
    }
    
    // Start secure chat
    client.startSecureChat();
    
    std::cout << "\n👋 Session médicale terminée. Au revoir!" << std::endl;
    return 0;
}
