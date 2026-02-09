#include "EnvLoader.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

void EnvLoader::loadEnvFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[messaging-service] EnvLoader: cannot open " << filename << std::endl;
        return;
    }

    std::string line;
    auto ltrim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    };
    auto rtrim = [](std::string& s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    };
    auto trim = [&](std::string& s) {
        ltrim(s);
        rtrim(s);
    };

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        const size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        trim(key);
        trim(value);

        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

#if defined(_WIN32)
        _putenv_s(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 1);
#endif
    }
}

std::string EnvLoader::get(const std::string& key) {
#if defined(_WIN32)
    char* val = nullptr;
    size_t len = 0;
    _dupenv_s(&val, &len, key.c_str());
    if (!val) return {};
    std::string result(val);
    free(val);
    return result;
#else
    const char* val = std::getenv(key.c_str());
    return val ? std::string(val) : std::string();
#endif
}
