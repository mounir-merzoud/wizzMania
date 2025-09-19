#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

#define PORT 8080

class Client {
private:
    int sock = 0;
    struct sockaddr_in serv_addr;
    bool running = true;

public:
    Client() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    }

    bool connectToServer() {
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            return false;
        }
        std::cout << "Connecté au serveur" << std::endl;
        return true;
    }

    void startCommunication() {
        std::thread receiver(&Client::receiveMessage, this);
        std::string message;
        
        while (running) {
            std::cout << "you : ";
            std::getline(std::cin, message);
            if (message == "quit") {
                running = false;
                break;
            }
            send(sock, message.c_str(), message.length(), 0);
        }
        
        receiver.join();
    }

    void receiveMessage() {
        char buffer[1024];
        while (running) {
            int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived <= 0) break;
            buffer[bytesReceived] = '\0';
            std::cout << "\nother : " << buffer << "\nyou : ";
            std::cout.flush();
        }
    }

    ~Client() {
        close(sock);
    }
};



