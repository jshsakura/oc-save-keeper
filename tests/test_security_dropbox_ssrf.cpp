#include "tests/TestHarness.hpp"

#include "network/DropboxUtil.hpp"

TEST_CASE("Dropbox URL prefix validation accepts matching prefix") {
    REQUIRE_EQ(network::dropbox::isUrlPrefixValid(
        "https://bridge.example.com/v1/sessions/abc/status",
        "https://bridge.example.com"), true);
    REQUIRE_EQ(network::dropbox::isUrlPrefixValid(
        "https://bridge.example.com/",
        "https://bridge.example.com"), true);
}

TEST_CASE("Dropbox URL prefix validation rejects non-matching prefix") {
    REQUIRE_EQ(network::dropbox::isUrlPrefixValid(
        "https://evil.com/v1/sessions/abc/status",
        "https://bridge.example.com"), false);
    REQUIRE_EQ(network::dropbox::isUrlPrefixValid(
        "http://bridge.example.com/v1/sessions",
        "https://bridge.example.com"), false);
    REQUIRE_EQ(network::dropbox::isUrlPrefixValid(
        "https://bridge.example.com.evil.com/v1",
        "https://bridge.example.com"), false);
}

TEST_CASE("Dropbox URL prefix validation rejects empty inputs") {
    REQUIRE_EQ(network::dropbox::isUrlPrefixValid("", "https://bridge.example.com"), false);
    REQUIRE_EQ(network::dropbox::isUrlPrefixValid("https://bridge.example.com/v1", ""), false);
    REQUIRE_EQ(network::dropbox::isUrlPrefixValid("", ""), false);
}

TEST_CASE("Dropbox buildSafePollUrl uses valid pollUrl when prefix matches") {
    const std::string bridgeBase = "https://bridge.example.com";
    const std::string sessionId = "session-123";
    const std::string validPollUrl = "https://bridge.example.com/v1/sessions/session-123/status";
    
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(validPollUrl, bridgeBase, sessionId), 
               validPollUrl);
}

TEST_CASE("Dropbox buildSafePollUrl uses default when pollUrl is empty") {
    const std::string bridgeBase = "https://bridge.example.com";
    const std::string sessionId = "session-123";
    const std::string expectedDefault = "https://bridge.example.com/v1/sessions/session-123/status";
    
    REQUIRE_EQ(network::dropbox::buildSafePollUrl("", bridgeBase, sessionId), 
               expectedDefault);
}

TEST_CASE("Dropbox buildSafePollUrl uses default when pollUrl has wrong prefix") {
    const std::string bridgeBase = "https://bridge.example.com";
    const std::string sessionId = "session-123";
    const std::string maliciousUrl = "https://evil.com/v1/sessions/session-123/status";
    const std::string expectedDefault = "https://bridge.example.com/v1/sessions/session-123/status";
    
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(maliciousUrl, bridgeBase, sessionId), 
               expectedDefault);
}

TEST_CASE("Dropbox buildSafePollUrl rejects SSRF attack attempts") {
    const std::string bridgeBase = "https://bridge.example.com";
    const std::string sessionId = "session-123";
    const std::string expectedDefault = "https://bridge.example.com/v1/sessions/session-123/status";
    
    // Internal network SSRF attempts
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(
        "http://localhost/admin", bridgeBase, sessionId), expectedDefault);
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(
        "http://127.0.0.1/admin", bridgeBase, sessionId), expectedDefault);
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(
        "http://192.168.1.1/admin", bridgeBase, sessionId), expectedDefault);
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(
        "http://10.0.0.1/admin", bridgeBase, sessionId), expectedDefault);
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(
        "http://169.254.169.254/latest/meta-data", bridgeBase, sessionId), expectedDefault);
    
    // Protocol downgrade attack
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(
        "http://bridge.example.com/v1/sessions/session-123/status", 
        bridgeBase, sessionId), expectedDefault);
    
    // Subdomain spoofing
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(
        "https://bridge.example.com.evil.com/v1", 
        bridgeBase, sessionId), expectedDefault);
}

TEST_CASE("Dropbox buildSafePollUrl handles edge cases") {
    const std::string bridgeBase = "https://bridge.example.com";
    const std::string sessionId = "session-123";
    const std::string expectedDefault = "https://bridge.example.com/v1/sessions/session-123/status";
    
    // Similar but different domain
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(
        "https://bridge.example.org/v1/sessions/session-123/status", 
        bridgeBase, sessionId), expectedDefault);
    
    // Missing protocol
    REQUIRE_EQ(network::dropbox::buildSafePollUrl(
        "bridge.example.com/v1/sessions/session-123/status", 
        bridgeBase, sessionId), expectedDefault);
}
