#include "EnvLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>

void EnvLoader::loadEnvFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "⚠️ Impossible d’ouvrir le fichier " << filename << std::endl;
        return;
    }

    std::string line;
    auto ltrim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch){ return !std::isspace(ch); }));
    };
    auto rtrim = [](std::string& s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
    };
    auto trim = [&](std::string& s){ ltrim(s); rtrim(s); };

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Nettoyage: trim aux bords mais garder les espaces internes (ex: POSTGRES_CONN)
        trim(key);
        trim(value);

        // Si valeur entre guillemets, retirer les guillemets d'encadrement
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front()=='\'' && value.back()=='\''))) {
            value = value.substr(1, value.size() - 2);
        }

#if defined(_WIN32)
    _putenv_s(key.c_str(), value.c_str());
#else
    setenv(key.c_str(), value.c_str(), 1);
#endif
    std::cout << "Loaded env: " << key << "=***" << std::endl;
    }

    file.close();
}

std::string EnvLoader::get(const std::string& key) {
#if defined(_WIN32)
    char* val = nullptr;
    size_t len;
    _dupenv_s(&val, &len, key.c_str());
    if (val == nullptr) return "";
    std::string result(val);
    free(val);
    return result;
#else
    const char* val = std::getenv(key.c_str());
    return val ? std::string(val) : "";
#endif
}
