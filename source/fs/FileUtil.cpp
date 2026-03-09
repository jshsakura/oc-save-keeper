/**
 * Drop-Keep - Dropbox Save Sync for Nintendo Switch
 * File utilities implementation
 */

#include "fs/FileUtil.hpp"
#include <algorithm>
#include <array>

namespace fs {

bool ensureDirectoryExists(const std::string& path) {
    if (path.empty()) return false;
    
    // Check if already exists
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    
    // Create parent directories first
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos && pos > 0) {
        std::string parent = path.substr(0, pos);
        if (!ensureDirectoryExists(parent)) {
            return false;
        }
    }
    
    // Create this directory
    return mkdir(path.c_str(), 0777) == 0 || errno == EEXIST;
}

bool copyFileWithProgress(const std::string& source, const std::string& dest,
                          int64_t journalSize,
                          std::function<void(size_t, size_t)> progressCallback) {
    FILE* srcFile = fopen(source.c_str(), "rb");
    if (!srcFile) {
        LOG_ERROR("Failed to open source: %s", source.c_str());
        return false;
    }
    
    // Get source file size
    fseek(srcFile, 0, SEEK_END);
    size_t fileSize = ftell(srcFile);
    fseek(srcFile, 0, SEEK_SET);
    
    FILE* dstFile = fopen(dest.c_str(), "wb");
    if (!dstFile) {
        LOG_ERROR("Failed to create dest: %s", dest.c_str());
        fclose(srcFile);
        return false;
    }
    
    // Allocate buffer
    std::array<char, COPY_BUFFER_SIZE> buffer{};
    int64_t journalCount = 0;
    size_t totalCopied = 0;
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
            
            totalCopied += written;
            
            // Progress callback
            if (progressCallback) {
                progressCallback(totalCopied, fileSize);
            }
            
            // Journal-based commit for save data integrity
            journalCount += readSize;
            if (journalSize > 0 && journalCount >= journalSize) {
                fflush(dstFile);
                journalCount = 0;
            }
        }
    }
    
    fclose(srcFile);
    fclose(dstFile);
    
    return success;
}

bool copyDirectoryWithProgress(const std::string& source, const std::string& dest,
                               int64_t journalSize,
                               std::function<void(size_t, size_t)> progressCallback) {
    // Create destination directory
    if (!ensureDirectoryExists(dest)) {
        LOG_ERROR("Failed to create directory: %s", dest.c_str());
        return false;
    }
    
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
            success = copyDirectoryWithProgress(srcPath, dstPath, journalSize, progressCallback);
        } else if (S_ISREG(st.st_mode)) {
            // Copy file
            success = copyFileWithProgress(srcPath, dstPath, journalSize, progressCallback);
        }
    }
    
    closedir(dir);
    return success;
}

int64_t getSaveJournalSize(uint64_t titleId) {
    (void)titleId;
    return DEFAULT_JOURNAL_SIZE;
}

} // namespace fs
