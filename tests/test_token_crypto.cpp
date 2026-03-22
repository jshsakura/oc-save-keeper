#include "tests/TestHarness.hpp"

#include "utils/TokenCrypto.hpp"

#include <string>

TEST_CASE("TokenCrypto encrypt output stays JSON-safe text") {
    const std::string plaintext = "sl.ABC123_xyz-RefreshToken";
    const std::string encrypted = utils::TokenCrypto::encrypt(plaintext);

    REQUIRE(encrypted.rfind("x1:", 0) == 0);
    for (std::size_t i = 3; i < encrypted.size(); ++i) {
        const char ch = encrypted[i];
        const bool isDigit = ch >= '0' && ch <= '9';
        const bool isLowerHex = ch >= 'a' && ch <= 'f';
        REQUIRE(isDigit || isLowerHex);
    }
}

TEST_CASE("TokenCrypto decrypt round-trips new encoded ciphertext") {
    const std::string plaintext = "refresh-token-with-symbols._-~abc123";
    const std::string encrypted = utils::TokenCrypto::encrypt(plaintext);

    REQUIRE_EQ(utils::TokenCrypto::decrypt(encrypted), plaintext);
}

TEST_CASE("TokenCrypto decrypt rejects legacy raw XOR ciphertext") {
    const std::string plaintext = "legacy-refresh-token";
    const std::string key = "oc-save-keeper-token-v1-secure";

    std::string legacyCipher = plaintext;
    for (std::size_t i = 0; i < legacyCipher.size(); ++i) {
        legacyCipher[i] ^= key[i % key.size()];
    }

    REQUIRE_EQ(utils::TokenCrypto::decrypt(legacyCipher), std::string(""));
}
