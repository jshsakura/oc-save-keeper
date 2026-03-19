#include "tests/TestHarness.hpp"

#include "zip/ZipArchive.hpp"

TEST_CASE("ZIP path validation accepts safe relative paths") {
    REQUIRE_EQ(zip::isSafeArchivePath("file.txt"), true);
    REQUIRE_EQ(zip::isSafeArchivePath("save/file.txt"), true);
    REQUIRE_EQ(zip::isSafeArchivePath("save/data/file.txt"), true);
    REQUIRE_EQ(zip::isSafeArchivePath("folder/subfolder/file.dat"), true);
}

TEST_CASE("ZIP path validation rejects empty path") {
    REQUIRE_EQ(zip::isSafeArchivePath(""), false);
}

TEST_CASE("ZIP path validation rejects path traversal with double dot") {
    REQUIRE_EQ(zip::isSafeArchivePath("../etc/passwd"), false);
    REQUIRE_EQ(zip::isSafeArchivePath("save/../../etc/passwd"), false);
    REQUIRE_EQ(zip::isSafeArchivePath("save/../file.txt"), false);
    REQUIRE_EQ(zip::isSafeArchivePath("..\\windows\\system32"), false);
    REQUIRE_EQ(zip::isSafeArchivePath("save\\..\\file.txt"), false);
}

TEST_CASE("ZIP path validation rejects absolute Unix paths") {
    REQUIRE_EQ(zip::isSafeArchivePath("/etc/passwd"), false);
    REQUIRE_EQ(zip::isSafeArchivePath("/root/.ssh/id_rsa"), false);
    REQUIRE_EQ(zip::isSafeArchivePath("/home/user/file.txt"), false);
}

TEST_CASE("ZIP path validation rejects absolute Windows paths") {
    REQUIRE_EQ(zip::isSafeArchivePath("C:\\Windows\\System32"), false);
    REQUIRE_EQ(zip::isSafeArchivePath("D:\\secret\\file.txt"), false);
    REQUIRE_EQ(zip::isSafeArchivePath("c:\\users\\admin\\desktop"), false);
}

TEST_CASE("ZIP path validation rejects backslash-prefixed paths") {
    REQUIRE_EQ(zip::isSafeArchivePath("\\\\server\\share\\file"), false);
    REQUIRE_EQ(zip::isSafeArchivePath("\\etc\\passwd"), false);
}

TEST_CASE("ZIP path validation accepts paths with valid special characters") {
    REQUIRE_EQ(zip::isSafeArchivePath("save-file.txt"), true);
    REQUIRE_EQ(zip::isSafeArchivePath("save_file.txt"), true);
    REQUIRE_EQ(zip::isSafeArchivePath("save.file.txt"), true);
    REQUIRE_EQ(zip::isSafeArchivePath("save file.txt"), true);
}

TEST_CASE("ZIP path validation rejects hidden double dot not at start") {
    REQUIRE_EQ(zip::isSafeArchivePath("file..txt"), false);
    REQUIRE_EQ(zip::isSafeArchivePath("save..backup/file.txt"), false);
}
