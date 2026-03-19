/**
 * oc-save-keeper - Token Encryption Utility
 * Simple XOR-based encryption for Refresh Token storage
 */
#include "utils/TokenCrypto.hpp"

namespace utils {

std::string TokenCrypto::encrypt(const std::string& plaintext) {
    std::string result = plaintext;
    const std::string key = ENCRYPTION_KEY;
    
    if (key.empty()) {
        return result; // No encryption if key is empty
    }
    
    for (size_t i = 0; i < result.size(); i++) {
        result[i] ^= key[i % key.size()];
    }
    return result;
}

std::string TokenCrypto::decrypt(const std::string& ciphertext) {
    // XOR cipher is self-inverse
    return encrypt(ciphertext);
}

} // namespace utils
