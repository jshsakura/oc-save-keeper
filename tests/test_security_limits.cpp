#include "tests/TestHarness.hpp"
#include "network/Dropbox.hpp"
#include "zip/ZipArchive.hpp"

TEST_CASE("Network security limits are correctly defined") {
    // Verify that our proactive defense limits are set as planned
    REQUIRE_EQ(network::Dropbox::MAX_JSON_RESPONSE_SIZE, 10 * 1024 * 1024);
    REQUIRE_EQ(network::Dropbox::MAX_DOWNLOAD_FILE_SIZE, 512 * 1024 * 1024);
}

TEST_CASE("Zip security limits are correctly defined") {
    // Verify Zip Bomb defense constants
    REQUIRE_EQ(zip::MAX_TOTAL_UNCOMPRESSED_SIZE, 512ULL * 1024 * 1024);
    REQUIRE_EQ(zip::MAX_COMPRESSION_RATIO, 100U);
}
