/**
 * oc-save-keeper - Safe save backup and sync for homebrew
 * File utilities
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include "utils/Logger.hpp"

namespace fs {

constexpr int64_t DEFAULT_JOURNAL_SIZE = 0x1000000;
constexpr size_t COPY_BUFFER_SIZE = 0x10000;

struct ScopedFile {
    FILE* fp = nullptr;
    explicit ScopedFile(FILE* f = nullptr) : fp(f) {}
    ~ScopedFile() { if (fp) fclose(fp); }
    ScopedFile(const ScopedFile&) = delete;
    ScopedFile& operator=(const ScopedFile&) = delete;
    ScopedFile(ScopedFile&& o) noexcept : fp(o.fp) { o.fp = nullptr; }
    ScopedFile& operator=(ScopedFile&& o) noexcept {
        if (this != &o) { if (fp) fclose(fp); fp = o.fp; o.fp = nullptr; }
        return *this;
    }
    FILE* get() const { return fp; }
    operator bool() const { return fp != nullptr; }
};

struct ScopedDir {
    DIR* dp = nullptr;
    explicit ScopedDir(DIR* d = nullptr) : dp(d) {}
    ~ScopedDir() { if (dp) closedir(dp); }
    ScopedDir(const ScopedDir&) = delete;
    ScopedDir& operator=(const ScopedDir&) = delete;
    ScopedDir(ScopedDir&& o) noexcept : dp(o.dp) { o.dp = nullptr; }
    ScopedDir& operator=(ScopedDir&& o) noexcept {
        if (this != &o) { if (dp) closedir(dp); dp = o.dp; o.dp = nullptr; }
        return *this;
    }
    DIR* get() const { return dp; }
    operator bool() const { return dp != nullptr; }
};

bool ensureDirectoryExists(const std::string& path);

bool copyDirectoryWithProgress(const std::string& source, const std::string& dest,
                               int64_t journalSize = DEFAULT_JOURNAL_SIZE,
                               const std::string& mountName = "",
                               std::function<void(size_t, size_t)> progressCallback = nullptr);
bool copyFileWithProgress(const std::string& source, const std::string& dest,
                          int64_t journalSize = DEFAULT_JOURNAL_SIZE,
                          const std::string& mountName = "",
                          std::function<void(size_t, size_t)> progressCallback = nullptr);
bool clearDirectoryContents(const std::string& path);
bool deleteDirectory(const std::string& path);
int64_t getDirectorySize(const std::string& path);
#ifdef __SWITCH__
int64_t getSaveJournalSize(uint64_t titleId);
#else
inline int64_t getSaveJournalSize(uint64_t) { return DEFAULT_JOURNAL_SIZE; }
#endif

} // namespace fs
