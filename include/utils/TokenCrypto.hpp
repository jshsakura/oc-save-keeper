/**
 * oc-save-keeper - Token Encryption Utility
 * Simple XOR-based encryption for Refresh Token storage
 * Security: Prevents plain-text token extraction from SD card
 */
#pragma once

#include <string>

namespace utils {

class TokenCrypto {
public:
    /**
     * Encrypt a plaintext string using XOR cipher
     * @param plaintext The string to encrypt
     * @return Encrypted string (same length as input)
     */
    static std::string encrypt(const std::string& plaintext);
    
    /**
     * Decrypt a ciphertext string using XOR cipher
     * @param ciphertext The encrypted string
     * @return Decrypted plaintext
     */
    static std::string decrypt(const std::string& ciphertext);

private:
    // Encryption key - compiled into binary
    // Note: This provides obfuscation, not cryptographic security
    // It prevents casual token extraction from SD card
    static constexpr const char* ENCRYPTION_KEY = "oc-save-keeper-token-v1-secure";
};

} // namespace utils
