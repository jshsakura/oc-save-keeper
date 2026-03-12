#include "tests/TestHarness.hpp"

#include "network/DropboxUtil.hpp"

#include <string>

TEST_CASE("Dropbox authorize URL includes required parameters") {
    const std::string url = network::dropbox::buildAuthorizeUrl(
        "https://www.dropbox.com/oauth2/authorize",
        "client-key",
        "https://example.com/complete",
        "csrf-token",
        "challenge123"
    );

    REQUIRE(url.find("https://www.dropbox.com/oauth2/authorize?") == 0);
    REQUIRE(url.find("client_id=client-key") != std::string::npos);
    REQUIRE(url.find("response_type=code") != std::string::npos);
    REQUIRE(url.find("token_access_type=offline") != std::string::npos);
    REQUIRE(url.find("code_challenge_method=S256") != std::string::npos);
    REQUIRE(url.find("code_challenge=challenge123") != std::string::npos);
    REQUIRE(url.find("redirect_uri=https%3A%2F%2Fexample.com%2Fcomplete") != std::string::npos);
    REQUIRE(url.find("state=csrf-token") != std::string::npos);
}

TEST_CASE("Dropbox authorize URL percent-encodes reserved characters") {
    const std::string url = network::dropbox::buildAuthorizeUrl(
        "https://www.dropbox.com/oauth2/authorize",
        "client key",
        "https://example.com/complete?from=switch",
        "csrf token",
        "challenge+/="
    );

    REQUIRE(url.find("client_id=client%20key") != std::string::npos);
    REQUIRE(url.find("redirect_uri=https%3A%2F%2Fexample.com%2Fcomplete%3Ffrom%3Dswitch") != std::string::npos);
    REQUIRE(url.find("state=csrf%20token") != std::string::npos);
    REQUIRE(url.find("code_challenge=challenge%2B%2F%3D") != std::string::npos);
}

TEST_CASE("Dropbox time parser accepts valid RFC3339-like timestamps") {
    const std::time_t parsed = network::dropbox::parseDropboxTime("2026-03-11T12:34:56Z");
    REQUIRE(parsed > 0);
}

TEST_CASE("Dropbox time parser rejects invalid timestamps") {
    REQUIRE_EQ(network::dropbox::parseDropboxTime(nullptr), static_cast<std::time_t>(0));
    REQUIRE_EQ(network::dropbox::parseDropboxTime(""), static_cast<std::time_t>(0));
    REQUIRE_EQ(network::dropbox::parseDropboxTime("not-a-time"), static_cast<std::time_t>(0));
}

TEST_CASE("Dropbox list_folder request payload is stable") {
    const std::string payload = network::dropbox::buildListFolderRequest("/apps/oc-save-keeper");
    REQUIRE_EQ(payload, std::string("{\"path\":\"/apps/oc-save-keeper\",\"recursive\":false,\"include_deleted\":false}"));
    REQUIRE_EQ(network::dropbox::buildListFolderRequest(""), std::string("{\"path\":\"\",\"recursive\":false,\"include_deleted\":false}"));
    REQUIRE_EQ(network::dropbox::buildListFolderRequest("/"), std::string("{\"path\":\"/\",\"recursive\":false,\"include_deleted\":false}"));
    REQUIRE_EQ(network::dropbox::buildListFolderRequest("/games/Quote\"Test"),
               std::string("{\"path\":\"/games/Quote\\\"Test\",\"recursive\":false,\"include_deleted\":false}"));
}

TEST_CASE("Dropbox path helpers split folder and filename correctly") {
    REQUIRE_EQ(network::dropbox::parentPath("/games/Zelda/latest.zip"), std::string("/games/Zelda"));
    REQUIRE_EQ(network::dropbox::fileName("/games/Zelda/latest.zip"), std::string("latest.zip"));
    REQUIRE_EQ(network::dropbox::parentPath("latest.zip"), std::string(""));
    REQUIRE_EQ(network::dropbox::fileName("latest.zip"), std::string("latest.zip"));
    REQUIRE_EQ(network::dropbox::parentPath("/latest.zip"), std::string("/"));
    REQUIRE_EQ(network::dropbox::fileName("/latest.zip"), std::string("latest.zip"));
    REQUIRE_EQ(network::dropbox::parentPath("/games/Zelda/"), std::string("/games/Zelda"));
    REQUIRE_EQ(network::dropbox::fileName("/games/Zelda/"), std::string(""));
}

TEST_CASE("Dropbox request helpers escape quoted paths consistently") {
    const std::string path = "/games/Quote\"Test";
    REQUIRE_EQ(network::dropbox::buildUploadArg(path),
               std::string("{\"path\":\"/games/Quote\\\"Test\",\"mode\":\"overwrite\",\"autorename\":false}"));
    REQUIRE_EQ(network::dropbox::buildDownloadArg(path),
               std::string("{\"path\":\"/games/Quote\\\"Test\"}"));
    REQUIRE_EQ(network::dropbox::buildCreateFolderRequest(path),
               std::string("{\"path\":\"/games/Quote\\\"Test\"}"));
    REQUIRE_EQ(network::dropbox::buildDeleteRequest(path),
               std::string("{\"path\":\"/games/Quote\\\"Test\"}"));
}

TEST_CASE("Dropbox fileExists path guard rejects empty file components") {
    REQUIRE_EQ(network::dropbox::hasFileComponent(""), false);
    REQUIRE_EQ(network::dropbox::hasFileComponent("/"), false);
    REQUIRE_EQ(network::dropbox::hasFileComponent("/games/Zelda/"), false);
    REQUIRE_EQ(network::dropbox::hasFileComponent("/games/Zelda/latest.zip"), true);
}

TEST_CASE("Dropbox HTTP status helper accepts only 2xx") {
    REQUIRE_EQ(network::dropbox::isSuccessfulHttpStatus(200), true);
    REQUIRE_EQ(network::dropbox::isSuccessfulHttpStatus(204), true);
    REQUIRE_EQ(network::dropbox::isSuccessfulHttpStatus(299), true);
    REQUIRE_EQ(network::dropbox::isSuccessfulHttpStatus(199), false);
    REQUIRE_EQ(network::dropbox::isSuccessfulHttpStatus(300), false);
    REQUIRE_EQ(network::dropbox::isSuccessfulHttpStatus(401), false);
    REQUIRE_EQ(network::dropbox::isSuccessfulHttpStatus(500), false);
}

TEST_CASE("Dropbox PKCE code verifier generates valid length and charset") {
    const std::string v43 = network::dropbox::generateCodeVerifier(43);
    REQUIRE_EQ(v43.size(), static_cast<std::size_t>(43));

    const std::string v128 = network::dropbox::generateCodeVerifier(128);
    REQUIRE_EQ(v128.size(), static_cast<std::size_t>(128));

    const std::string v64 = network::dropbox::generateCodeVerifier(64);
    REQUIRE_EQ(v64.size(), static_cast<std::size_t>(64));

    const std::string vTooSmall = network::dropbox::generateCodeVerifier(10);
    REQUIRE_EQ(vTooSmall.size(), static_cast<std::size_t>(43));

    const std::string vTooLarge = network::dropbox::generateCodeVerifier(200);
    REQUIRE_EQ(vTooLarge.size(), static_cast<std::size_t>(128));

    for (char ch : v64) {
        const bool isUpper = (ch >= 'A' && ch <= 'Z');
        const bool isLower = (ch >= 'a' && ch <= 'z');
        const bool isDigit = (ch >= '0' && ch <= '9');
        const bool isSpecial = (ch == '-' || ch == '.' || ch == '_' || ch == '~');
        REQUIRE(isUpper || isLower || isDigit || isSpecial);
    }
}

TEST_CASE("Dropbox base64url encoding removes padding and uses URL-safe chars") {
    std::vector<unsigned char> data = {0x03, 0x9f, 0xd7, 0x2e, 0xb3};
    const std::string encoded = network::dropbox::base64UrlEncode(data);
    REQUIRE(encoded.find('+') == std::string::npos);
    REQUIRE(encoded.find('/') == std::string::npos);
    REQUIRE(encoded.find('=') == std::string::npos);
    REQUIRE(encoded.find('-') != std::string::npos || encoded.find('_') != std::string::npos || !encoded.empty());
}

TEST_CASE("Dropbox PKCE challenge is deterministic for same verifier") {
    const std::string verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
    const std::string c1 = network::dropbox::buildCodeChallengeS256(verifier);
    const std::string c2 = network::dropbox::buildCodeChallengeS256(verifier);
    REQUIRE_EQ(c1, c2);
    REQUIRE(!c1.empty());
}

TEST_CASE("Dropbox extractQueryParam finds values correctly") {
    REQUIRE_EQ(network::dropbox::extractQueryParam("code=ABC123&state=xyz", "code"), std::string("ABC123"));
    REQUIRE_EQ(network::dropbox::extractQueryParam("code=ABC123&state=xyz", "state"), std::string("xyz"));
    REQUIRE_EQ(network::dropbox::extractQueryParam("code=ABC123#", "code"), std::string("ABC123"));
    REQUIRE_EQ(network::dropbox::extractQueryParam("https://example.com/complete?code=ABC%20123&state=xyz%20value", "code"), std::string("ABC 123"));
    REQUIRE_EQ(network::dropbox::extractQueryParam("https://example.com/complete?code=ABC123&state=xyz", "state"), std::string("xyz"));
    REQUIRE_EQ(network::dropbox::extractQueryParam("foo=bar", "missing"), std::string(""));
    REQUIRE_EQ(network::dropbox::extractQueryParam("", "code"), std::string(""));
}

TEST_CASE("Dropbox extractAuthorizationCode prefers code param, falls back to raw input") {
    REQUIRE_EQ(network::dropbox::extractAuthorizationCode("code=XYZ789&state=abc"), std::string("XYZ789"));
    REQUIRE_EQ(network::dropbox::extractAuthorizationCode("https://example.com/complete?code=XYZ%20789&state=abc"), std::string("XYZ 789"));
    REQUIRE_EQ(network::dropbox::extractAuthorizationCode("plain-code-value"), std::string("plain-code-value"));
    REQUIRE_EQ(network::dropbox::extractAuthorizationCode(" plain-code-value "), std::string("plain-code-value"));
    REQUIRE_EQ(network::dropbox::extractAuthorizationCode(""), std::string(""));
    REQUIRE_EQ(network::dropbox::extractAuthorizationCode("state=abc&code=FIRST"), std::string("FIRST"));
    REQUIRE_EQ(network::dropbox::extractAuthorizationCode("state=abc"), std::string(""));
}
