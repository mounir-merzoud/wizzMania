#include "Base64.h"

#include <array>
#include <cctype>
#include <stdexcept>

namespace {
constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline bool is_base64_char(unsigned char c) {
    return (std::isalnum(c) || (c == '+') || (c == '/'));
}

inline unsigned char b64_value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<unsigned char>(c - 'A');
    if (c >= 'a' && c <= 'z') return static_cast<unsigned char>(c - 'a' + 26);
    if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0' + 52);
    if (c == '+') return 62;
    if (c == '/') return 63;
    return 255;
}
}

namespace Base64 {

std::string encode(const std::string& bytes) {
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= bytes.size()) {
        const unsigned char b0 = static_cast<unsigned char>(bytes[i + 0]);
        const unsigned char b1 = static_cast<unsigned char>(bytes[i + 1]);
        const unsigned char b2 = static_cast<unsigned char>(bytes[i + 2]);

        out.push_back(kAlphabet[(b0 >> 2) & 0x3F]);
        out.push_back(kAlphabet[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)]);
        out.push_back(kAlphabet[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)]);
        out.push_back(kAlphabet[b2 & 0x3F]);

        i += 3;
    }

    const size_t rem = bytes.size() - i;
    if (rem == 1) {
        const unsigned char b0 = static_cast<unsigned char>(bytes[i + 0]);
        out.push_back(kAlphabet[(b0 >> 2) & 0x3F]);
        out.push_back(kAlphabet[(b0 & 0x03) << 4]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const unsigned char b0 = static_cast<unsigned char>(bytes[i + 0]);
        const unsigned char b1 = static_cast<unsigned char>(bytes[i + 1]);
        out.push_back(kAlphabet[(b0 >> 2) & 0x3F]);
        out.push_back(kAlphabet[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)]);
        out.push_back(kAlphabet[(b1 & 0x0F) << 2]);
        out.push_back('=');
    }

    return out;
}

std::string decode(const std::string& b64) {
    std::string in;
    in.reserve(b64.size());
    for (unsigned char c : b64) {
        if (std::isspace(c)) continue;
        in.push_back(static_cast<char>(c));
    }

    if (in.empty()) return {};
    if (in.size() % 4 != 0) {
        throw std::runtime_error("invalid base64 length");
    }

    std::string out;
    out.reserve((in.size() / 4) * 3);

    for (size_t i = 0; i < in.size(); i += 4) {
        const unsigned char c0 = static_cast<unsigned char>(in[i + 0]);
        const unsigned char c1 = static_cast<unsigned char>(in[i + 1]);
        const unsigned char c2 = static_cast<unsigned char>(in[i + 2]);
        const unsigned char c3 = static_cast<unsigned char>(in[i + 3]);

        if (!is_base64_char(c0) || !is_base64_char(c1) || (c2 != '=' && !is_base64_char(c2)) || (c3 != '=' && !is_base64_char(c3))) {
            throw std::runtime_error("invalid base64 character");
        }

        const unsigned char v0 = b64_value(c0);
        const unsigned char v1 = b64_value(c1);
        const unsigned char v2 = (c2 == '=') ? 0 : b64_value(c2);
        const unsigned char v3 = (c3 == '=') ? 0 : b64_value(c3);

        out.push_back(static_cast<char>((v0 << 2) | (v1 >> 4)));
        if (c2 != '=') {
            out.push_back(static_cast<char>(((v1 & 0x0F) << 4) | (v2 >> 2)));
        }
        if (c3 != '=') {
            out.push_back(static_cast<char>(((v2 & 0x03) << 6) | v3));
        }
    }

    return out;
}

} // namespace Base64
