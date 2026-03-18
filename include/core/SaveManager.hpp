/**
 * Drop-Keep - Dropbox Save Sync for Nintendo Switch
 * Save Manager - handles save scanning, backup, restore
 */

#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <cstdint>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "utils/Logger.hpp"

namespace core {

// Save types
enum class SaveType {
    Account,     // User save
    System,      // System save
    Device,      // Device/shared save
    BCAT,        // BCAT data
    Cache,       // Cache
    Temporary    // Temporary
};

enum class SyncPriorityPolicy {
    PreferPriority,
    PreferNewest,
    PreferLocalOnTie
};

// User info
struct UserInfo {
#ifdef __SWITCH__
    AccountUid uid{};
#else
    uint64_t uidPlaceholder = 0;
#endif
    std::string id;
    std::string name;
    std::string iconPath;
};

// Title info
struct TitleInfo {
    uint64_t titleId = 0;
    std::string name;
    std::string publisher;
    std::string iconPath;
    std::string savePath;
    SaveType saveType = SaveType::Account;
    SaveType actualSaveType = SaveType::Account; 
    int64_t saveSize = 0;
    bool hasSave = false;
    bool hasAccountSave = false; // New
    bool hasDeviceSave = false;  // New
    int64_t accountSize = 0;     // New
    int64_t deviceSize = 0;      // New
    bool isFavorite = false;
};

// Backup version
struct BackupVersion {
    std::string path;
    std::string name;
    std::string deviceId;
    std::string deviceLabel;
    std::string userId;
    std::string userName;
    std::string source;
    std::time_t timestamp = 0;
    int64_t size = 0;
    bool isCloudSynced = false;
    bool isAutoBackup = false;      // Mark if this was auto-created before restore
    bool isTrashed = false;         // Mark if this is in trash bin
};

struct BackupMetadata {
    uint64_t titleId = 0;
    std::string titleName;
    std::string backupName;
    std::string revisionId;
    std::string deviceId;
    std::string deviceLabel;
    std::string userId;
    std::string userName;
    std::string source;
    std::time_t createdAt = 0;
    int devicePriority = 100;
    int64_t size = 0;
    bool isAutoBackup = false;
};

struct SyncDecision {
    bool useIncoming = false;
    std::string reason;
};

class SaveManager {
public:
    SaveManager();
    ~SaveManager();
    
    // Initialization
    bool initialize();
    void scanTitles();
    
    // User selection
    const std::vector<UserInfo>& getUsers() const { return m_users; }
    bool selectUser(size_t index);
    UserInfo* getSelectedUser() { return m_selectedUser; }
    
    // Title listing
    const std::vector<TitleInfo>& getTitles() const { return m_titles; }
    TitleInfo* getTitle(size_t index);
    TitleInfo* getTitleById(uint64_t titleId);
    std::vector<TitleInfo*> getTitlesWithSaves();
    
    // Backup operations
    bool backupSave(TitleInfo* title, const std::string& backupName);
    bool restoreSave(TitleInfo* title, const std::string& backupPath);
    bool deleteBackup(const std::string& backupPath);
    
    // Trash bin operations
    bool moveToTrash(const std::string& backupPath);
    bool restoreFromTrash(const std::string& trashPath);
    bool emptyTrash();
    std::vector<BackupVersion> listTrash(TitleInfo* title);
    std::string getTrashPath(TitleInfo* title) const;
    std::string getTrashPath() const;
    
    // Version management
    std::vector<BackupVersion> getBackupVersions(TitleInfo* title);
    bool createVersionedBackup(TitleInfo* title, int maxVersions = 5);
    
    // Batch operations
    bool backupAll();
    
    // Path helpers
    std::string getBackupPath(TitleInfo* title) const;
    std::string getCloudUserPath() const;
    std::string getCloudDevicesPath() const;
    std::string getCloudDevicePath(const std::string& deviceId = "") const;
    std::string getCloudTitlePath(TitleInfo* title) const;
    std::string getCloudPath(TitleInfo* title) const;
    std::string getCloudMetadataPath(TitleInfo* title) const;
    std::string getCloudRevisionDirectory(TitleInfo* title, const std::string& deviceId = "") const;

    // Sync metadata and archive helpers
    std::string getDeviceId();
    std::string getDeviceLabel() const { return m_deviceLabel; }
    int getDevicePriority() const { return m_devicePriority; }
    bool readBackupMetadata(const std::string& backupPath, BackupMetadata& outMeta);
    bool readMetadataFile(const std::string& metadataPath, BackupMetadata& outMeta);
    SyncDecision evaluateIncomingMetadata(TitleInfo* title, const BackupMetadata& incomingMeta) const;
    SyncDecision decideSync(const BackupMetadata* localMeta, const BackupMetadata& incomingMeta) const;
    std::string exportBackupArchive(TitleInfo* title, const std::string& backupPath);
    bool importBackupArchive(TitleInfo* title, const std::string& archivePath, std::string* outReason = nullptr, bool skipConflictCheck = false);
    
private:
    bool m_initialized;
    std::string m_deviceId;
    std::string m_deviceLabel;
    int m_devicePriority;
    SyncPriorityPolicy m_priorityPolicy;
    
    // Users
    std::vector<UserInfo> m_users;
    UserInfo* m_selectedUser;
    
    // Titles
    std::vector<TitleInfo> m_titles;
    
    // FS handles (legacy - prefer ScopedSaveMount)
#ifdef __SWITCH__
    FsFileSystem m_saveFs;
#endif
    std::string m_currentMountName;
    
    // Internal methods
    bool loadUsers();
    bool loadDeviceConfig();
    bool scanTitle(uint64_t titleId);
    bool mountSave(TitleInfo* title);
    void unmountSave();
    bool writeBackupMetadata(TitleInfo* title, const std::string& backupPath, const std::string& backupName, const std::string& source);
    std::string getBackupMetadataPath(const std::string& backupPath) const;
    std::string makeUniqueTempPath(TitleInfo* title, const std::string& prefix) const;
    std::string getSelectedUserId() const;
};

} // namespace core
