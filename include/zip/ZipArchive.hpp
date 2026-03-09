/**
 * Drop-Keep - Dropbox Save Sync for Nintendo Switch
 * ZIP archive support for backups
 * Uses minizip library
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

#include "utils/Logger.hpp"

namespace zip {

/**
 * Simple ZIP file wrapper for backup operations
 */
class ZipArchive {
public:
    ZipArchive();
    ~ZipArchive();
    
    // No copy
    ZipArchive(const ZipArchive&) = delete;
    ZipArchive& operator=(const ZipArchive&) = delete;
    
    // Move allowed
    ZipArchive(ZipArchive&& other) noexcept;
    ZipArchive& operator=(ZipArchive&& other) noexcept;
    
    /**
     * Create a new ZIP file for writing
     */
    bool create(const std::string& path);
    
    /**
     * Open an existing ZIP file for reading
     */
    bool open(const std::string& path);
    
    /**
     * Close the archive
     */
    void close();
    
    /**
     * Add a file to the archive
     * @param sourcePath Path to source file
     * @param archivePath Path within ZIP (e.g., "save/file.dat")
     */
    bool addFile(const std::string& sourcePath, const std::string& archivePath);
    
    /**
     * Add a directory recursively
     * @param sourceDir Source directory path
     * @param archiveBase Base path within ZIP
     */
    bool addDirectory(const std::string& sourceDir, const std::string& archiveBase = "");
    
    /**
     * Extract a file from the archive
     * @param archivePath Path within ZIP
     * @param destPath Destination file path
     */
    bool extractFile(const std::string& archivePath, const std::string& destPath);
    
    /**
     * Extract entire archive to directory
     * @param destDir Destination directory
     */
    bool extractAll(const std::string& destDir);
    
    /**
     * Get list of files in archive
     */
    std::vector<std::string> listFiles();
    
    /**
     * Check if a file exists in archive
     */
    bool hasFile(const std::string& archivePath);
    
    /**
     * Check if archive is open
     */
    bool isOpen() const { return m_isOpen; }
    
    /**
     * Check if archive is open for writing
     */
    bool isWriting() const { return m_isWriting; }

private:
    void* m_zip;           // zipFile (opaque pointer)
    void* m_unz;           // unzFile (opaque pointer)
    bool m_isOpen;
    bool m_isWriting;
    std::string m_path;
};

/**
 * Convenience functions
 */

/**
 * Create a ZIP file from a directory
 */
inline bool zipDirectory(const std::string& sourceDir, const std::string& zipPath) {
    ZipArchive zip;
    if (!zip.create(zipPath)) {
        LOG_ERROR("Failed to create ZIP: %s", zipPath.c_str());
        return false;
    }
    
    bool success = zip.addDirectory(sourceDir);
    zip.close();
    
    if (success) {
        LOG_INFO("Created ZIP: %s", zipPath.c_str());
    }
    return success;
}

/**
 * Extract a ZIP file to a directory
 */
inline bool unzipToDirectory(const std::string& zipPath, const std::string& destDir) {
    ZipArchive zip;
    if (!zip.open(zipPath)) {
        LOG_ERROR("Failed to open ZIP: %s", zipPath.c_str());
        return false;
    }
    
    bool success = zip.extractAll(destDir);
    zip.close();
    
    if (success) {
        LOG_INFO("Extracted ZIP: %s -> %s", zipPath.c_str(), destDir.c_str());
    }
    return success;
}

/**
 * Save metadata structure stored in ZIP files
 */
struct SaveMeta {
    static constexpr uint32_t MAGIC = 0x4B504447; // "GDKP" (Drop-Keep)
    static constexpr uint8_t VERSION = 1;
    
    uint32_t magic = MAGIC;
    uint8_t version = VERSION;
    uint8_t reserved[3] = {0};
    uint64_t titleId = 0;
    uint64_t timestamp = 0;
    uint8_t accountId[16] = {0}; // AccountUid
    char titleName[64] = {0};
    char backupName[32] = {0};
    int64_t saveSize = 0;
    
    /**
     * Write metadata to a file path
     */
    bool writeToFile(const std::string& path) const;
    
    /**
     * Read metadata from a file path
     */
    bool readFromFile(const std::string& path);
};

} // namespace zip
