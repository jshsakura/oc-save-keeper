#include "tests/TestHarness.hpp"

#include "core/MetadataLogic.hpp"

#include <cstdio>
#include <string>
#include <unistd.h>

namespace {

std::string makeTempPath() {
    char tmpl[] = "/tmp/ocsk_meta_XXXXXX";
    const int fd = mkstemp(tmpl);
    if (fd >= 0) {
        close(fd);
    }
    return tmpl;
}

core::BackupMetadata makeMeta() {
    core::BackupMetadata meta;
    meta.titleId = 0x01006A800016E000ULL;
    meta.titleName = "The Legend of Zelda: BOTW";
    meta.backupName = "slot-1";
    meta.revisionId = "rev-1";
    meta.deviceId = "dev-a";
    meta.deviceLabel = "OLED";
    meta.userId = "user-a";
    meta.userName = "Alice";
    meta.source = "cloud";
    meta.createdAt = static_cast<std::time_t>(1710000000);
    meta.devicePriority = 200;
    meta.size = 777;
    return meta;
}

void writeRaw(const std::string& path, const std::string& text) {
    FILE* file = std::fopen(path.c_str(), "w");
    if (!file) {
        return;
    }
    std::fputs(text.c_str(), file);
    std::fclose(file);
}

std::string readRaw(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "r");
    if (!file) {
        return "";
    }

    std::string text;
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), file)) {
        text += buffer;
    }

    std::fclose(file);
    return text;
}

}

TEST_CASE("Metadata file logic writes and reads round-trip") {
    const std::string path = makeTempPath();
    const core::BackupMetadata input = makeMeta();

    REQUIRE(core::writeBackupMetadataFile(path, input));

    core::BackupMetadata parsed;
    REQUIRE(core::readBackupMetadataFile(path, parsed));
    REQUIRE_EQ(parsed.titleId, input.titleId);
    REQUIRE_EQ(parsed.backupName, input.backupName);
    REQUIRE_EQ(parsed.devicePriority, input.devicePriority);
    REQUIRE_EQ(parsed.size, input.size);

    std::remove(path.c_str());
}

TEST_CASE("Metadata file logic returns false for missing file") {
    core::BackupMetadata parsed;
    REQUIRE_EQ(core::readBackupMetadataFile("/tmp/ocsk_missing_meta_please_ignore", parsed), false);
}

TEST_CASE("Metadata file logic parses CRLF and ignores malformed lines") {
    const std::string path = makeTempPath();
    writeRaw(path,
             "title_id=42\r\n"
             "broken_line\r\n"
             "backup_name=slot-crlf\r\n"
             "user_id=user-1\r\n");

    core::BackupMetadata parsed;
    REQUIRE(core::readBackupMetadataFile(path, parsed));
    REQUIRE_EQ(parsed.titleId, static_cast<uint64_t>(42));
    REQUIRE_EQ(parsed.backupName, std::string("slot-crlf"));
    REQUIRE_EQ(parsed.userId, std::string("user-1"));

    std::remove(path.c_str());
}

TEST_CASE("Metadata file logic write fails for invalid parent path") {
    const core::BackupMetadata meta = makeMeta();
    REQUIRE_EQ(core::writeBackupMetadataFile("/path/that/does/not/exist/ocsk.meta", meta), false);
}

TEST_CASE("Metadata file logic copies metadata text as-is") {
    const std::string sourcePath = makeTempPath();
    const std::string destinationPath = makeTempPath();
    const std::string input =
        "title_id=42\n"
        "backup_name=slot-a\r\n"
        "user_id=user-a\n";

    writeRaw(sourcePath, input);
    REQUIRE(core::copyMetadataFile(sourcePath, destinationPath));
    REQUIRE_EQ(readRaw(destinationPath), input);

    std::remove(sourcePath.c_str());
    std::remove(destinationPath.c_str());
}

TEST_CASE("Metadata file logic copy fails for missing source") {
    const std::string destinationPath = makeTempPath();
    REQUIRE_EQ(core::copyMetadataFile("/tmp/ocsk_missing_source_meta", destinationPath), false);
    std::remove(destinationPath.c_str());
}

TEST_CASE("Metadata file logic copy fails for invalid destination") {
    const std::string sourcePath = makeTempPath();
    writeRaw(sourcePath, "title_id=7\nbackup_name=test\n");

    REQUIRE_EQ(core::copyMetadataFile(sourcePath, "/path/that/does/not/exist/target.meta"), false);

    std::remove(sourcePath.c_str());
}
