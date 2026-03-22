/**
 * oc-save-keeper - Safe save backup and sync for homebrew
 * File utilities implementation
 * 100% Aligned with JKSV's physical commitment strategy
 * Ultimate safety edition: Iteration-safe deletion and eager commits
 */

#include "fs/FileUtil.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <vector>

namespace fs {

bool ensureDirectoryExists(const std::string& path) {
    if (path.empty()) return false;
    
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos && pos > 0) {
        std::string parent = path.substr(0, pos);
        if (!ensureDirectoryExists(parent)) {
            return false;
        }
    }
    
    return mkdir(path.c_str(), 0777) == 0 || errno == EEXIST;
}

/**
 * Physical commit to fsdev mount
 */
static void physicalCommit(const std::string& mountName) {
#ifdef __SWITCH__
    if (!mountName.empty()) {
        fsdevCommitDevice(mountName.c_str());
    }
#else
    (void)mountName;
#endif
}

bool copyFileWithProgress(const std::string& source, const std::string& dest,
                          int64_t journalSize,
                          const std::string& mountName,
                          std::function<void(size_t, size_t)> progressCallback) {
    ScopedFile srcFile(fopen(source.c_str(), "rb"));
    if (!srcFile) {
        LOG_ERROR("FS: Failed to open source %s", source.c_str());
        return false;
    }
    
    if (fseek(srcFile.get(), 0, SEEK_END) != 0) {
        LOG_ERROR("FS: Failed to seek in source %s", source.c_str());
        return false;
    }
    long long fileSizeLong = ftell(srcFile.get());
    if (fileSizeLong < 0) {
        LOG_ERROR("FS: Failed to get file size for %s", source.c_str());
        return false;
    }
    size_t fileSize = static_cast<size_t>(fileSizeLong);
    fseek(srcFile.get(), 0, SEEK_SET);
    
    ScopedFile dstFile(fopen(dest.c_str(), "wb"));
    if (!dstFile) {
        LOG_ERROR("FS: Failed to create dest %s", dest.c_str());
        return false;
    }
    
    std::array<char, COPY_BUFFER_SIZE> buffer{};
    int64_t journalCount = 0;
    size_t totalCopied = 0;
    bool success = true;
    
    while (success && !feof(srcFile.get())) {
        size_t readSize = fread(buffer.data(), 1, COPY_BUFFER_SIZE, srcFile.get());
        if (readSize == 0 && !feof(srcFile.get())) {
            LOG_ERROR("FS: Read error in %s", source.c_str());
            success = false;
            break;
        }
        
        if (readSize > 0) {
            // Check journal size before writing (JKSV Style)
            if (journalSize > 0 && journalCount + (int64_t)readSize >= journalSize) {
                dstFile = ScopedFile(nullptr);  // Close via RAII
                LOG_INFO("FS: Journal threshold reached, committing %s", mountName.c_str());
                physicalCommit(mountName);
                
                dstFile = ScopedFile(fopen(dest.c_str(), "rb+"));
                if (!dstFile) {
                    LOG_ERROR("FS: Failed to re-open dest %s after commit", dest.c_str());
                    success = false;
                    break;
                }
                if (fseek(dstFile.get(), (long)totalCopied, SEEK_SET) != 0) {
                    LOG_ERROR("FS: CRITICAL - Failed to seek to position %zu in %s after commit!", totalCopied, dest.c_str());
                    success = false;
                    break;
                }
                journalCount = 0;
            }

            size_t written = fwrite(buffer.data(), 1, readSize, dstFile.get());
            if (written != readSize) {
                LOG_ERROR("FS: Write error in %s", dest.c_str());
                success = false;
                break;
            }
            
            totalCopied += written;
            journalCount += written;
            
            if (progressCallback) {
                progressCallback(totalCopied, fileSize);
            }
        }
    }
    
    if (success) {
        physicalCommit(mountName);
    }
    
    return success;
}

bool copyDirectoryWithProgress(const std::string& source, const std::string& dest,
                               int64_t journalSize,
                               const std::string& mountName,
                               std::function<void(size_t, size_t)> progressCallback) {
    if (!ensureDirectoryExists(dest)) {
        LOG_ERROR("FS: Failed to create directory %s", dest.c_str());
        return false;
    }
    
    // Eager commit after directory creation to avoid journal pressure
    if (!mountName.empty()) {
        physicalCommit(mountName);
    }
    
    DIR* dir = opendir(source.c_str());
    if (!dir) {
        LOG_ERROR("FS: Failed to open directory %s", source.c_str());
        return false;
    }
    
    struct dirent* entry;
    bool success = true;
    
    while ((entry = readdir(dir)) != nullptr && success) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string srcPath = source + "/" + entry->d_name;
        std::string dstPath = dest + "/" + entry->d_name;
        
        struct stat st;
        if (stat(srcPath.c_str(), &st) != 0) {
            LOG_ERROR("FS: Failed to stat %s", srcPath.c_str());
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            success = copyDirectoryWithProgress(srcPath, dstPath, journalSize, mountName, progressCallback);
        } else if (S_ISREG(st.st_mode)) {
            success = copyFileWithProgress(srcPath, dstPath, journalSize, mountName, progressCallback);
        }
    }
    
    closedir(dir);
    
    if (success) {
        physicalCommit(mountName);
    }
    
    return success;
}

/**
 * Helper to collect all entry names in a directory.
 * Prevents issues with readdir while modifying the directory structure.
 */
static std::vector<std::string> getDirectoryEntries(const std::string& path) {
    std::vector<std::string> entries;
    DIR* dir = opendir(path.c_str());
    if (!dir) return entries;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        entries.push_back(entry->d_name);
    }
    closedir(dir);
    return entries;
}

bool clearDirectoryContents(const std::string& path) {
    std::vector<std::string> entries = getDirectoryEntries(path);
    bool success = true;

    for (const auto& name : entries) {
        std::string fullPath = path + "/" + name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (!deleteDirectory(fullPath)) success = false;
            } else {
                if (unlink(fullPath.c_str()) != 0) success = false;
            }
        }
    }
    return success;
}

bool deleteDirectory(const std::string& path) {
    if (!clearDirectoryContents(path)) return false;
    return rmdir(path.c_str()) == 0;
}

int64_t getDirectorySize(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) return 0;

    int64_t total = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string fullPath = path + "/" + entry->d_name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                total += getDirectorySize(fullPath);
            } else {
                total += st.st_size;
            }
        }
    }
    closedir(dir);
    return total;
}

int64_t getSaveJournalSize(uint64_t titleId) {
    (void)titleId;
    return DEFAULT_JOURNAL_SIZE;
}

} // namespace fs
