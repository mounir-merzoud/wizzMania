#pragma once

#include <string>

namespace Base64 {
std::string encode(const std::string& bytes);
std::string decode(const std::string& b64);
}
