#pragma once

#include <string>

namespace utils {

class TokenCrypto {
public:
    static std::string encrypt(const std::string& plaintext);

    static std::string decrypt(const std::string& ciphertext);

private:
    // Encryption key - compiled into binary
    // Note: This provides obfuscation, not cryptographic security
    // It prevents casual token extraction from SD card
    static constexpr const char* ENCRYPTION_KEY = "oc-save-keeper-token-v1-secure";
};

} // namespace utils
