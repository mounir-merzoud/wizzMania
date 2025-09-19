#include <iostream>
#include <string>
#include "client_server/client.cpp"
#include "server/server.cpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " [client|server]" << std::endl;
        return 1;
    }
    
    std::string mode = argv[1];
    
    if (mode == "client") {
        Client client;
        if (client.connectToServer()) {
            client.startCommunication();
        }
    } else if (mode == "server") {
        Server server;
        server.start();
    } else {
        std::cout << "Mode invalide. Utilisez 'client' ou 'server'" << std::endl;
        return 1;
    }
    
    return 0;
}