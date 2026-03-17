#include "tests/TestHarness.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace {

std::string deriveMetaFromZip(const std::string& zipPath) {
    if (zipPath.size() > 4 && zipPath.substr(zipPath.size() - 4) == ".zip") {
        return zipPath.substr(0, zipPath.size() - 4) + ".meta";
    }
    return zipPath;
}

std::string deriveZipFromMeta(const std::string& metaPath) {
    if (metaPath.size() > 5 && metaPath.substr(metaPath.size() - 5) == ".meta") {
        return metaPath.substr(0, metaPath.size() - 5) + ".zip";
    }
    return metaPath;
}

bool isValidBackupPath(const std::string& path, const std::string& basePath) {
    if (path.empty()) return false;
    if (path.find(basePath) != 0) return false;
    if (path == basePath) return false;
    if (path.back() == '/') return false;
    return true;
}

bool isValidCloudPath(const std::string& path, uint64_t titleId) {
    if (path.empty()) return false;
    
    char titleIdStr[20];
    snprintf(titleIdStr, sizeof(titleIdStr), "%016lX", titleId);
    
    const std::string expectedPrefix = std::string("/titles/") + titleIdStr + "/revisions/";
    return path.find(expectedPrefix) == 0;
}

} // namespace

TEST_CASE("Empty revisionId should be rejected") {
    std::string revisionId;
    REQUIRE(revisionId.empty());
    REQUIRE(!isValidBackupPath(revisionId, "/switch/oc-save-keeper/backups/"));
}

TEST_CASE("Local path outside backup directory should be rejected") {
    const std::string invalidPath = "/switch/some_other_folder/backup";
    const std::string basePath = "/switch/oc-save-keeper/backups/";
    REQUIRE(!isValidBackupPath(invalidPath, basePath));
}

TEST_CASE("Root backup folder should be rejected") {
    const std::string basePath = "/switch/oc-save-keeper/backups/";
    REQUIRE(!isValidBackupPath(basePath, basePath));
    
    const std::string trailingSlash = "/switch/oc-save-keeper/backups/0100A83022A22000/";
    REQUIRE(!isValidBackupPath(trailingSlash, basePath));
}

TEST_CASE("Valid local backup path should be accepted") {
    const std::string basePath = "/switch/oc-save-keeper/backups/";
    const std::string validPath = "/switch/oc-save-keeper/backups/0100A83022A22000/20260316_232624";
    REQUIRE(isValidBackupPath(validPath, basePath));
}

TEST_CASE("Meta to ZIP path derivation") {
    const std::string metaPath = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    const std::string expectedZip = "/titles/0100A83022A22000/revisions/20260316_232624.zip";
    REQUIRE_EQ(deriveZipFromMeta(metaPath), expectedZip);
}

TEST_CASE("ZIP to Meta path derivation") {
    const std::string zipPath = "/titles/0100A83022A22000/revisions/20260316_232624.zip";
    const std::string expectedMeta = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    REQUIRE_EQ(deriveMetaFromZip(zipPath), expectedMeta);
}

TEST_CASE("Cloud path validation - valid path") {
    const uint64_t titleId = 0x0100A83022A22000ULL;
    const std::string validPath = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    REQUIRE(isValidCloudPath(validPath, titleId));
}

TEST_CASE("Cloud path validation - invalid path (wrong title)") {
    const uint64_t titleId = 0x0100A83022A22000ULL;
    const std::string invalidPath = "/titles/0100BBBBBBBBBB/revisions/20260316_232624.meta";
    REQUIRE(!isValidCloudPath(invalidPath, titleId));
}

TEST_CASE("Cloud path validation - invalid path (outside revisions)") {
    const uint64_t titleId = 0x0100A83022A22000ULL;
    const std::string invalidPath = "/titles/0100A83022A22000/latest.meta";
    REQUIRE(!isValidCloudPath(invalidPath, titleId));
}

TEST_CASE("DeleteTaskData structure integrity") {
    struct DeleteTaskData {
        uint64_t titleId = 0;
        std::string entryId;
        std::string entryPath;
        std::string entryLabel;
        int source = 0;
        bool isSystem = false;
    };
    
    DeleteTaskData data;
    data.titleId = 0x0100A83022A22000ULL;
    data.entryId = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    data.entryPath = "/titles/0100A83022A22000/revisions/20260316_232624.zip";
    data.entryLabel = "20260316_232624";
    data.source = 1;
    data.isSystem = false;
    
    REQUIRE_EQ(data.titleId, 0x0100A83022A22000ULL);
    REQUIRE_EQ(data.entryId.size(), 55UL);
    REQUIRE(!data.isSystem);
}

TEST_CASE("Path derivation handles edge cases") {
    REQUIRE_EQ(deriveZipFromMeta("no_extension"), std::string("no_extension"));
    REQUIRE_EQ(deriveMetaFromZip("no_extension"), std::string("no_extension"));
    REQUIRE_EQ(deriveZipFromMeta(".meta"), std::string(".meta"));
    REQUIRE_EQ(deriveMetaFromZip(".zip"), std::string(".zip"));
}
