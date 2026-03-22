#include "utils/TokenCrypto.hpp"

#include <cstddef>
#include <string>

namespace utils {

namespace {

constexpr char kHexPrefix[] = "x1:";
constexpr char kEncryptionKey[] = "oc-save-keeper-token-v1-secure";

char nibbleToHex(unsigned char nibble) {
    return static_cast<char>(nibble < 10 ? ('0' + nibble) : ('a' + (nibble - 10)));
}

bool hexToNibble(char ch, unsigned char& out) {
    if (ch >= '0' && ch <= '9') {
        out = static_cast<unsigned char>(ch - '0');
        return true;
    }
    if (ch >= 'a' && ch <= 'f') {
        out = static_cast<unsigned char>(10 + (ch - 'a'));
        return true;
    }
    if (ch >= 'A' && ch <= 'F') {
        out = static_cast<unsigned char>(10 + (ch - 'A'));
        return true;
    }
    return false;
}

std::string xorCipher(const std::string& input) {
    std::string result = input;
    const std::string key = kEncryptionKey;

    if (key.empty()) {
        return result;
    }

    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] ^= key[i % key.size()];
    }
    return result;
}

std::string hexEncode(const std::string& bytes) {
    std::string encoded;
    encoded.reserve(sizeof(kHexPrefix) - 1 + (bytes.size() * 2));
    encoded += kHexPrefix;

    for (unsigned char byte : bytes) {
        encoded.push_back(nibbleToHex(static_cast<unsigned char>((byte >> 4) & 0x0F)));
        encoded.push_back(nibbleToHex(static_cast<unsigned char>(byte & 0x0F)));
    }
    return encoded;
}

bool hexDecode(const std::string& encoded, std::string& out) {
    if (encoded.rfind(kHexPrefix, 0) != 0) {
        return false;
    }

    const std::size_t hexStart = sizeof(kHexPrefix) - 1;
    const std::size_t hexSize = encoded.size() - hexStart;
    if ((hexSize % 2) != 0) {
        return false;
    }

    out.clear();
    out.reserve(hexSize / 2);
    for (std::size_t i = hexStart; i < encoded.size(); i += 2) {
        unsigned char hi = 0;
        unsigned char lo = 0;
        if (!hexToNibble(encoded[i], hi) || !hexToNibble(encoded[i + 1], lo)) {
            return false;
        }
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return true;
}

}

std::string TokenCrypto::encrypt(const std::string& plaintext) {
    return hexEncode(xorCipher(plaintext));
}

std::string TokenCrypto::decrypt(const std::string& ciphertext) {
    std::string decoded;
    if (hexDecode(ciphertext, decoded)) {
        return xorCipher(decoded);
    }
    return "";
}

} // namespace utils
