#include "tests/TestHarness.hpp"

#include <string>

namespace {

bool shouldSkipSafetyRollbackForRestorePath(const std::string& backupPath) {
    if (backupPath.empty()) {
        return false;
    }

    const size_t slash = backupPath.find_last_of('/');
    const std::string entryName = (slash == std::string::npos) ? backupPath : backupPath.substr(slash + 1);
    const std::string suffix = "_autosave";
    return entryName.size() > suffix.size() && entryName.compare(entryName.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool shouldCreateRollbackForRestore(const std::string& backupPath, bool createSafetyRollbackFlag) {
    return createSafetyRollbackFlag && !shouldSkipSafetyRollbackForRestorePath(backupPath);
}

}

TEST_CASE("Restore rollback skip guard matches autosave import path") {
    const std::string autosavePath = "/switch/oc-save-keeper/backups/0100A83022A22000/20260316_232624_autosave";
    REQUIRE(shouldSkipSafetyRollbackForRestorePath(autosavePath));
}

TEST_CASE("Restore rollback skip guard does not trigger for normal backup path") {
    const std::string normalPath = "/switch/oc-save-keeper/backups/0100A83022A22000/20260316_232624";
    REQUIRE(!shouldSkipSafetyRollbackForRestorePath(normalPath));
}

TEST_CASE("Restore rollback skip guard only checks backup entry basename") {
    const std::string parentWithSuffix = "/switch/oc-save-keeper/backups/0100A83022A22000_autosave/20260316_232624";
    REQUIRE(!shouldSkipSafetyRollbackForRestorePath(parentWithSuffix));
}

TEST_CASE("Cloud import restore must skip duplicate pre-restore rollback") {
    const std::string importAutosavePath = "/switch/oc-save-keeper/backups/0100A83022A22000/20260316_232624_autosave";
    REQUIRE(!shouldCreateRollbackForRestore(importAutosavePath, true));
}

TEST_CASE("Local restore keeps safety rollback creation enabled") {
    const std::string localBackupPath = "/switch/oc-save-keeper/backups/0100A83022A22000/20260316_232624";
    REQUIRE(shouldCreateRollbackForRestore(localBackupPath, true));
}
