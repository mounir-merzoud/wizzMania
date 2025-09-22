#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include "../httplib.h"

class SimpleHTTPClient {
private:
    httplib::Client client;
    std::string auth_token;
    std::string username;
    std::atomic<bool> running{true};
    std::thread message_receiver;

public:
    SimpleHTTPClient() : client("http://localhost:8080") {
        client.set_read_timeout(10, 0);
        client.set_write_timeout(10, 0);
    }
    
    bool checkHealth() {
        std::cout << "🔍 Vérification du serveur HTTP...\n";
        auto res = client.Get("/health");
        
        if (res && res->status == 200) {
            std::cout << "✅ Serveur HTTP opérationnel: " << res->body << "\n";
            return true;
        } else {
            std::cout << "❌ Serveur HTTP non accessible\n";
            return false;
        }
    }
    
    bool authenticate() {
        std::cout << "\n🏥 === AUTHENTIFICATION SIMPLE ===\n";
        
        std::string password;
        std::cout << "Nom d'utilisateur: ";
        std::getline(std::cin, username);
        std::cout << "Mot de passe: ";
        std::getline(std::cin, password);
        
        // Simple JSON payload
        std::string json_payload = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
        
        auto res = client.Post("/api/v1/auth/login", json_payload, "application/json");
        
        if (res && res->status == 200) {
            std::cout << "✅ Authentification réussie!\n";
            std::cout << "Réponse: " << res->body << "\n";
            auth_token = "simple_token";
            return true;
        } else {
            std::cout << "❌ Échec de l'authentification\n";
            if (res) {
                std::cout << "Status: " << res->status << "\n";
                std::cout << "Body: " << res->body << "\n";
            }
            return false;
        }
    }
    
    void receiveMessages() {
        while (running) {
            httplib::Headers headers = {
                {"Authorization", "Bearer " + auth_token}
            };
            
            auto res = client.Get("/api/v1/chat/messages", headers);
            
            if (res && res->status == 200) {
                // Simple parsing - in real app, use JSON parser
                std::string response = res->body;
                if (response != "{\"messages\":[]}") {
                    std::cout << "\n📨 Nouveau message reçu!\n";
                    std::cout << "Données: " << response << "\n";
                    std::cout << username << ": ";
                    std::cout.flush();
                }
            }
            
            // Poll every 2 seconds
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    
    void startChat() {
        std::cout << "\n💬 === CHAT SIMPLE ===\n";
        std::cout << "Tapez vos messages (tapez 'quit' pour quitter)\n";
        std::cout << "📡 Démarrage de la réception des messages...\n\n";
        
        // Start message receiver thread
        message_receiver = std::thread(&SimpleHTTPClient::receiveMessages, this);
        
        std::string message;
        while (running) {
            std::cout << username << ": ";
            std::getline(std::cin, message);
            
            if (message == "quit") {
                running = false;
                break;
            }
            
            if (message.empty()) {
                continue;
            }
            
            // Send message
            std::string json_payload = "{\"message\":\"" + message + "\",\"sender\":\"" + username + "\"}";
            
            httplib::Headers headers = {
                {"Authorization", "Bearer " + auth_token}
            };
            
            auto res = client.Post("/api/v1/chat/send", headers, json_payload, "application/json");
            
            if (res && res->status == 200) {
                std::cout << "✅ Message envoyé: " << res->body << "\n";
            } else {
                std::cout << "❌ Erreur d'envoi\n";
                if (res) {
                    std::cout << "Status: " << res->status << "\n";
                }
            }
        }
        
        // Wait for receiver thread to finish
        if (message_receiver.joinable()) {
            message_receiver.join();
        }
        
        std::cout << "👋 Chat terminé!\n";
    }
};

int main() {
    std::cout << "🏥 === CLIENT SIMPLE HTTP ===\n";
    std::cout << "Version de test pour développement\n";
    std::cout << "================================\n\n";
    
    SimpleHTTPClient client;
    
    // Check server
    if (!client.checkHealth()) {
        std::cout << "💡 Démarrez le serveur avec: ./main server\n";
        return 1;
    }
    
    // Authenticate
    if (!client.authenticate()) {
        return 1;
    }
    
    // Start chat
    client.startChat();
    
    return 0;
}
