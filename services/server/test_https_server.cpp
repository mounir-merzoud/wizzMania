#include <iostream>
#include "../httplib.h"

int main() {
    httplib::SSLServer server("server.crt", "server.key");
    
    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });
    
    server.Post("/api/v1/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "Login request: " << req.body << std::endl;
        res.set_content("{\"token\":\"test_token\",\"expires_in\":3600}", "application/json");
    });
    
    std::cout << "Test HTTPS server starting on port 8443..." << std::endl;
    server.listen("localhost", 8443);
}
