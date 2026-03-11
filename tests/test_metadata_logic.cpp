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
