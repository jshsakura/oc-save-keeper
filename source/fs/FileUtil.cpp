/**
 * oc-save-keeper - Dropbox Save Sync for Nintendo Switch
 * File utilities implementation
 * 100% Aligned with JKSV's physical commitment strategy
 */

#include "fs/FileUtil.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <unistd.h>

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
    FILE* srcFile = fopen(source.c_str(), "rb");
    if (!srcFile) {
        LOG_ERROR("FS: Failed to open source %s", source.c_str());
        return false;
    }
    
    fseek(srcFile, 0, SEEK_END);
    size_t fileSize = ftell(srcFile);
    fseek(srcFile, 0, SEEK_SET);
    
    FILE* dstFile = fopen(dest.c_str(), "wb");
    if (!dstFile) {
        LOG_ERROR("FS: Failed to create dest %s", dest.c_str());
        fclose(srcFile);
        return false;
    }
    
    std::array<char, COPY_BUFFER_SIZE> buffer{};
    int64_t journalCount = 0;
    size_t totalCopied = 0;
    bool success = true;
    
    while (success && !feof(srcFile)) {
        size_t readSize = fread(buffer.data(), 1, COPY_BUFFER_SIZE, srcFile);
        if (readSize == 0 && !feof(srcFile)) {
            LOG_ERROR("FS: Read error in %s", source.c_str());
            success = false;
            break;
        }
        
        if (readSize > 0) {
            // Check journal size before writing (JKSV Style)
            // If the next write will exceed journal, we must commit.
            if (journalSize > 0 && journalCount + (int64_t)readSize >= journalSize) {
                // 1. Close the file to ensure all libc buffers are flushed
                fclose(dstFile);
                
                // 2. Perform physical commit on the NAND filesystem via fsdev
                LOG_INFO("FS: Journal threshold reached, committing %s", mountName.c_str());
                physicalCommit(mountName);
                
                // 3. Re-open the file in append mode or seek to current position
                dstFile = fopen(dest.c_str(), "rb+");
                if (!dstFile) {
                    LOG_ERROR("FS: Failed to re-open dest %s after commit", dest.c_str());
                    success = false;
                    break;
                }
                if (fseek(dstFile, totalCopied, SEEK_SET) != 0) {
                    LOG_ERROR("FS: CRITICAL - Failed to seek to position %zu after commit in %s", totalCopied, dest.c_str());
                    success = false;
                    break;
                }
                journalCount = 0;
            }

            size_t written = fwrite(buffer.data(), 1, readSize, dstFile);
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
    
    fclose(srcFile);
    if (dstFile) fclose(dstFile);
    
    // Final commit after file is finished (JKSV Style)
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
    
    // Directory level commit
    if (success) {
        physicalCommit(mountName);
    }
    
    return success;
}

bool clearDirectoryContents(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) return false;

    bool success = true;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string fullPath = path + "/" + entry->d_name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (!deleteDirectory(fullPath)) success = false;
            } else {
                if (unlink(fullPath.c_str()) != 0) success = false;
            }
        }
    }
    closedir(dir);
    return success;
}

bool deleteDirectory(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) return false;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string fullPath = path + "/" + entry->d_name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                deleteDirectory(fullPath);
            } else {
                unlink(fullPath.c_str());
            }
        }
    }
    closedir(dir);
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
