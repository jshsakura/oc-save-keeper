/**
 * Drop-Keep - Dropbox Save Sync for Nintendo Switch
 * ZIP archive implementation using minizip
 */

#include "zip/ZipArchive.hpp"
#include "fs/FileUtil.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <ctime>
#include <array>

// Minizip headers
extern "C" {
#include <minizip/zip.h>
#include <minizip/unzip.h>
}

namespace zip {

// Buffer size for ZIP operations
constexpr size_t ZIP_BUFFER_SIZE = 0x10000; // 64KB

ZipArchive::ZipArchive()
    : m_zip(nullptr)
    , m_unz(nullptr)
    , m_isOpen(false)
    , m_isWriting(false) {
}

ZipArchive::~ZipArchive() {
    close();
}

ZipArchive::ZipArchive(ZipArchive&& other) noexcept
    : m_zip(other.m_zip)
    , m_unz(other.m_unz)
    , m_isOpen(other.m_isOpen)
    , m_isWriting(other.m_isWriting)
    , m_path(std::move(other.m_path)) {
    other.m_zip = nullptr;
    other.m_unz = nullptr;
    other.m_isOpen = false;
}

ZipArchive& ZipArchive::operator=(ZipArchive&& other) noexcept {
    if (this != &other) {
        close();
        m_zip = other.m_zip;
        m_unz = other.m_unz;
        m_isOpen = other.m_isOpen;
        m_isWriting = other.m_isWriting;
        m_path = std::move(other.m_path);
        other.m_zip = nullptr;
        other.m_unz = nullptr;
        other.m_isOpen = false;
    }
    return *this;
}

bool ZipArchive::create(const std::string& path) {
    if (m_isOpen) close();
    
    // Create parent directories if needed
    size_t lastSlash = path.rfind('/');
    if (lastSlash != std::string::npos) {
        std::string dir = path.substr(0, lastSlash);
        fs::ensureDirectoryExists(dir);
    }
    
    m_zip = zipOpen64(path.c_str(), APPEND_STATUS_CREATE);
    if (!m_zip) {
        LOG_ERROR("Failed to create ZIP: %s", path.c_str());
        return false;
    }
    
    m_path = path;
    m_isOpen = true;
    m_isWriting = true;
    return true;
}

bool ZipArchive::open(const std::string& path) {
    if (m_isOpen) close();
    
    m_unz = unzOpen64(path.c_str());
    if (!m_unz) {
        LOG_ERROR("Failed to open ZIP: %s", path.c_str());
        return false;
    }
    
    m_path = path;
    m_isOpen = true;
    m_isWriting = false;
    return true;
}

void ZipArchive::close() {
    if (m_zip) {
        zipClose(m_zip, nullptr);
        m_zip = nullptr;
    }
    if (m_unz) {
        unzClose(m_unz);
        m_unz = nullptr;
    }
    m_isOpen = false;
    m_isWriting = false;
}

bool ZipArchive::addFile(const std::string& sourcePath, const std::string& archivePath) {
    if (!m_isOpen || !m_isWriting) return false;
    
    FILE* srcFile = fopen(sourcePath.c_str(), "rb");
    if (!srcFile) {
        LOG_ERROR("Failed to open source file: %s", sourcePath.c_str());
        return false;
    }
    
    fseek(srcFile, 0, SEEK_SET);
    
    // Get file modification time
    struct stat st;
    time_t modTime = time(nullptr);
    if (stat(sourcePath.c_str(), &st) == 0) {
        modTime = st.st_mtime;
    }
    struct tm* t = localtime(&modTime);
    
    // Setup zip file info
    zip_fileinfo zi = {};
    zi.dosDate = 0;
    zi.tmz_date.tm_year = t->tm_year;
    zi.tmz_date.tm_mon = t->tm_mon;
    zi.tmz_date.tm_mday = t->tm_mday;
    zi.tmz_date.tm_hour = t->tm_hour;
    zi.tmz_date.tm_min = t->tm_min;
    zi.tmz_date.tm_sec = t->tm_sec;
    
    // Open new entry in zip
    int err = zipOpenNewFileInZip(m_zip, archivePath.c_str(), &zi,
                                   nullptr, 0, nullptr, 0, nullptr,
                                   Z_DEFLATED, Z_DEFAULT_COMPRESSION);
    if (err != ZIP_OK) {
        LOG_ERROR("Failed to create ZIP entry: %s", archivePath.c_str());
        fclose(srcFile);
        return false;
    }
    
    // Copy file data
    std::array<char, ZIP_BUFFER_SIZE> buffer{};
    bool success = true;
    
    while (!feof(srcFile) && success) {
        size_t bytesRead = fread(buffer.data(), 1, ZIP_BUFFER_SIZE, srcFile);
        if (bytesRead > 0) {
            err = zipWriteInFileInZip(m_zip, buffer.data(), bytesRead);
            if (err != ZIP_OK) {
                LOG_ERROR("Failed to write ZIP data: %s", archivePath.c_str());
                success = false;
            }
        }
    }

    fclose(srcFile);
    zipCloseFileInZip(m_zip);
    
    return success;
}

bool ZipArchive::addDirectory(const std::string& sourceDir, const std::string& archiveBase) {
    if (!m_isOpen || !m_isWriting) return false;
    
    DIR* dir = opendir(sourceDir.c_str());
    if (!dir) {
        LOG_ERROR("Failed to open directory: %s", sourceDir.c_str());
        return false;
    }
    
    struct dirent* entry;
    bool success = true;
    
    while ((entry = readdir(dir)) != nullptr && success) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string srcPath = sourceDir + "/" + entry->d_name;
        std::string archivePath = archiveBase.empty() ? entry->d_name : archiveBase + "/" + entry->d_name;
        
        struct stat st;
        if (stat(srcPath.c_str(), &st) != 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            // Recursively add subdirectory
            success = addDirectory(srcPath, archivePath);
        } else if (S_ISREG(st.st_mode)) {
            // Add file
            success = addFile(srcPath, archivePath);
        }
    }
    
    closedir(dir);
    return success;
}

bool ZipArchive::extractFile(const std::string& archivePath, const std::string& destPath) {
    if (!m_isOpen || m_isWriting) return false;
    
    // Find file in archive
    if (unzLocateFile(m_unz, archivePath.c_str(), 0) != UNZ_OK) {
        LOG_ERROR("File not found in ZIP: %s", archivePath.c_str());
        return false;
    }
    
    // Get file info
    unz_file_info64 info;
    char filename[256];
    if (unzGetCurrentFileInfo64(m_unz, &info, filename, sizeof(filename), 
                                 nullptr, 0, nullptr, 0) != UNZ_OK) {
        return false;
    }
    
    // Open file in archive
    if (unzOpenCurrentFile(m_unz) != UNZ_OK) {
        LOG_ERROR("Failed to open file in ZIP: %s", archivePath.c_str());
        return false;
    }
    
    // Create destination directory
    size_t lastSlash = destPath.rfind('/');
    if (lastSlash != std::string::npos) {
        fs::ensureDirectoryExists(destPath.substr(0, lastSlash));
    }
    
    // Create destination file
    FILE* dstFile = fopen(destPath.c_str(), "wb");
    if (!dstFile) {
        LOG_ERROR("Failed to create file: %s", destPath.c_str());
        unzCloseCurrentFile(m_unz);
        return false;
    }
    
    // Extract data
    std::array<char, ZIP_BUFFER_SIZE> buffer{};
    bool success = true;
    int bytesRead;
    
    while ((bytesRead = unzReadCurrentFile(m_unz, buffer.data(), ZIP_BUFFER_SIZE)) > 0) {
        if (fwrite(buffer.data(), 1, bytesRead, dstFile) != (size_t)bytesRead) {
            LOG_ERROR("Failed to write file: %s", destPath.c_str());
            success = false;
            break;
        }
    }
    
    if (bytesRead < 0) {
        LOG_ERROR("Failed to read from ZIP: %s", archivePath.c_str());
        success = false;
    }
    
    fclose(dstFile);
    unzCloseCurrentFile(m_unz);
    
    return success;
}

bool ZipArchive::extractAll(const std::string& destDir) {
    if (!m_isOpen || m_isWriting) return false;
    
    // Create destination directory
    fs::ensureDirectoryExists(destDir);
    
    // Go to first file
    if (unzGoToFirstFile(m_unz) != UNZ_OK) {
        return true; // Empty archive
    }
    
    bool success = true;
    do {
        // Get file info
        unz_file_info64 info;
        char filename[512];
        if (unzGetCurrentFileInfo64(m_unz, &info, filename, sizeof(filename),
                                     nullptr, 0, nullptr, 0) != UNZ_OK) {
            success = false;
            continue;
        }
        
        std::string destPath = destDir + "/" + filename;
        
        // Check if it's a directory entry
        size_t len = strlen(filename);
        if (len > 0 && filename[len - 1] == '/') {
            fs::ensureDirectoryExists(destPath);
            continue;
        }
        
        // Extract file
        if (!extractFile(filename, destPath)) {
            LOG_ERROR("Failed to extract: %s", filename);
            success = false;
        }
        
    } while (unzGoToNextFile(m_unz) == UNZ_OK);
    
    return success;
}

std::vector<std::string> ZipArchive::listFiles() {
    std::vector<std::string> files;
    if (!m_isOpen || m_isWriting) return files;
    
    if (unzGoToFirstFile(m_unz) != UNZ_OK) return files;
    
    do {
        char filename[512];
        if (unzGetCurrentFileInfo64(m_unz, nullptr, filename, sizeof(filename),
                                     nullptr, 0, nullptr, 0) == UNZ_OK) {
            // Skip directory entries
            size_t len = strlen(filename);
            if (len > 0 && filename[len - 1] != '/') {
                files.push_back(filename);
            }
        }
    } while (unzGoToNextFile(m_unz) == UNZ_OK);
    
    return files;
}

bool ZipArchive::hasFile(const std::string& archivePath) {
    if (!m_isOpen || m_isWriting) return false;
    return unzLocateFile(m_unz, archivePath.c_str(), 0) == UNZ_OK;
}

// SaveMeta implementation

bool SaveMeta::writeToFile(const std::string& path) const {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    
    bool success = fwrite(this, sizeof(SaveMeta), 1, f) == 1;
    fclose(f);
    return success;
}

bool SaveMeta::readFromFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    
    bool success = fread(this, sizeof(SaveMeta), 1, f) == 1;
    fclose(f);
    
    // Verify magic and version
    if (magic != MAGIC || version > VERSION) {
        return false;
    }
    
    return success;
}

} // namespace zip
