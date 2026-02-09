#pragma once
#include <string>

class EnvLoader {
public:
    static std::string get(const std::string& key);
    static void loadEnvFile(const std::string& filename = ".env");
};
