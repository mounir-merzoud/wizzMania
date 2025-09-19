#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

#define PORT 8080
#define MAX_CLIENTS 2

class Server {
private:
    int server_fd;
    std::vector<int> clients;
    std::mutex clients_mutex;

public:
    Server() {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == 0) {
            perror("Échec de création du socket");
            exit(EXIT_FAILURE);
        }
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);
        
       
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("Échec du bind");
            exit(EXIT_FAILURE);
        }
        if (listen(server_fd, 10) < 0) {
            perror("Échec du listen");
            exit(EXIT_FAILURE);
        }
    }

    void start() {
        std::cout << "Serveur en écoute sur le port " << PORT << std::endl;
        
        while (true) {
            struct sockaddr_in client_addr;
            int addrlen = sizeof(client_addr);
            int client_socket = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t*)&addrlen);
            
            if (client_socket < 0) {
                perror("Échec de l'accept");
                continue;
            }
                  
            // Vérification et ajout atomique avec mutex
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                if (clients.size() >= MAX_CLIENTS) {
                    std::cout << "Limite de clients atteinte. Nouvelle connexion refusée." << std::endl;
                    close(client_socket);
                    continue;
                }
                clients.push_back(client_socket);
            }
            
            std::cout << "Client connecté" << std::endl;
            std::thread(&Server::handleClient, this, client_socket).detach();
        }
    }

    void handleClient(int client_socket) {
        char buffer[1024];
        int client = client_socket;
        
        while (true) {
            int bytesReceived = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived <= 0) {
                if(bytesReceived == 0){
                    std::cout << "Client déconnecté proprement" << std::endl;
                } else {
                    std::cout << "Client déconnecté par erreur" << std::endl;
                }
                
                
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
                }
                break;
            }
            
            buffer[bytesReceived] = '\0';
            std::cout << "Message reçu: " << buffer << std::endl;
            
           
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                std::vector<int> clients_to_remove;
                
                for (int current_client : clients) {  
                    if (current_client != client) {
                        if (send(current_client, buffer, bytesReceived, 0) < 0) {
                            std::cout << "Erreur d'envoi au client" << std::endl;
                            close(current_client);
                            clients_to_remove.push_back(current_client);  
                        }
                    }
                }
                
               
                for (int client_to_remove : clients_to_remove) {
                    clients.erase(std::remove(clients.begin(), clients.end(), client_to_remove), clients.end());
                }
            }
        }
        close(client_socket);
    }

    ~Server() {
        close(server_fd);
    }
};
