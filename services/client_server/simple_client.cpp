#include <iostream>
#include <string>
#include "../httplib.h"
#include <json/json.h>

class SimpleMedicalClient {
private:
    httplib::Client client;
    std::string auth_token;

public:
    SimpleMedicalClient() : client("http://localhost:8080") {}
    
    bool authenticate() {
        std::cout << "\n🏥 === AUTHENTIFICATION ===" << std::endl;
        
        std::string username, password;
        std::cout << "Nom d'utilisateur: ";
        std::getline(std::cin, username);
        std::cout << "Mot de passe: ";
        std::getline(std::cin, password);
        
        // Simple JSON (sans parser pour simplifier)
        std::string json_payload = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
        
        auto res = client.Post("/api/v1/auth/login", json_payload, "application/json");
        
        if (res && res->status == 200) {
            std::cout << "✅ Authentification réussie!" << std::endl;
            std::cout << "Réponse: " << res->body << std::endl;
            auth_token = "test_token";
            return true;
        }
        
        std::cout << "❌ Échec de l'authentification" << std::endl;
        if (res) {
            std::cout << "Status: " << res->status << std::endl;
            std::cout << "Response: " << res->body << std::endl;
        }
        return false;
    }
    
    void sendMessage() {
        std::cout << "\n💬 === ENVOI DE MESSAGE ===" << std::endl;
        std::string message;
        std::cout << "Message: ";
        std::getline(std::cin, message);
        
        std::string json_payload = "{\"message\":\"" + message + "\",\"sender\":\"test_user\"}";
        
        httplib::Headers headers = {
            {"Authorization", "Bearer " + auth_token}
        };
        
        auto res = client.Post("/api/v1/chat/send", headers, json_payload, "application/json");
        
        if (res && res->status == 200) {
            std::cout << "✅ Message envoyé!" << std::endl;
            std::cout << "Réponse: " << res->body << std::endl;
        } else {
            std::cout << "❌ Erreur d'envoi" << std::endl;
            if (res) {
                std::cout << "Status: " << res->status << std::endl;
                std::cout << "Response: " << res->body << std::endl;
            }
        }
    }
};

int main() {
    std::cout << "🏥 === CLIENT MÉDICAL SIMPLE ===" << std::endl;
    
    // Test health
    httplib::Client test_client("http://localhost:8080");
    auto health = test_client.Get("/health");
    
    if (!health || health->status != 200) {
        std::cout << "❌ Serveur non accessible" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Serveur accessible: " << health->body << std::endl;
    
    SimpleMedicalClient client;
    
    if (client.authenticate()) {
        client.sendMessage();
    }
    
    return 0;
}
