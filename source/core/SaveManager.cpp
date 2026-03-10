/**
 * oc-save-keeper - Safe save backup and sync for Nintendo Switch
 * Save Manager implementation
 */

#include "core/SaveManager.hpp"
#include "fs/FileUtil.hpp"
#include "fs/ScopedSaveMount.hpp"
#include "zip/ZipArchive.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <cstdlib>

namespace core {

namespace {

constexpr const char* BACKUP_BASE_PATH = "/switch/oc-save-keeper/backups";
constexpr const char* CONFIG_BASE_PATH = "/switch/oc-save-keeper";
constexpr const char* TEMP_BASE_PATH = "/switch/oc-save-keeper/temp";
constexpr const char* ICON_BASE_PATH = "/switch/oc-save-keeper/icons";
constexpr const char* DEVICE_ID_PATH = "/switch/oc-save-keeper/device_id.txt";
constexpr const char* DEVICE_PRIORITY_PATH = "/switch/oc-save-keeper/device_priority.txt";
constexpr const char* META_ENTRY_NAME = ".dropkeep.meta";
constexpr int DEFAULT_MAX_VERSIONS = 5;

std::string sanitizePathComponent(std::string value) {
    if (value.empty()) {
        return "backup";
    }

    for (char& c : value) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
            c == ' ') {
            c = '_';
        }
    }

    return value;
}

std::string readLineFromFile(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        return "";
    }

    char buffer[128] = {0};
    if (!fgets(buffer, sizeof(buffer), file)) {
        fclose(file);
        return "";
    }

    fclose(file);
    buffer[strcspn(buffer, "\r\n")] = 0;
    return buffer;
}

bool writeLineToFile(const char* path, const std::string& value) {
    FILE* file = fopen(path, "w");
    if (!file) {
        return false;
    }

    const bool success = fprintf(file, "%s\n", value.c_str()) > 0;
    fclose(file);
    return success;
}

bool writeBinaryFile(const char* path, const void* data, size_t size) {
    FILE* file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    const bool success = fwrite(data, 1, size, file) == size;
    fclose(file);
    return success;
}

bool looksBrokenDeviceToken(const std::string& value) {
    return value.empty() || value.find("FFFFFFFFFFFFFFFF") != std::string::npos;
}

SaveType convertSaveType(u8 type) {
    switch (type) {
        case FsSaveDataType_System:
            return SaveType::System;
        case FsSaveDataType_Device:
            return SaveType::Device;
        case FsSaveDataType_Bcat:
        case FsSaveDataType_SystemBcat:
            return SaveType::BCAT;
        case FsSaveDataType_Cache:
            return SaveType::Cache;
        case FsSaveDataType_Temporary:
            return SaveType::Temporary;
        case FsSaveDataType_Account:
        default:
            return SaveType::Account;
    }
}

std::string makeDeviceLabel(const std::string& deviceId) {
    if (deviceId.empty()) {
        return "Device";
    }

    const size_t dash = deviceId.find('-');
    const std::string compact = dash == std::string::npos ? deviceId : deviceId.substr(dash + 1);
    const size_t nextDash = compact.find('-');
    const std::string token = nextDash == std::string::npos ? compact : compact.substr(0, nextDash);
    return "Device " + token;
}

} // namespace

SaveManager::SaveManager()
    : m_initialized(false)
    , m_devicePriority(100)
    , m_priorityPolicy(SyncPriorityPolicy::PreferPriority)
    , m_selectedUser(nullptr)
    , m_currentMountName("") {
}

SaveManager::~SaveManager() {
    unmountSave();
}

bool SaveManager::initialize() {
    LOG_INFO("Initializing SaveManager...");
    
    // Create directories
    mkdir(CONFIG_BASE_PATH, 0777);
    mkdir(BACKUP_BASE_PATH, 0777);
    mkdir(ICON_BASE_PATH, 0777);

    if (!loadDeviceConfig()) {
        LOG_ERROR("Failed to load device config");
        return false;
    }
    
    // Load users
    if (!loadUsers()) {
        LOG_ERROR("Failed to load users");
        return false;
    }
    
    // Select first user by default
    if (!m_users.empty()) {
        m_selectedUser = &m_users[0];
    }
    
    m_initialized = true;
    LOG_INFO("SaveManager initialized with %zu users", m_users.size());
    return true;
}

bool SaveManager::loadDeviceConfig() {
    // Persist a per-device identity so cloud backups can carry origin metadata
    // without relying on transient runtime state.
    m_deviceId = readLineFromFile(DEVICE_ID_PATH);
    if (looksBrokenDeviceToken(m_deviceId)) {
        char generated[32];
        std::snprintf(generated, sizeof(generated), "dev-%08llX-%08lX",
                      static_cast<unsigned long long>(time(nullptr)),
                      static_cast<unsigned long>(clock()));
        m_deviceId = generated;
        if (!writeLineToFile(DEVICE_ID_PATH, m_deviceId)) {
            return false;
        }
    }

    std::string priorityText = readLineFromFile(DEVICE_PRIORITY_PATH);
    m_deviceLabel = makeDeviceLabel(m_deviceId);
    if (!priorityText.empty()) {
        m_devicePriority = std::max(1, std::atoi(priorityText.c_str()));
    } else if (!writeLineToFile(DEVICE_PRIORITY_PATH, std::to_string(m_devicePriority))) {
        return false;
    }

    LOG_INFO("Loaded device config: id=%s label=%s priority=%d",
             m_deviceId.c_str(), m_deviceLabel.c_str(), m_devicePriority);
    return true;
}

bool SaveManager::loadUsers() {
    m_users.clear();
    
#ifdef __SWITCH__
    Result rc = accountInitialize(AccountServiceType_Application);
    if (R_FAILED(rc)) {
        LOG_WARNING("accountInitialize(Application) failed: 0x%x", rc);
        rc = accountInitialize(AccountServiceType_System);
    }
    if (R_FAILED(rc)) {
        LOG_WARNING("accountInitialize(System) failed: 0x%x", rc);
        rc = accountInitialize(AccountServiceType_Administrator);
    }
    if (R_FAILED(rc)) {
        LOG_ERROR("accountInitialize failed: 0x%x", rc);
        UserInfo fallbackUser;
        fallbackUser.id = "device-user";
        fallbackUser.name = "Default User";
        fallbackUser.iconPath = "";
        m_users.push_back(fallbackUser);
        return true;
    }

    AccountUid uids[ACC_USER_LIST_SIZE]{};
    s32 userCount = 0;
    
    rc = accountListAllUsers(uids, ACC_USER_LIST_SIZE, &userCount);
    if (R_FAILED(rc) || userCount <= 0) {
        LOG_WARNING("accountListAllUsers failed or empty: 0x%x", rc);

        AccountUid fallbackUid{};
        if (R_SUCCEEDED(accountGetPreselectedUser(&fallbackUid)) && accountUidIsValid(&fallbackUid)) {
            uids[0] = fallbackUid;
            userCount = 1;
        } else if (R_SUCCEEDED(accountGetLastOpenedUser(&fallbackUid)) && accountUidIsValid(&fallbackUid)) {
            uids[0] = fallbackUid;
            userCount = 1;
        }
    }
    
    for (s32 i = 0; i < userCount; i++) {
        if (!accountUidIsValid(&uids[i])) {
            continue;
        }

        AccountProfile profile;
        rc = accountGetProfile(&profile, uids[i]);
        if (R_FAILED(rc)) {
            UserInfo user;
            user.uid = uids[i];
            user.id = std::to_string(static_cast<unsigned long long>(uids[i].uid[0])) + "-" +
                      std::to_string(static_cast<unsigned long long>(uids[i].uid[1]));
            user.name = "User";
            user.iconPath = "";
            m_users.push_back(user);
            continue;
        }
        
        AccountUserData userData;
        AccountProfileBase profileBase;
        rc = accountProfileGet(&profile, &userData, &profileBase);
        if (R_SUCCEEDED(rc)) {
            UserInfo user;
            user.uid = uids[i];
            user.id = std::to_string(static_cast<unsigned long long>(uids[i].uid[0])) + "-" +
                      std::to_string(static_cast<unsigned long long>(uids[i].uid[1]));
            user.name = profileBase.nickname;
            user.iconPath = "";
            m_users.push_back(user);
            LOG_INFO("Found user: %s", user.name.c_str());
        }
        
        accountProfileClose(&profile);
    }
    
    accountExit();

    if (m_users.empty()) {
        UserInfo fallbackUser;
        fallbackUser.id = "device-user";
        fallbackUser.name = "Default User";
        fallbackUser.iconPath = "";
        m_users.push_back(fallbackUser);
    }
#else
    // For development/testing, create a mock user
    UserInfo mockUser;
    mockUser.uidPlaceholder = 1;
    mockUser.id = "mock-user-1";
    mockUser.name = "TestUser";
    m_users.push_back(mockUser);
    LOG_INFO("Created mock user for development");
#endif
    
    return !m_users.empty();
}

void SaveManager::scanTitles() {
    LOG_INFO("Scanning titles...");
    m_titles.clear();
    
#ifdef __SWITCH__
    Result rc = nsInitialize();
    if (R_FAILED(rc)) {
        LOG_ERROR("nsInitialize failed: 0x%x", rc);
        return;
    }

    // Scan SD card for installed titles
    NsApplicationRecord records[1024];
    s32 recordCount = 0;
    
    rc = nsListApplicationRecord(records, 1024, 0, &recordCount);
    if (R_FAILED(rc)) {
        LOG_ERROR("nsListApplicationRecord failed: 0x%x", rc);
        nsExit();
        return;
    }
    
    for (s32 i = 0; i < recordCount; i++) {
        scanTitle(records[i].application_id);
    }

    nsExit();
#else
    // Mock titles for development
    TitleInfo mockTitle;
    mockTitle.titleId = 0x01006A800016E000;
    mockTitle.name = "The Legend of Zelda: BOTW";
    mockTitle.publisher = "Nintendo";
    mockTitle.hasSave = true;
    mockTitle.saveType = SaveType::Account;
    mockTitle.saveSize = 0;
    mockTitle.isFavorite = false;
    m_titles.push_back(mockTitle);
#endif
    
    LOG_INFO("Found %zu titles", m_titles.size());
}

bool SaveManager::scanTitle(uint64_t titleId) {
    TitleInfo info;
    info.titleId = titleId;
    info.hasSave = false;
    info.saveType = SaveType::Account;
    info.saveSize = 0;
    info.isFavorite = false;
    
#ifdef __SWITCH__
    // Get title name
    NsApplicationControlData controlData;
    size_t controlSize = 0;
    
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, 
                                            titleId, &controlData, 
                                            sizeof(controlData), &controlSize);
    
    if (R_SUCCEEDED(rc) && controlSize >= sizeof(NacpStruct)) {
        NacpLanguageEntry* langEntry = nullptr;
        if (R_SUCCEEDED(nacpGetLanguageEntry(&controlData.nacp, &langEntry)) && langEntry) {
            info.name = langEntry->name;
            info.publisher = langEntry->author;
        } else {
            info.name = controlData.nacp.lang[SetLanguage_ENUS].name;
            info.publisher = controlData.nacp.lang[SetLanguage_ENUS].author;
        }
    } else {
        // Fallback: use title ID as name
        char name[32];
        snprintf(name, sizeof(name), "Title %016lX", titleId);
        info.name = name;
    }
    
    // Cache the title icon from NACP control data so the UI can load it later.
    char iconPath[256];
    std::snprintf(iconPath, sizeof(iconPath), "%s/%016lX.jpg", ICON_BASE_PATH, titleId);
    info.iconPath = iconPath;
    FILE* iconFile = fopen(iconPath, "rb");
    if (iconFile) {
        fclose(iconFile);
    } else if (R_SUCCEEDED(rc) && controlSize > sizeof(NacpStruct)) {
        const size_t iconSize = std::min(sizeof(controlData.icon), controlSize - sizeof(NacpStruct));
        writeBinaryFile(iconPath, controlData.icon, iconSize);
    }

    FsSaveDataFilter filter{};
    filter.filter_by_application_id = true;
    filter.attr.application_id = titleId;

    FsSaveDataInfoReader reader;
    rc = fsOpenSaveDataInfoReaderWithFilter(&reader, FsSaveDataSpaceId_All, &filter);
    if (R_SUCCEEDED(rc)) {
        FsSaveDataInfo entries[16]{};
        s64 totalEntries = 0;
        rc = fsSaveDataInfoReaderRead(&reader, entries, 16, &totalEntries);
        fsSaveDataInfoReaderClose(&reader);

        if (R_SUCCEEDED(rc)) {
            for (s64 i = 0; i < totalEntries && i < 16; i++) {
                info.hasSave = true;
                info.saveType = convertSaveType(entries[i].save_data_type);
                info.saveSize = entries[i].size;

                // Prefer account save when available, but still keep device/shared
                // saves visible when that's the only save type present.
                if (entries[i].save_data_type == FsSaveDataType_Account) {
                    info.saveType = SaveType::Account;
                    break;
                }
            }
        }
    } else if (m_selectedUser) {
        // Fallback for older service paths.
        FsFileSystem tmpFs;
        rc = fsOpen_SaveData(&tmpFs, titleId, m_selectedUser->uid);
        if (R_SUCCEEDED(rc)) {
            info.hasSave = true;
            info.saveType = SaveType::Account;
            fsFsClose(&tmpFs);
        }
    }
#else
    char name[32];
    snprintf(name, sizeof(name), "Title %016lX", titleId);
    info.name = name;
    info.hasSave = true;
#endif
    
    // Set paths
    char backupPath[256];
    snprintf(backupPath, sizeof(backupPath), "%s/%016lX", BACKUP_BASE_PATH, titleId);
    info.savePath = backupPath;
    
    m_titles.push_back(info);
    return true;
}

bool SaveManager::selectUser(size_t index) {
    if (index >= m_users.size()) return false;
    m_selectedUser = &m_users[index];
    
    // Rescan titles for new user
    scanTitles();
    return true;
}

TitleInfo* SaveManager::getTitle(size_t index) {
    if (index >= m_titles.size()) return nullptr;
    return &m_titles[index];
}

TitleInfo* SaveManager::getTitleById(uint64_t titleId) {
    for (auto& title : m_titles) {
        if (title.titleId == titleId) return &title;
    }
    return nullptr;
}

std::vector<TitleInfo*> SaveManager::getTitlesWithSaves() {
    std::vector<TitleInfo*> result;
    for (auto& title : m_titles) {
        if (title.hasSave) {
            result.push_back(&title);
        }
    }

    // When account services are unavailable on homebrew launch, save probing can
    // fail even though installed titles exist. In that case, still show the
    // scanned titles so the UI remains usable and the user can see what was
    // detected on the system.
    if (result.empty()) {
        for (auto& title : m_titles) {
            result.push_back(&title);
        }
    }

    return result;
}

bool SaveManager::backupSave(TitleInfo* title, const std::string& backupName) {
    if (!title || !m_selectedUser) {
        LOG_ERROR("Invalid title or no user selected");
        return false;
    }
    
    LOG_INFO("Backing up: %s -> %s", title->name.c_str(), backupName.c_str());
    
    // Create backup directory first
    std::string backupDir = getBackupPath(title);
    mkdir(backupDir.c_str(), 0777);
    
    std::string backupPath = backupDir + "/" + backupName;
    mkdir(backupPath.c_str(), 0777);
    
#ifdef __SWITCH__
    // Get journal size for proper commits
    int64_t journalSize = fs::getSaveJournalSize(title->titleId);
    
    // Use scoped mount for RAII safety
    fs::ScopedSaveMount saveMount("save", title->titleId, m_selectedUser->uid);
    if (!saveMount.isOpen()) {
        LOG_ERROR("Failed to mount save for backup");
        return false;
    }
    
    // Copy from save to backup
    std::string savePath = saveMount.getMountPath();
    if (!fs::copyDirectory(savePath, backupPath, journalSize)) {
        LOG_ERROR("Failed to copy save data");
        return false;
    }
#else
    // For development, just create a marker file
    FILE* f = fopen((backupPath + "/save.meta").c_str(), "w");
    if (f) {
        fprintf(f, "Backup of %s\n", title->name.c_str());
        fclose(f);
    }
#endif
    
    // Update save size
    title->saveSize = fs::getDirectorySize(backupPath);
    writeBackupMetadata(title, backupPath, backupName, "local");
    
    LOG_INFO("Backup complete: %s (%ld bytes)", backupName.c_str(), title->saveSize);
    return true;
}

bool SaveManager::restoreSave(TitleInfo* title, const std::string& backupPath) {
    if (!title || !m_selectedUser) {
        LOG_ERROR("Invalid title or no user selected");
        return false;
    }
    
    LOG_INFO("Restoring: %s from %s", title->name.c_str(), backupPath.c_str());
    
    // Verify backup exists
    DIR* dir = opendir(backupPath.c_str());
    if (!dir) {
        LOG_ERROR("Backup not found: %s", backupPath.c_str());
        return false;
    }
    closedir(dir);
    
#ifdef __SWITCH__
    // Get journal size for proper commits
    int64_t journalSize = fs::getSaveJournalSize(title->titleId);
    
    // Use scoped mount for RAII safety
    fs::ScopedSaveMount saveMount("save", title->titleId, m_selectedUser->uid);
    if (!saveMount.isOpen()) {
        LOG_ERROR("Failed to mount save for restore");
        return false;
    }
    
    std::string savePath = saveMount.getMountPath();
    
    // Clear existing save data first
    fs::deleteDirectory(savePath);
    mkdir(savePath.c_str(), 0777);
    
    // Copy from backup to save
    if (!fs::copyDirectory(backupPath, savePath, journalSize)) {
        LOG_ERROR("Failed to restore save data");
        return false;
    }
    
    // CRITICAL: Commit the save data
    if (!saveMount.commit()) {
        LOG_ERROR("Failed to commit save data");
        return false;
    }
#else
    LOG_INFO("Development mode: skipping actual restore");
#endif
    
    LOG_INFO("Restore complete: %s", title->name.c_str());
    return true;
}

bool SaveManager::deleteBackup(const std::string& backupPath) {
    LOG_INFO("Deleting backup: %s", backupPath.c_str());
    const bool deleted = fs::deleteDirectory(backupPath);
    remove(getBackupMetadataPath(backupPath).c_str());
    remove((backupPath + ".zip").c_str());
    return deleted;
}

bool SaveManager::mountSave(TitleInfo* title) {
    if (!m_selectedUser) return false;
    
#ifdef __SWITCH__
    // This is kept for backward compatibility
    // Prefer using ScopedSaveMount instead
    Result rc = fsOpen_SaveData(&m_saveFs, title->titleId, m_selectedUser->uid);
    if (R_FAILED(rc)) {
        LOG_ERROR("fsOpen_SaveData failed: 0x%x", rc);
        return false;
    }
    m_currentMountName = "save";
#endif
    
    return true;
}

void SaveManager::unmountSave() {
#ifdef __SWITCH__
    if (!m_currentMountName.empty()) {
        fsdevUnmountDevice(m_currentMountName.c_str());
        m_currentMountName.clear();
    }
#endif
}

std::string SaveManager::getBackupPath(TitleInfo* title) const {
    char path[256];
    snprintf(path, sizeof(path), "%s/%016lX", BACKUP_BASE_PATH, title->titleId);
    return path;
}

std::string SaveManager::getCloudPath(TitleInfo* title) const {
    // Sanitize name for cloud path (remove special chars)
    std::string safeName = sanitizePathComponent(title->name);
    return safeName + "/latest.zip";
}

std::string SaveManager::getCloudMetadataPath(TitleInfo* title) const {
    std::string safeName = sanitizePathComponent(title->name);
    return safeName + "/latest.meta";
}

std::vector<BackupVersion> SaveManager::getBackupVersions(TitleInfo* title) {
    std::vector<BackupVersion> versions;
    
    std::string backupPath = getBackupPath(title);
    DIR* dir = opendir(backupPath.c_str());
    if (!dir) return versions;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
            BackupVersion ver;
            ver.name = entry->d_name;
            ver.path = backupPath + "/" + entry->d_name;
            ver.isCloudSynced = false;
            ver.size = 0;
            
            // Get timestamp from directory
            struct stat st;
            if (stat(ver.path.c_str(), &st) == 0) {
                ver.timestamp = st.st_mtime;
                ver.size = fs::getDirectorySize(ver.path);
            }

            BackupMetadata meta;
            if (readBackupMetadata(ver.path, meta)) {
                ver.deviceId = meta.deviceId;
                ver.deviceLabel = meta.deviceLabel;
                ver.userId = meta.userId;
                ver.userName = meta.userName;
                ver.source = meta.source;
                if (meta.createdAt != 0) {
                    ver.timestamp = meta.createdAt;
                }
            }
            
            versions.push_back(ver);
        }
    }
    
    closedir(dir);
    
    // Sort by timestamp (newest first)
    std::sort(versions.begin(), versions.end(), 
        [](const BackupVersion& a, const BackupVersion& b) {
            return a.timestamp > b.timestamp;
        });
    
    return versions;
}

bool SaveManager::createVersionedBackup(TitleInfo* title, int maxVersions) {
    // Create timestamped backup name
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char name[64];
    strftime(name, sizeof(name), "%Y%m%d_%H%M%S", t);
    
    // Create backup
    if (!backupSave(title, name)) {
        return false;
    }
    
    // Clean up old versions
    auto versions = getBackupVersions(title);
    while (versions.size() > (size_t)maxVersions) {
        BackupVersion& oldest = versions.back();
        deleteBackup(oldest.path);
        LOG_INFO("Deleted old backup: %s", oldest.name.c_str());
        versions.pop_back();
    }
    
    return true;
}

bool SaveManager::backupAll() {
    auto titles = getTitlesWithSaves();
    bool allSuccess = true;
    int successCount = 0;
    int failCount = 0;
    
    LOG_INFO("Starting backup all (%zu titles)...", titles.size());
    
    for (auto title : titles) {
        if (createVersionedBackup(title, DEFAULT_MAX_VERSIONS)) {
            successCount++;
        } else {
            LOG_ERROR("Failed to backup: %s", title->name.c_str());
            failCount++;
            allSuccess = false;
        }
    }
    
    LOG_INFO("Backup all complete: %d success, %d failed", successCount, failCount);
    return allSuccess;
}

std::string SaveManager::getDeviceId() {
    return m_deviceId;
}

std::string SaveManager::getSelectedUserId() const {
    if (!m_selectedUser) {
        return "";
    }
    return m_selectedUser->id;
}

std::string SaveManager::getBackupMetadataPath(const std::string& backupPath) const {
    return backupPath + ".meta";
}

bool SaveManager::writeBackupMetadata(TitleInfo* title,
                                      const std::string& backupPath,
                                      const std::string& backupName,
                                      const std::string& source) {
    BackupMetadata meta;
    meta.titleId = title ? title->titleId : 0;
    meta.titleName = title ? title->name : "";
    meta.backupName = backupName;
    meta.revisionId = backupName;
    meta.deviceId = m_deviceId;
    meta.deviceLabel = m_deviceLabel;
    meta.userId = getSelectedUserId();
    meta.userName = m_selectedUser ? m_selectedUser->name : "";
    meta.source = source;
    meta.createdAt = time(nullptr);
    meta.devicePriority = m_devicePriority;
    meta.size = fs::getDirectorySize(backupPath);

    FILE* file = fopen(getBackupMetadataPath(backupPath).c_str(), "w");
    if (!file) {
        LOG_ERROR("Failed to write metadata for %s", backupPath.c_str());
        return false;
    }

    fprintf(file, "title_id=%llu\n", static_cast<unsigned long long>(meta.titleId));
    fprintf(file, "title_name=%s\n", meta.titleName.c_str());
    fprintf(file, "backup_name=%s\n", meta.backupName.c_str());
    fprintf(file, "revision_id=%s\n", meta.revisionId.c_str());
    fprintf(file, "device_id=%s\n", meta.deviceId.c_str());
    fprintf(file, "device_label=%s\n", meta.deviceLabel.c_str());
    fprintf(file, "user_id=%s\n", meta.userId.c_str());
    fprintf(file, "user_name=%s\n", meta.userName.c_str());
    fprintf(file, "source=%s\n", meta.source.c_str());
    fprintf(file, "created_at=%lld\n", static_cast<long long>(meta.createdAt));
    fprintf(file, "device_priority=%d\n", meta.devicePriority);
    fprintf(file, "size=%lld\n", static_cast<long long>(meta.size));

    fclose(file);
    return true;
}

bool SaveManager::readBackupMetadata(const std::string& backupPath, BackupMetadata& outMeta) {
    return readMetadataFile(getBackupMetadataPath(backupPath), outMeta);
}

bool SaveManager::readMetadataFile(const std::string& metadataPath, BackupMetadata& outMeta) {
    FILE* file = fopen(metadataPath.c_str(), "r");
    if (!file) {
        return false;
    }

    outMeta = {};
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        char* delimiter = strchr(line, '=');
        if (!delimiter) {
            continue;
        }

        *delimiter = 0;
        const char* key = line;
        const char* value = delimiter + 1;

        if (strcmp(key, "title_id") == 0) {
            outMeta.titleId = std::strtoull(value, nullptr, 10);
        } else if (strcmp(key, "title_name") == 0) {
            outMeta.titleName = value;
        } else if (strcmp(key, "backup_name") == 0) {
            outMeta.backupName = value;
        } else if (strcmp(key, "revision_id") == 0) {
            outMeta.revisionId = value;
        } else if (strcmp(key, "device_id") == 0) {
            outMeta.deviceId = value;
        } else if (strcmp(key, "device_label") == 0) {
            outMeta.deviceLabel = value;
        } else if (strcmp(key, "user_id") == 0) {
            outMeta.userId = value;
        } else if (strcmp(key, "user_name") == 0) {
            outMeta.userName = value;
        } else if (strcmp(key, "source") == 0) {
            outMeta.source = value;
        } else if (strcmp(key, "created_at") == 0) {
            outMeta.createdAt = static_cast<std::time_t>(std::atoll(value));
        } else if (strcmp(key, "device_priority") == 0) {
            outMeta.devicePriority = std::atoi(value);
        } else if (strcmp(key, "size") == 0) {
            outMeta.size = std::atoll(value);
        }
    }

    fclose(file);
    return outMeta.titleId != 0 || !outMeta.backupName.empty();
}

SyncDecision SaveManager::evaluateIncomingMetadata(TitleInfo* title, const BackupMetadata& incomingMeta) const {
    if (!title) {
        return {false, "No title selected"};
    }

    // Compare against the newest local snapshot that actually has metadata.
    // This keeps the decision cheap and avoids touching the larger archive first.
    BackupMetadata latestLocalMeta;
    BackupMetadata* latestLocalMetaPtr = nullptr;
    auto versions = const_cast<SaveManager*>(this)->getBackupVersions(title);
    for (const auto& version : versions) {
        if (const_cast<SaveManager*>(this)->readBackupMetadata(version.path, latestLocalMeta)) {
            latestLocalMetaPtr = &latestLocalMeta;
            break;
        }
    }

    return decideSync(latestLocalMetaPtr, incomingMeta);
}

SyncDecision SaveManager::decideSync(const BackupMetadata* localMeta, const BackupMetadata& incomingMeta) const {
    if (!localMeta) {
        return {true, "No local backup metadata found"};
    }

    if (!incomingMeta.userId.empty() && !localMeta->userId.empty() && incomingMeta.userId != localMeta->userId) {
        return {false, "Different user save detected; manual restore only"};
    }

    if (!incomingMeta.deviceId.empty() && !localMeta->deviceId.empty() && incomingMeta.deviceId != localMeta->deviceId) {
        if (m_priorityPolicy == SyncPriorityPolicy::PreferPriority) {
            if (incomingMeta.devicePriority > localMeta->devicePriority) {
                return {true, "Different device save accepted by higher priority"};
            }
            if (incomingMeta.devicePriority < localMeta->devicePriority) {
                return {false, "Different device save kept out by local priority"};
            }
        }
    }

    if (m_priorityPolicy == SyncPriorityPolicy::PreferPriority) {
        if (incomingMeta.devicePriority > localMeta->devicePriority) {
            return {true, "Incoming backup has higher device priority"};
        }
        if (incomingMeta.devicePriority < localMeta->devicePriority) {
            return {false, "Local backup has higher device priority"};
        }
    }

    if (incomingMeta.createdAt > localMeta->createdAt) {
        return {true, "Incoming backup is newer"};
    }
    if (incomingMeta.createdAt < localMeta->createdAt) {
        return {false, "Local backup is newer"};
    }

    if (m_priorityPolicy == SyncPriorityPolicy::PreferNewest) {
        return {false, "Timestamps match; keeping local backup"};
    }

    return {false, "Priority and timestamp are tied; keeping local backup"};
}

std::string SaveManager::exportBackupArchive(TitleInfo* title, const std::string& backupPath) {
    if (!title) {
        return "";
    }

    DIR* dir = opendir(backupPath.c_str());
    if (!dir) {
        LOG_ERROR("Backup path does not exist: %s", backupPath.c_str());
        return "";
    }
    closedir(dir);

    BackupMetadata meta;
    if (!readBackupMetadata(backupPath, meta)) {
        const size_t slash = backupPath.find_last_of('/');
        const std::string backupName = slash == std::string::npos ? backupPath : backupPath.substr(slash + 1);
        if (!writeBackupMetadata(title, backupPath, backupName, "local")) {
            return "";
        }
    }

    const std::string zipPath = backupPath + ".zip";
    zip::ZipArchive archive;
    if (!archive.create(zipPath)) {
        return "";
    }
    if (!archive.addDirectory(backupPath) ||
        !archive.addFile(getBackupMetadataPath(backupPath), META_ENTRY_NAME)) {
        archive.close();
        return "";
    }

    archive.close();
    return zipPath;
}

std::string SaveManager::makeUniqueTempPath(TitleInfo* title, const std::string& prefix) const {
    mkdir(TEMP_BASE_PATH, 0777);
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s/%s_%016llX_%lld",
                  TEMP_BASE_PATH,
                  prefix.c_str(),
                  static_cast<unsigned long long>(title ? title->titleId : 0),
                  static_cast<long long>(time(nullptr)));
    return buffer;
}

bool SaveManager::importBackupArchive(TitleInfo* title, const std::string& archivePath, std::string* outReason, bool skipConflictCheck) {
    if (!title) {
        if (outReason) *outReason = "No title selected";
        return false;
    }

    // Extraction stays on disk so Switch RAM usage is bounded by our fixed IO buffers.
    std::string tempDir = makeUniqueTempPath(title, "import");
    if (!zip::unzipToDirectory(archivePath, tempDir)) {
        if (outReason) *outReason = "Failed to extract downloaded archive";
        return false;
    }

    BackupMetadata incomingMeta;
    const std::string incomingMetaPath = tempDir + "/" + META_ENTRY_NAME;
    {
        FILE* metaFile = fopen(incomingMetaPath.c_str(), "r");
        if (metaFile) {
            fclose(metaFile);

            FILE* dst = fopen(getBackupMetadataPath(tempDir).c_str(), "w");
            FILE* src = fopen(incomingMetaPath.c_str(), "r");
            if (src && dst) {
                char buffer[512];
                while (fgets(buffer, sizeof(buffer), src)) {
                    fputs(buffer, dst);
                }
            }
            if (src) fclose(src);
            if (dst) fclose(dst);
        }
    }

    const bool hasIncomingMeta = readBackupMetadata(tempDir, incomingMeta);
    remove(incomingMetaPath.c_str());
    remove(getBackupMetadataPath(tempDir).c_str());

    SyncDecision decision = hasIncomingMeta
        ? (skipConflictCheck ? SyncDecision{true, "Metadata precheck accepted incoming backup"}
                             : evaluateIncomingMetadata(title, incomingMeta))
        : SyncDecision{true, "Incoming backup has no metadata; applying as recovery import"};

    if (!decision.useIncoming) {
        fs::deleteDirectory(tempDir);
        if (outReason) *outReason = decision.reason;
        return true;
    }

    // Keep one rollback point before any incoming cloud restore touches the live save.
    createVersionedBackup(title, DEFAULT_MAX_VERSIONS);

    const std::string baseName = hasIncomingMeta && !incomingMeta.backupName.empty()
        ? incomingMeta.backupName
        : ("cloud_" + std::to_string(static_cast<long long>(time(nullptr))));
    const std::string importName = sanitizePathComponent(baseName) + "_import";
    const std::string importPath = getBackupPath(title) + "/" + importName;

    fs::deleteDirectory(importPath);
    if (!fs::copyDirectory(tempDir, importPath, fs::DEFAULT_JOURNAL_SIZE)) {
        fs::deleteDirectory(tempDir);
        if (outReason) *outReason = "Failed to stage imported backup";
        return false;
    }

    BackupMetadata finalMeta = incomingMeta;
    if (!hasIncomingMeta) {
        finalMeta.titleId = title->titleId;
        finalMeta.titleName = title->name;
        finalMeta.backupName = importName;
        finalMeta.revisionId = importName;
        finalMeta.createdAt = time(nullptr);
        finalMeta.deviceId = "unknown";
        finalMeta.deviceLabel = "Unknown device";
        finalMeta.userId = getSelectedUserId();
        finalMeta.userName = m_selectedUser ? m_selectedUser->name : "";
        finalMeta.devicePriority = 0;
        finalMeta.size = fs::getDirectorySize(importPath);
    }
    finalMeta.backupName = importName;
    finalMeta.revisionId = importName;
    finalMeta.source = "cloud";

    FILE* file = fopen(getBackupMetadataPath(importPath).c_str(), "w");
    if (file) {
        fprintf(file, "title_id=%llu\n", static_cast<unsigned long long>(finalMeta.titleId));
        fprintf(file, "title_name=%s\n", finalMeta.titleName.c_str());
        fprintf(file, "backup_name=%s\n", finalMeta.backupName.c_str());
        fprintf(file, "revision_id=%s\n", finalMeta.revisionId.c_str());
        fprintf(file, "device_id=%s\n", finalMeta.deviceId.c_str());
        fprintf(file, "device_label=%s\n", finalMeta.deviceLabel.c_str());
        fprintf(file, "user_id=%s\n", finalMeta.userId.c_str());
        fprintf(file, "user_name=%s\n", finalMeta.userName.c_str());
        fprintf(file, "source=%s\n", finalMeta.source.c_str());
        fprintf(file, "created_at=%lld\n", static_cast<long long>(finalMeta.createdAt));
        fprintf(file, "device_priority=%d\n", finalMeta.devicePriority);
        fprintf(file, "size=%lld\n", static_cast<long long>(finalMeta.size));
        fclose(file);
    }

    const bool restored = restoreSave(title, importPath);
    fs::deleteDirectory(tempDir);

    if (outReason) {
        *outReason = restored ? decision.reason : "Restore failed after import";
    }
    return restored;
}

} // namespace core
