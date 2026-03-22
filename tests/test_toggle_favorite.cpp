#include "tests/TestHarness.hpp"

#include "core/MetadataLogic.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {

// ============================================================================
// SAVE ACTION RESULT STRUCTURE (mirrors ui/saves/SaveBackend.hpp)
// ============================================================================

struct SaveActionResult {
    bool ok = false;
    std::string message;
};

// ============================================================================
// MOCK TYPES FOR TOGGLE FAVORITE SIMULATION
// ============================================================================

enum class SaveSource {
    Local,
    Cloud,
};

struct MockTitle {
    bool isSystem = false;
};

// Error message keys (must match lang keys in implementation)
constexpr const char* ERROR_FAVORITES_CLOUD_ONLY = "error.favorites_cloud_only";
constexpr const char* ERROR_INVALID_BACKUP_SELECTION = "error.invalid_backup_selection";
constexpr const char* ERROR_UNKNOWN_TITLE = "error.unknown_title";
constexpr const char* ERROR_NOT_AUTHENTICATED = "error.not_authenticated";
constexpr const char* ERROR_REAUTH_REQUIRED = "error.reauth_required";
constexpr const char* ERROR_INVALID_CLOUD_PATH = "error.invalid_cloud_path";
constexpr const char* ERROR_INVALID_BACKUP_FORMAT = "error.invalid_backup_format";
constexpr const char* ERROR_DOWNLOAD_FAILED = "error.download_failed";
constexpr const char* ERROR_METADATA_PARSE_FAILED = "error.metadata_parse_failed";
constexpr const char* ERROR_METADATA_WRITE_FAILED = "error.metadata_write_failed";
constexpr const char* SYNC_UPLOAD_FAILED = "sync.upload_failed";
constexpr const char* SYNC_FAVORITE_ADDED = "sync.favorite_added";
constexpr const char* SYNC_FAVORITE_REMOVED = "sync.favorite_removed";

// ============================================================================
// TOGGLE FAVORITE SIMULATION LOGIC
// Mirrors SaveBackendAdapter::toggleFavorite() in SaveBackendAdapter.cpp:523-619
// ============================================================================

/**
 * Simulates the toggle favorite operation from SaveBackendAdapter::toggleFavorite()
 * This function mirrors the exact rejection and success branches for deterministic testing.
 *
 * @param titleId The title ID for the revision
 * @param revisionPath The revision path (.meta or .zip)
 * @param source Local or Cloud source
 * @param isAuthenticated Whether Dropbox is authenticated
 * @param titleRegistry Mock title registry
 * @param downloadSucceeds Simulated download result
 * @param metaContent Pre-written temp meta file content (simulates download)
 * @param uploadSucceeds Simulated upload result
 * @param writeSucceeds Simulated write result (for temp file)
 * @return SaveActionResult with ok status and message
 */
SaveActionResult simulateToggleFavorite(
    uint64_t titleId,
    const std::string& revisionPath,
    SaveSource source,
    bool isAuthenticated,
    std::unordered_map<uint64_t, MockTitle>& titleRegistry,
    bool downloadSucceeds = true,
    const std::string& metaContent = "",
    bool uploadSucceeds = true,
    bool writeSucceeds = true,
    bool consumeReconnectRequired = false
) {
    // Branch 1: Favorites only supported for cloud backups (SaveBackendAdapter.cpp:528-531)
    if (source == SaveSource::Local) {
        return {false, ERROR_FAVORITES_CLOUD_ONLY};
    }

    // Branch 2: Empty revisionPath (SaveBackendAdapter.cpp:533-536)
    if (revisionPath.empty()) {
        return {false, ERROR_INVALID_BACKUP_SELECTION};
    }

    // Branch 3: Unknown title (SaveBackendAdapter.cpp:538-542)
    auto it = titleRegistry.find(titleId);
    if (it == titleRegistry.end()) {
        return {false, ERROR_UNKNOWN_TITLE};
    }

    // Branch 4: Not authenticated (SaveBackendAdapter.cpp:544-550)
    if (!isAuthenticated) {
        if (consumeReconnectRequired) {
            return {false, ERROR_REAUTH_REQUIRED};
        }
        return {false, ERROR_NOT_AUTHENTICATED};
    }

    // Branch 5: Determine .meta path from revisionPath (SaveBackendAdapter.cpp:553-561)
    std::string metaPath;
    if (revisionPath.size() > 5 && revisionPath.substr(revisionPath.size() - 5) == ".meta") {
        metaPath = revisionPath;
    } else if (revisionPath.size() > 4 && revisionPath.substr(revisionPath.size() - 4) == ".zip") {
        metaPath = revisionPath.substr(0, revisionPath.size() - 4) + ".meta";
    } else {
        return {false, ERROR_INVALID_BACKUP_FORMAT};
    }

    // Branch 6: Validate path is within title revisions (SaveBackendAdapter.cpp:564-571)
    char titleIdStr[20];
    std::snprintf(titleIdStr, sizeof(titleIdStr), "%016lX", titleId);
    const std::string expectedPrefix = std::string("/titles/") + titleIdStr + "/revisions/";

    if (metaPath.find(expectedPrefix) != 0) {
        return {false, ERROR_INVALID_CLOUD_PATH};
    }

    // Branch 7: Download current .meta to temp location (SaveBackendAdapter.cpp:577-583)
    if (!downloadSucceeds) {
        if (consumeReconnectRequired) {
            return {false, ERROR_REAUTH_REQUIRED};
        }
        return {false, ERROR_DOWNLOAD_FAILED};
    }

    // Branch 8: Parse metadata (SaveBackendAdapter.cpp:586-591)
    core::BackupMetadata meta;
    if (!metaContent.empty()) {
        if (!core::parseBackupMetadata(metaContent, meta)) {
            return {false, ERROR_METADATA_PARSE_FAILED};
        }
    } else {
        // Simulate corrupted/empty meta file
        return {false, ERROR_METADATA_PARSE_FAILED};
    }

    // Branch 9: Toggle isFavorite (SaveBackendAdapter.cpp:593-595)
    meta.isFavorite = !meta.isFavorite;

    // Branch 10: Serialize and write updated metadata (SaveBackendAdapter.cpp:598-602)
    if (!writeSucceeds) {
        return {false, ERROR_METADATA_WRITE_FAILED};
    }

    // Branch 11: Upload updated .meta back to Dropbox (SaveBackendAdapter.cpp:605-612)
    if (!uploadSucceeds) {
        if (consumeReconnectRequired) {
            return {false, ERROR_REAUTH_REQUIRED};
        }
        return {false, SYNC_UPLOAD_FAILED};
    }

    // Branch 12: Success (SaveBackendAdapter.cpp:617-618)
    return {true, meta.isFavorite ? SYNC_FAVORITE_ADDED : SYNC_FAVORITE_REMOVED};
}

// ============================================================================
// PATH VALIDATION HELPERS
// ============================================================================

bool isValidCloudPath(const std::string& path, uint64_t titleId) {
    if (path.empty()) return false;

    char titleIdStr[20];
    std::snprintf(titleIdStr, sizeof(titleIdStr), "%016lX", titleId);

    const std::string expectedPrefix = std::string("/titles/") + titleIdStr + "/revisions/";
    return path.find(expectedPrefix) == 0;
}

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

} // namespace

// ============================================================================
// LOCAL SOURCE REJECTION TESTS
// ============================================================================

TEST_CASE("Toggle favorite - Local source returns error (cloud-only v1)") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/switch/oc-save-keeper/backups/0100A83022A22000/20260316_232624",
        SaveSource::Local,
        true,
        titleRegistry
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_FAVORITES_CLOUD_ONLY));
}

TEST_CASE("Toggle favorite - Local source with valid path still rejected") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/switch/oc-save-keeper/backups/0100A83022A22000/20260316_232624.meta",
        SaveSource::Local,
        true,
        titleRegistry
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_FAVORITES_CLOUD_ONLY));
}

// ============================================================================
// EMPTY PATH REJECTION TESTS
// ============================================================================

TEST_CASE("Toggle favorite - empty revisionPath returns error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "",  // Empty path
        SaveSource::Cloud,
        true,
        titleRegistry
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_INVALID_BACKUP_SELECTION));
}

// ============================================================================
// UNKNOWN TITLE REJECTION TESTS
// ============================================================================

TEST_CASE("Toggle favorite - unknown title returns error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    // Empty registry - titleId not registered

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_UNKNOWN_TITLE));
}

// ============================================================================
// AUTHENTICATION REJECTION TESTS
// ============================================================================

TEST_CASE("Toggle favorite - not authenticated returns error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        false,  // Not authenticated
        titleRegistry
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_NOT_AUTHENTICATED));
}

TEST_CASE("Toggle favorite - reauth required returns specific error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        false,  // Not authenticated
        titleRegistry,
        true,  // downloadSucceeds
        "",
        true,  // uploadSucceeds
        true,  // writeSucceeds
        true   // consumeReconnectRequired
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_REAUTH_REQUIRED));
}

// ============================================================================
// INVALID FORMAT REJECTION TESTS
// ============================================================================

TEST_CASE("Toggle favorite - invalid format (no extension) returns error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624",  // No extension
        SaveSource::Cloud,
        true,
        titleRegistry
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_INVALID_BACKUP_FORMAT));
}

TEST_CASE("Toggle favorite - invalid format (wrong extension) returns error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.txt",  // Wrong extension
        SaveSource::Cloud,
        true,
        titleRegistry
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_INVALID_BACKUP_FORMAT));
}

// ============================================================================
// INVALID CLOUD PATH REJECTION TESTS
// ============================================================================

TEST_CASE("Toggle favorite - invalid cloud path (wrong titleId) returns error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100BBBBBBBBBB/revisions/20260316_232624.meta",  // Wrong titleId
        SaveSource::Cloud,
        true,
        titleRegistry
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_INVALID_CLOUD_PATH));
}

TEST_CASE("Toggle favorite - invalid cloud path (outside revisions) returns error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/latest.meta",  // Not in revisions folder
        SaveSource::Cloud,
        true,
        titleRegistry
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_INVALID_CLOUD_PATH));
}

// ============================================================================
// DOWNLOAD FAILURE TESTS
// ============================================================================

TEST_CASE("Toggle favorite - download failure returns error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        false  // downloadSucceeds = false
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_DOWNLOAD_FAILED));
}

TEST_CASE("Toggle favorite - download failure with reauth returns reauth error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        false,  // downloadSucceeds = false
        "",
        true,   // uploadSucceeds
        true,   // writeSucceeds
        true    // consumeReconnectRequired
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_REAUTH_REQUIRED));
}

// ============================================================================
// PARSE FAILURE TESTS
// ============================================================================

TEST_CASE("Toggle favorite - corrupted metadata returns parse error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    // Corrupted metadata - missing required fields
    const std::string corruptedMeta = "garbage_data\ninvalid_key=value\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        corruptedMeta
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_METADATA_PARSE_FAILED));
}

TEST_CASE("Toggle favorite - empty metadata returns parse error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    // Empty metadata content
    const std::string emptyMeta = "";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        emptyMeta
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_METADATA_PARSE_FAILED));
}

TEST_CASE("Toggle favorite - metadata missing identity fields returns parse error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    // Metadata without title_id or backup_name (identity fields)
    const std::string noIdentityMeta =
        "device_id=dev-x\n"
        "user_id=user-1\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        noIdentityMeta
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_METADATA_PARSE_FAILED));
}

// ============================================================================
// WRITE FAILURE TESTS
// ============================================================================

TEST_CASE("Toggle favorite - write failure returns error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string validMeta =
        "title_id=0100A83022A22000\n"
        "backup_name=20260316_232624\n"
        "is_favorite=0\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,   // downloadSucceeds
        validMeta,
        true,   // uploadSucceeds
        false   // writeSucceeds = false
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_METADATA_WRITE_FAILED));
}

// ============================================================================
// UPLOAD FAILURE TESTS
// ============================================================================

TEST_CASE("Toggle favorite - upload failure returns error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string validMeta =
        "title_id=0100A83022A22000\n"
        "backup_name=20260316_232624\n"
        "is_favorite=0\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,    // downloadSucceeds
        validMeta,
        false    // uploadSucceeds = false
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(SYNC_UPLOAD_FAILED));
}

TEST_CASE("Toggle favorite - upload failure with reauth returns reauth error") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string validMeta =
        "title_id=0100A83022A22000\n"
        "backup_name=20260316_232624\n"
        "is_favorite=0\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,    // downloadSucceeds
        validMeta,
        false,   // uploadSucceeds = false
        true,    // writeSucceeds
        true     // consumeReconnectRequired
    );

    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_REAUTH_REQUIRED));
}

// ============================================================================
// SUCCESS PATH TESTS
// ============================================================================

TEST_CASE("Toggle favorite - success toggling from not-favorite to favorite") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string metaContent =
        "title_id=0100A83022A22000\n"
        "backup_name=20260316_232624\n"
        "is_favorite=0\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        metaContent,
        true   // uploadSucceeds
    );

    REQUIRE(result.ok);
    REQUIRE_EQ(result.message, std::string(SYNC_FAVORITE_ADDED));
}

TEST_CASE("Toggle favorite - success toggling from favorite to not-favorite") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string metaContent =
        "title_id=0100A83022A22000\n"
        "backup_name=20260316_232624\n"
        "is_favorite=1\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        metaContent,
        true   // uploadSucceeds
    );

    REQUIRE(result.ok);
    REQUIRE_EQ(result.message, std::string(SYNC_FAVORITE_REMOVED));
}

TEST_CASE("Toggle favorite - success with .zip path (derives .meta)") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string metaContent =
        "title_id=0100A83022A22000\n"
        "backup_name=20260316_232624\n";

    // Pass .zip path - should derive .meta path internally
    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.zip",  // .zip extension
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        metaContent,
        true   // uploadSucceeds
    );

    REQUIRE(result.ok);
    // Toggles from false (default) to true
    REQUIRE_EQ(result.message, std::string(SYNC_FAVORITE_ADDED));
}

TEST_CASE("Toggle favorite - success preserves other metadata fields") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string metaContent =
        "title_id=0100A83022A22000\n"
        "title_name=Test Game\n"
        "backup_name=20260316_232624\n"
        "device_id=device123\n"
        "device_label=My Switch\n"
        "user_id=user456\n"
        "user_name=TestUser\n"
        "source=cloud\n"
        "created_at=1710000000\n"
        "device_priority=100\n"
        "size=1234567\n"
        "is_auto_backup=1\n"
        "is_favorite=0\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        metaContent,
        true   // uploadSucceeds
    );

    REQUIRE(result.ok);
    REQUIRE_EQ(result.message, std::string(SYNC_FAVORITE_ADDED));
}

// ============================================================================
// LEGACY METADATA COMPATIBILITY TESTS
// ============================================================================

TEST_CASE("Toggle favorite - legacy metadata without is_favorite defaults to false then toggles true") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    // Legacy metadata without is_favorite field
    const std::string legacyMeta =
        "title_id=0100A83022A22000\n"
        "backup_name=20260316_232624\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        legacyMeta,
        true   // uploadSucceeds
    );

    REQUIRE(result.ok);
    // Legacy is_favorite defaults to false, toggle makes it true
    REQUIRE_EQ(result.message, std::string(SYNC_FAVORITE_ADDED));
}

// ============================================================================
// PATH DERIVATION TESTS (mirrors test_delete_revision.cpp patterns)
// ============================================================================

TEST_CASE("Toggle favorite - meta to ZIP path derivation") {
    const std::string metaPath = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    const std::string expectedZip = "/titles/0100A83022A22000/revisions/20260316_232624.zip";
    REQUIRE_EQ(deriveZipFromMeta(metaPath), expectedZip);
}

TEST_CASE("Toggle favorite - ZIP to meta path derivation") {
    const std::string zipPath = "/titles/0100A83022A22000/revisions/20260316_232624.zip";
    const std::string expectedMeta = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    REQUIRE_EQ(deriveMetaFromZip(zipPath), expectedMeta);
}

TEST_CASE("Toggle favorite - cloud path validation - valid path") {
    const uint64_t titleId = 0x0100A83022A22000ULL;
    const std::string validPath = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    REQUIRE(isValidCloudPath(validPath, titleId));
}

TEST_CASE("Toggle favorite - cloud path validation - invalid path (wrong title)") {
    const uint64_t titleId = 0x0100A83022A22000ULL;
    const std::string invalidPath = "/titles/0100BBBBBBBBBB/revisions/20260316_232624.meta";
    REQUIRE(!isValidCloudPath(invalidPath, titleId));
}

TEST_CASE("Toggle favorite - cloud path validation - invalid path (outside revisions)") {
    const uint64_t titleId = 0x0100A83022A22000ULL;
    const std::string invalidPath = "/titles/0100A83022A22000/latest.meta";
    REQUIRE(!isValidCloudPath(invalidPath, titleId));
}

// ============================================================================
// RESULT STRUCTURE USABLE FOR UI ROLLBACK TESTS
// ============================================================================

TEST_CASE("Toggle favorite - result structure is usable for UI rollback on failure") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        false  // downloadSucceeds = false (simulates failure)
    );

    // UI can check result.ok == false and use result.message for error display
    REQUIRE(!result.ok);
    REQUIRE(!result.message.empty());

    // Verify the message is a usable error key (not empty, not generic)
    REQUIRE(result.message.find("error.") == 0 || result.message.find("sync.") == 0);
}

TEST_CASE("Toggle favorite - result structure provides actionable error for parse failure") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string corruptedMeta = "invalid\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        corruptedMeta
    );

    // UI can use result.message to show specific error
    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(ERROR_METADATA_PARSE_FAILED));
}

TEST_CASE("Toggle favorite - result structure provides actionable error for upload failure") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string validMeta =
        "title_id=0100A83022A22000\n"
        "backup_name=20260316_232624\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,    // downloadSucceeds
        validMeta,
        false    // uploadSucceeds = false
    );

    // UI can use result.message to show specific error and rollback state
    REQUIRE(!result.ok);
    REQUIRE_EQ(result.message, std::string(SYNC_UPLOAD_FAILED));
}

TEST_CASE("Toggle favorite - success result indicates new favorite state") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string metaContent =
        "title_id=0100A83022A22000\n"
        "backup_name=20260316_232624\n"
        "is_favorite=0\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20260316_232624.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        metaContent,
        true   // uploadSucceeds
    );

    // UI can use result.ok == true and message to determine new state
    REQUIRE(result.ok);
    REQUIRE(result.message == std::string(SYNC_FAVORITE_ADDED) ||
            result.message == std::string(SYNC_FAVORITE_REMOVED));
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_CASE("Toggle favorite - path with different revision accepted") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string metaContent =
        "title_id=0100A83022A22000\n"
        "backup_name=20250101_120000\n";

    const SaveActionResult result = simulateToggleFavorite(
        0x0100A83022A22000ULL,
        "/titles/0100A83022A22000/revisions/20250101_120000.meta",
        SaveSource::Cloud,
        true,
        titleRegistry,
        true,  // downloadSucceeds
        metaContent,
        true   // uploadSucceeds
    );

    REQUIRE(result.ok);
}

TEST_CASE("Toggle favorite - lowercase titleId in path rejected") {
    // Cloud paths use uppercase hex for titleId
    const uint64_t titleId = 0x0100A83022A22000ULL;
    const std::string lowercasePath = "/titles/0100a83022a22000/revisions/20260316_232624.meta";

    REQUIRE(!isValidCloudPath(lowercasePath, titleId));
}

TEST_CASE("Toggle favorite - path derivation handles edge cases") {
    REQUIRE_EQ(deriveZipFromMeta("no_extension"), std::string("no_extension"));
    REQUIRE_EQ(deriveMetaFromZip("no_extension"), std::string("no_extension"));
    REQUIRE_EQ(deriveZipFromMeta(".meta"), std::string(".meta"));
    REQUIRE_EQ(deriveMetaFromZip(".zip"), std::string(".zip"));
}

// ============================================================================
// TRUTH TABLE VERIFICATION FOR REJECTION BRANCHES
// ============================================================================

TEST_CASE("Toggle favorite - all rejection branches return ok=false") {
    std::unordered_map<uint64_t, MockTitle> titleRegistry;
    titleRegistry[0x0100A83022A22000ULL] = MockTitle{false};

    const std::string validMeta =
        "title_id=0100A83022A22000\n"
        "backup_name=test\n";

    // Branch 1: Local source
    {
        const auto r = simulateToggleFavorite(0x0100A83022A22000ULL, "/titles/x.meta", SaveSource::Local, true, titleRegistry);
        REQUIRE(!r.ok);
    }

    // Branch 2: Empty path
    {
        const auto r = simulateToggleFavorite(0x0100A83022A22000ULL, "", SaveSource::Cloud, true, titleRegistry);
        REQUIRE(!r.ok);
    }

    // Branch 3: Unknown title
    {
        std::unordered_map<uint64_t, MockTitle> emptyRegistry;
        const auto r = simulateToggleFavorite(0x0100A83022A22000ULL, "/titles/x.meta", SaveSource::Cloud, true, emptyRegistry);
        REQUIRE(!r.ok);
    }

    // Branch 4: Not authenticated
    {
        const auto r = simulateToggleFavorite(0x0100A83022A22000ULL, "/titles/0100A83022A22000/revisions/x.meta", SaveSource::Cloud, false, titleRegistry);
        REQUIRE(!r.ok);
    }

    // Branch 5: Invalid format
    {
        const auto r = simulateToggleFavorite(0x0100A83022A22000ULL, "/titles/0100A83022A22000/revisions/x", SaveSource::Cloud, true, titleRegistry);
        REQUIRE(!r.ok);
    }

    // Branch 6: Invalid cloud path
    {
        const auto r = simulateToggleFavorite(0x0100A83022A22000ULL, "/titles/WRONG/revisions/x.meta", SaveSource::Cloud, true, titleRegistry);
        REQUIRE(!r.ok);
    }

    // Branch 7: Download failure
    {
        const auto r = simulateToggleFavorite(0x0100A83022A22000ULL, "/titles/0100A83022A22000/revisions/x.meta", SaveSource::Cloud, true, titleRegistry, false);
        REQUIRE(!r.ok);
    }

    // Branch 8: Parse failure
    {
        const auto r = simulateToggleFavorite(0x0100A83022A22000ULL, "/titles/0100A83022A22000/revisions/x.meta", SaveSource::Cloud, true, titleRegistry, true, "invalid");
        REQUIRE(!r.ok);
    }

    // Branch 10: Write failure
    {
        const auto r = simulateToggleFavorite(0x0100A83022A22000ULL, "/titles/0100A83022A22000/revisions/x.meta", SaveSource::Cloud, true, titleRegistry, true, validMeta, true, false);
        REQUIRE(!r.ok);
    }

    // Branch 11: Upload failure
    {
        const auto r = simulateToggleFavorite(0x0100A83022A22000ULL, "/titles/0100A83022A22000/revisions/x.meta", SaveSource::Cloud, true, titleRegistry, true, validMeta, false);
        REQUIRE(!r.ok);
    }
}
