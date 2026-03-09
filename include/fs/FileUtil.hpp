/**
 * Drop-Keep - Dropbox Save Sync for Nintendo Switch
 * File utilities - recursive copy, delete, etc.
 * Based on JKSV patterns
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <cstring>
#include <array>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

#include "utils/Logger.hpp"

namespace fs {

// Buffer size for file copy operations
constexpr size_t COPY_BUFFER_SIZE = 0x40000; // 256KB
constexpr int64_t DEFAULT_JOURNAL_SIZE = 0x1000000; // 16MB default

/**
 * Ensure directory exists (creates parent directories if needed)
 */
bool ensureDirectoryExists(const std::string& path);

/**
 * Copy file with progress callback
 */
bool copyFileWithProgress(const std::string& source, const std::string& dest,
                          int64_t journalSize = DEFAULT_JOURNAL_SIZE,
                          std::function<void(size_t, size_t)> progressCallback = nullptr);

/**
 * Copy directory with progress callback
 */
bool copyDirectoryWithProgress(const std::string& source, const std::string& dest,
                               int64_t journalSize = DEFAULT_JOURNAL_SIZE,
                               std::function<void(size_t, size_t)> progressCallback = nullptr);

/**
 * Get journal size for a save
 */
int64_t getSaveJournalSize(uint64_t titleId);

/**
 * Copy a single file from source to destination
 * Uses journaling for save data integrity
 */
inline bool copyFile(const std::string& source, const std::string& dest, 
                     int64_t journalSize = DEFAULT_JOURNAL_SIZE) {
    FILE* srcFile = fopen(source.c_str(), "rb");
    if (!srcFile) {
        LOG_ERROR("Failed to open source: %s", source.c_str());
        return false;
    }
    
    fseek(srcFile, 0, SEEK_SET);
    
    FILE* dstFile = fopen(dest.c_str(), "wb");
    if (!dstFile) {
        LOG_ERROR("Failed to create dest: %s", dest.c_str());
        fclose(srcFile);
        return false;
    }
    
    // Fixed-size stack buffer keeps copy memory stable even for large saves.
    std::array<char, COPY_BUFFER_SIZE> buffer{};
    int64_t journalCount = 0;
    bool success = true;
    
    while (success && !feof(srcFile)) {
        size_t readSize = fread(buffer.data(), 1, COPY_BUFFER_SIZE, srcFile);
        if (readSize == 0 && !feof(srcFile)) {
            LOG_ERROR("Read error in: %s", source.c_str());
            success = false;
            break;
        }
        
        if (readSize > 0) {
            size_t written = fwrite(buffer.data(), 1, readSize, dstFile);
            if (written != readSize) {
                LOG_ERROR("Write error in: %s", dest.c_str());
                success = false;
                break;
            }
            
            // Journal-based commit for save data integrity
            journalCount += readSize;
            if (journalCount >= journalSize) {
                fflush(dstFile);
                journalCount = 0;
            }
        }
    }
    
    fclose(srcFile);
    fclose(dstFile);
    
    return success;
}

/**
 * Copy directory recursively from source to destination
 * Creates destination directory if it doesn't exist
 */
inline bool copyDirectory(const std::string& source, const std::string& dest,
                          int64_t journalSize = DEFAULT_JOURNAL_SIZE) {
    // Create destination directory
    mkdir(dest.c_str(), 0777);
    
    DIR* dir = opendir(source.c_str());
    if (!dir) {
        LOG_ERROR("Failed to open directory: %s", source.c_str());
        return false;
    }
    
    struct dirent* entry;
    bool success = true;
    
    while ((entry = readdir(dir)) != nullptr && success) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string srcPath = source + "/" + entry->d_name;
        std::string dstPath = dest + "/" + entry->d_name;
        
        struct stat st;
        if (stat(srcPath.c_str(), &st) != 0) {
            LOG_ERROR("Failed to stat: %s", srcPath.c_str());
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            // Recursively copy subdirectory
            success = copyDirectory(srcPath, dstPath, journalSize);
        } else if (S_ISREG(st.st_mode)) {
            // Copy file
            success = copyFile(srcPath, dstPath, journalSize);
        }
    }
    
    closedir(dir);
    return success;
}

/**
 * Delete directory recursively
 */
inline bool deleteDirectory(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return true; // Already doesn't exist
    }
    
    struct dirent* entry;
    bool success = true;
    
    while ((entry = readdir(dir)) != nullptr && success) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string fullPath = path + "/" + entry->d_name;
        
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            success = deleteDirectory(fullPath);
        } else {
            if (remove(fullPath.c_str()) != 0) {
                LOG_ERROR("Failed to delete: %s", fullPath.c_str());
                success = false;
            }
        }
    }
    
    closedir(dir);
    
    // Delete the now-empty directory
    if (success && rmdir(path.c_str()) != 0) {
        LOG_ERROR("Failed to rmdir: %s", path.c_str());
        return false;
    }
    
    return success;
}

/**
 * Check if directory has contents
 */
inline bool directoryHasContents(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) return false;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            closedir(dir);
            return true;
        }
    }
    
    closedir(dir);
    return false;
}

/**
 * Get directory size in bytes
 */
inline int64_t getDirectorySize(const std::string& path) {
    int64_t totalSize = 0;
    
    DIR* dir = opendir(path.c_str());
    if (!dir) return 0;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string fullPath = path + "/" + entry->d_name;
        struct stat st;
        
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                totalSize += getDirectorySize(fullPath);
            } else {
                totalSize += st.st_size;
            }
        }
    }
    
    closedir(dir);
    return totalSize;
}

/**
 * Commit save data to filesystem (critical for Switch saves)
 */
inline bool commitSaveData(const std::string& mountPoint) {
#ifdef __SWITCH__
    Result rc = fsdevCommitDevice(mountPoint.c_str());
    if (R_FAILED(rc)) {
        LOG_ERROR("fsdevCommitDevice failed: 0x%x", rc);
        return false;
    }
#endif
    return true;
}

} // namespace fs
