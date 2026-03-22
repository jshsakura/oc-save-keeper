#include "tests/TestHarness.hpp"

#include "core/MetadataLogic.hpp"

#include <string>

namespace {

core::BackupMetadata makeMeta() {
    core::BackupMetadata meta;
    meta.titleId = 0x01006A800016E000ULL;
    meta.titleName = "The Legend of Zelda: BOTW";
    meta.backupName = "20260311_090000";
    meta.revisionId = "rev-42";
    meta.deviceId = "dev-abc";
    meta.deviceLabel = "OLED Main";
    meta.userId = "user-1";
    meta.userName = "TestUser";
    meta.source = "cloud";
    meta.createdAt = static_cast<std::time_t>(1710000000);
    meta.devicePriority = 200;
    meta.size = 123456;
    meta.isAutoBackup = false;
    return meta;
}

core::BackupMetadata makeAutoBackupMeta() {
    core::BackupMetadata meta = makeMeta();
    meta.backupName = "auto_pre_restore_20260311_100000";
    meta.isAutoBackup = true;
    return meta;
}

}

TEST_CASE("Metadata logic round-trips serialized metadata") {
    const core::BackupMetadata input = makeMeta();
    const std::string text = core::serializeBackupMetadata(input);

    core::BackupMetadata parsed;
    REQUIRE(core::parseBackupMetadata(text, parsed));
    REQUIRE_EQ(parsed.titleId, input.titleId);
    REQUIRE_EQ(parsed.titleName, input.titleName);
    REQUIRE_EQ(parsed.backupName, input.backupName);
    REQUIRE_EQ(parsed.revisionId, input.revisionId);
    REQUIRE_EQ(parsed.deviceId, input.deviceId);
    REQUIRE_EQ(parsed.deviceLabel, input.deviceLabel);
    REQUIRE_EQ(parsed.userId, input.userId);
    REQUIRE_EQ(parsed.userName, input.userName);
    REQUIRE_EQ(parsed.source, input.source);
    REQUIRE_EQ(parsed.createdAt, input.createdAt);
    REQUIRE_EQ(parsed.devicePriority, input.devicePriority);
    REQUIRE_EQ(parsed.size, input.size);
}

TEST_CASE("Metadata logic tolerates unknown keys and malformed lines") {
    const std::string text =
        "title_id=42\n"
        "bad_line_without_delimiter\n"
        "backup_name=slot-a\n"
        "unknown_field=ignore-me\n"
        "user_id=user-1\n";

    core::BackupMetadata parsed;
    REQUIRE(core::parseBackupMetadata(text, parsed));
    REQUIRE_EQ(parsed.titleId, static_cast<uint64_t>(42));
    REQUIRE_EQ(parsed.backupName, std::string("slot-a"));
    REQUIRE_EQ(parsed.userId, std::string("user-1"));
}

TEST_CASE("Metadata logic rejects payload missing identity fields") {
    const std::string text =
        "title_name=No identity\n"
        "device_id=dev-x\n";

    core::BackupMetadata parsed;
    REQUIRE_EQ(core::parseBackupMetadata(text, parsed), false);
}

TEST_CASE("Metadata logic accepts backup_name-only metadata") {
    const std::string text = "backup_name=legacy-slot\n";

    core::BackupMetadata parsed;
    REQUIRE(core::parseBackupMetadata(text, parsed));
    REQUIRE_EQ(parsed.titleId, static_cast<uint64_t>(0));
    REQUIRE_EQ(parsed.backupName, std::string("legacy-slot"));
}

TEST_CASE("Metadata logic round-trips is_auto_backup flag") {
    const core::BackupMetadata input = makeAutoBackupMeta();
    const std::string text = core::serializeBackupMetadata(input);

    core::BackupMetadata parsed;
    REQUIRE(core::parseBackupMetadata(text, parsed));
    REQUIRE_EQ(parsed.isAutoBackup, true);
}

TEST_CASE("Metadata logic defaults is_auto_backup to false") {
    const std::string text =
        "title_id=42\n"
        "backup_name=manual-backup\n";

    core::BackupMetadata parsed;
    REQUIRE(core::parseBackupMetadata(text, parsed));
    REQUIRE_EQ(parsed.isAutoBackup, false);
}

TEST_CASE("Metadata logic parses is_auto_backup from both formats") {
    {
        const std::string text = "backup_name=test\nis_auto_backup=1\n";
        core::BackupMetadata parsed;
        REQUIRE(core::parseBackupMetadata(text, parsed));
        REQUIRE_EQ(parsed.isAutoBackup, true);
    }

    {
        const std::string text = "backup_name=test\nisAutoBackup=true\n";
        core::BackupMetadata parsed;
        REQUIRE(core::parseBackupMetadata(text, parsed));
        REQUIRE_EQ(parsed.isAutoBackup, true);
    }

    {
        const std::string text = "backup_name=test\nis_auto_backup=0\n";
        core::BackupMetadata parsed;
        REQUIRE(core::parseBackupMetadata(text, parsed));
        REQUIRE_EQ(parsed.isAutoBackup, false);
    }
}

TEST_CASE("Metadata logic defaults is_favorite to false when absent") {
    const std::string text =
        "title_id=42\n"
        "backup_name=manual-backup\n";

    core::BackupMetadata parsed;
    REQUIRE(core::parseBackupMetadata(text, parsed));
    REQUIRE_EQ(parsed.isFavorite, false);
}

TEST_CASE("Metadata logic parses is_favorite=1 as true") {
    const std::string text = "backup_name=test\nis_favorite=1\n";
    core::BackupMetadata parsed;
    REQUIRE(core::parseBackupMetadata(text, parsed));
    REQUIRE_EQ(parsed.isFavorite, true);
}

TEST_CASE("Metadata logic parses is_favorite=0 as false") {
    const std::string text = "backup_name=test\nis_favorite=0\n";
    core::BackupMetadata parsed;
    REQUIRE(core::parseBackupMetadata(text, parsed));
    REQUIRE_EQ(parsed.isFavorite, false);
}

TEST_CASE("Metadata logic parses isFavorite from both formats") {
    {
        const std::string text = "backup_name=test\nis_favorite=1\n";
        core::BackupMetadata parsed;
        REQUIRE(core::parseBackupMetadata(text, parsed));
        REQUIRE_EQ(parsed.isFavorite, true);
    }

    {
        const std::string text = "backup_name=test\nisFavorite=true\n";
        core::BackupMetadata parsed;
        REQUIRE(core::parseBackupMetadata(text, parsed));
        REQUIRE_EQ(parsed.isFavorite, true);
    }

    {
        const std::string text = "backup_name=test\nis_favorite=0\n";
        core::BackupMetadata parsed;
        REQUIRE(core::parseBackupMetadata(text, parsed));
        REQUIRE_EQ(parsed.isFavorite, false);
    }
}

TEST_CASE("Metadata logic round-trips is_favorite flag") {
    core::BackupMetadata input;
    input.titleId = 0x01006A800016E000ULL;
    input.backupName = "favorite-backup";
    input.isFavorite = true;

    const std::string text = core::serializeBackupMetadata(input);

    core::BackupMetadata parsed;
    REQUIRE(core::parseBackupMetadata(text, parsed));
    REQUIRE_EQ(parsed.isFavorite, true);
}

TEST_CASE("Metadata logic serialization includes is_favorite key") {
    core::BackupMetadata meta;
    meta.backupName = "test-backup";
    meta.isFavorite = true;

    const std::string text = core::serializeBackupMetadata(meta);
    REQUIRE(text.find("is_favorite=1") != std::string::npos);
}
