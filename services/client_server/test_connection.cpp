#include <iostream>
#include "../httplib.h"

int main() {
    std::cout << "Test de connectivité HTTP/HTTPS..." << std::endl;
    
    // Test HTTP
    httplib::Client http_client("http://localhost:8080");
    auto http_res = http_client.Get("/health");
    
    if (http_res && http_res->status == 200) {
        std::cout << "✅ HTTP OK: " << http_res->body << std::endl;
    } else {
        std::cout << "❌ HTTP KO" << std::endl;
    }
    
    // Test HTTPS
    httplib::SSLClient https_client("https://localhost:8443");
    https_client.enable_server_certificate_verification(false);
    auto https_res = https_client.Get("/health");
    
    if (https_res && https_res->status == 200) {
        std::cout << "✅ HTTPS OK: " << https_res->body << std::endl;
    } else {
        std::cout << "❌ HTTPS KO" << std::endl;
        if (https_res) {
            std::cout << "Status: " << https_res->status << std::endl;
        } else {
            std::cout << "Pas de réponse HTTPS" << std::endl;
        }
    }
    
    return 0;
}
