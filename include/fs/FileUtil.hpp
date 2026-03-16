/**
 * oc-save-keeper - Dropbox Save Sync for Nintendo Switch
 * File utilities
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <dirent.h>
#include <sys/stat.h>
#include <switch.h>
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

bool ensureDirectoryExists(const std::string& path);

/**
 * Recursive directory copy with progress and JKSV-style physical commits
 * @param mountName For save data, the fsdev mount name (e.g., "save") to commit to.
 */
bool copyDirectoryWithProgress(const std::string& source, const std::string& dest,
                               int64_t journalSize = DEFAULT_JOURNAL_SIZE,
                               const std::string& mountName = "",
                               std::function<void(size_t, size_t)> progressCallback = nullptr);

/**
 * File copy with JKSV-style physical commits (Close -> Commit -> Open)
 * @param mountName If not empty, will call fsdevCommitDevice(mountName) during the copy.
 */
bool copyFileWithProgress(const std::string& source, const std::string& dest,
                          int64_t journalSize = DEFAULT_JOURNAL_SIZE,
                          const std::string& mountName = "",
                          std::function<void(size_t, size_t)> progressCallback = nullptr);

/**
 * Safely delete all contents inside a directory WITHOUT deleting the directory itself.
 * Crucial for clearing save mounts (e.g. "save:") before restoration without unmounting.
 */
bool clearDirectoryContents(const std::string& path);

/**
 * Recursive directory deletion
 */
bool deleteDirectory(const std::string& path);

/**
 * Get total size of a directory
 */
int64_t getDirectorySize(const std::string& path);

/**
 * Get journal size for a specific title (or default)
 */
int64_t getSaveJournalSize(uint64_t titleId);

} // namespace fs
