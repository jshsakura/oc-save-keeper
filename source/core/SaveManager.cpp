/**
 * oc-save-keeper - Safe save backup and sync for Nintendo Switch
 * Save Manager implementation
 */

#include "core/SaveManager.hpp"
#include "core/MetadataLogic.hpp"
#include "core/SyncLogic.hpp"
#include "fs/FileUtil.hpp"
#include "fs/ScopedSaveMount.hpp"
#include "utils/Paths.hpp"
#include "utils/SettingsStore.hpp"
#include "zip/ZipArchive.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>

namespace core {

namespace {

constexpr const char* BACKUP_BASE_PATH = utils::paths::BACKUPS;
constexpr const char* TEMP_BASE_PATH = utils::paths::TEMP;
constexpr const char* ICON_BASE_PATH = utils::paths::CACHE_TITLE_ICONS;
constexpr const char* USER_ICON_BASE_PATH = utils::paths::CACHE_USER_ICONS;
constexpr const char* DEVICE_ID_PATH = utils::paths::DEVICE_ID;
constexpr const char* DEVICE_PRIORITY_PATH = utils::paths::DEVICE_PRIORITY;
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

struct TitleSaveRecord {
    bool hasAccount = false;
    bool hasDevice = false;
    bool hasSystem = false;
    int64_t accountSize = 0;
    int64_t deviceSize = 0;
    int64_t systemSize = 0;
};

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
    utils::paths::ensureBaseDirectories();

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
    return true;
}

bool SaveManager::loadDeviceConfig() {
    // Persist a per-device identity so cloud backups can carry origin metadata
    // without relying on transient runtime state.
    m_deviceId = utils::SettingsStore::getString("device_id", "");
    if (m_deviceId.empty()) {
        m_deviceId = readLineFromFile(DEVICE_ID_PATH);
    }
    if (looksBrokenDeviceToken(m_deviceId)) {
        char generated[32];
        std::snprintf(generated, sizeof(generated), "dev-%08llX-%08lX",
                      static_cast<unsigned long long>(time(nullptr)),
                      static_cast<unsigned long>(clock()));
        m_deviceId = generated;
        if (!utils::SettingsStore::setString("device_id", m_deviceId)) {
            return false;
        }
    }

    int priorityValue = utils::SettingsStore::getInt("device_priority", 0);
    std::string priorityText = priorityValue > 0 ? std::to_string(priorityValue) : readLineFromFile(DEVICE_PRIORITY_PATH);
    m_deviceLabel = makeDeviceLabel(m_deviceId);
    if (!priorityText.empty()) {
        m_devicePriority = std::max(1, std::atoi(priorityText.c_str()));
    } else {
        m_devicePriority = std::max(1, m_devicePriority);
    }

    if (!utils::SettingsStore::setString("device_id", m_deviceId) ||
        !utils::SettingsStore::setInt("device_priority", m_devicePriority)) {
        return false;
    }

    return true;
}

bool SaveManager::loadUsers() {
    m_users.clear();
#ifdef __SWITCH__
    // Try System first as it's more reliable in Applet Mode for basic info
    Result rc = accountInitialize(AccountServiceType_System);
    if (R_FAILED(rc) && rc != 0x2c7c) {
        rc = accountInitialize(AccountServiceType_Application);
    }
    
    AccountUid uids[ACC_USER_LIST_SIZE]{};
    s32 userCount = 0;
    
    // Attempt to list all, but don't hard-fail if it returns 0xe401
    rc = accountListAllUsers(uids, ACC_USER_LIST_SIZE, &userCount);
    
    if (R_FAILED(rc) || userCount <= 0) {
        // Essential Fallback: Get at least the current user
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
        if (!accountUidIsValid(&uids[i])) continue;

        AccountProfile profile;
        if (R_SUCCEEDED(accountGetProfile(&profile, uids[i]))) {
            AccountUserData userData;
            AccountProfileBase profileBase;
            if (R_SUCCEEDED(accountProfileGet(&profile, &userData, &profileBase))) {
                UserInfo user;
                user.uid = uids[i];
                char idStr[33];
                std::snprintf(idStr, sizeof(idStr), "%016lX%016lX", uids[i].uid[1], uids[i].uid[0]);
                user.id = idStr;
                user.name = profileBase.nickname;
                
                // Icon handling
                char userIconPath[256];
                std::snprintf(userIconPath, sizeof(userIconPath), "%s/%s.jpg", USER_ICON_BASE_PATH, user.id.c_str());
                u32 imageSize = 0;
                if (R_SUCCEEDED(accountProfileGetImageSize(&profile, &imageSize)) && imageSize > 0) {
                    std::vector<unsigned char> image(imageSize);
                    if (R_SUCCEEDED(accountProfileLoadImage(&profile, image.data(), image.size(), &imageSize))) {
                        writeBinaryFile(userIconPath, image.data(), imageSize);
                        user.iconPath = userIconPath;
                    }
                }
                m_users.push_back(user);
            }
            accountProfileClose(&profile);
        }
    }
#else
    // For development/testing, create a mock user
    UserInfo mockUser;
    mockUser.id = "mock-user-1";
    mockUser.name = "TestUser";
    m_users.push_back(mockUser);
#endif
    
    return !m_users.empty();
}

void SaveManager::scanTitles() {
    m_titles.clear();
    
#ifdef __SWITCH__
    std::set<uint64_t> titleIds;
    std::map<uint64_t, TitleSaveRecord> titleSaveInfo;
    const bool isDeviceUser = m_selectedUser && m_selectedUser->id == "device";

    FsSaveDataFilter filter{};
    FsSaveDataInfoReader reader;
    Result rc = fsOpenSaveDataInfoReaderWithFilter(&reader, FsSaveDataSpaceId_User, &filter);
    if (R_SUCCEEDED(rc)) {
        FsSaveDataInfo entries[256]{};
        while (true) {
            s64 recordCount = 0;
            rc = fsSaveDataInfoReaderRead(&reader, entries, 256, &recordCount);
            if (R_FAILED(rc) || recordCount <= 0) {
                break;
            }

            for (s64 i = 0; i < recordCount; ++i) {
                const auto& entry = entries[i];
                if (entry.application_id == 0) {
                    continue;
                }

                // Scan all relevant save types
                if (entry.save_data_type == FsSaveDataType_Account || 
                    entry.save_data_type == FsSaveDataType_Device || 
                    entry.save_data_type == FsSaveDataType_System) {
                    titleIds.insert(entry.application_id);
                    auto& record = titleSaveInfo[entry.application_id];
                    switch (entry.save_data_type) {
                        case FsSaveDataType_Account:
                            record.hasAccount = true;
                            record.accountSize = std::max(record.accountSize, static_cast<int64_t>(entry.size));
                            break;
                        case FsSaveDataType_Device:
                            record.hasDevice = true;
                            record.deviceSize = std::max(record.deviceSize, static_cast<int64_t>(entry.size));
                            break;
                        case FsSaveDataType_System:
                            record.hasSystem = true;
                            record.systemSize = std::max(record.systemSize, static_cast<int64_t>(entry.size));
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        fsSaveDataInfoReaderClose(&reader);
    } else {
        LOG_WARNING("fsOpenSaveDataInfoReaderWithFilter failed: 0x%x", rc);
    }

    rc = nsInitialize();
    if (R_FAILED(rc)) {
        LOG_ERROR("nsInitialize failed: 0x%x", rc);
        return;
    }

    if (!titleIds.empty()) {
        for (uint64_t titleId : titleIds) {
            scanTitle(titleId);
            if (auto* title = getTitleById(titleId)) {
                const auto it = titleSaveInfo.find(titleId);
                if (it != titleSaveInfo.end()) {
                    const auto& record = it->second;
                    
                    title->hasAccountSave = record.hasAccount;
                    title->accountSize = record.accountSize;
                    title->hasDeviceSave = record.hasDevice || record.hasSystem;
                    title->deviceSize = record.hasDevice ? record.deviceSize : record.systemSize;

                    // Set hasSave to true if either exists, default to Account for general UI
                    title->hasSave = title->hasAccountSave || title->hasDeviceSave;
                    if (title->hasAccountSave) {
                        title->saveType = SaveType::Account;
                        title->actualSaveType = SaveType::Account;
                        title->saveSize = title->accountSize;
                    } else if (title->hasDeviceSave) {
                        title->saveType = record.hasDevice ? SaveType::Device : SaveType::System;
                        title->actualSaveType = title->saveType;
                        title->saveSize = title->deviceSize;
                    }
                }
            }
        }
    } else {
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
    rc = fsOpenSaveDataInfoReaderWithFilter(&reader, FsSaveDataSpaceId_User, &filter);
    if (R_SUCCEEDED(rc)) {
        FsSaveDataInfo entries[16]{};
        s64 totalEntries = 0;
        rc = fsSaveDataInfoReaderRead(&reader, entries, 16, &totalEntries);
        fsSaveDataInfoReaderClose(&reader);

        if (R_SUCCEEDED(rc)) {
            const bool isDeviceUser = m_selectedUser && m_selectedUser->id == "device";
            for (s64 i = 0; i < totalEntries && i < 16; i++) {
                const auto& entry = entries[i];
                if (isDeviceUser) {
                    // In device mode, only show system/device saves
                    if (entry.save_data_type == FsSaveDataType_Device || entry.save_data_type == FsSaveDataType_System) {
                        info.hasSave = true;
                        info.saveType = convertSaveType(entry.save_data_type);
                        info.saveSize = entry.size;
                        break;
                    }
                } else {
                    // In user mode, only show account saves
                    if (entry.save_data_type == FsSaveDataType_Account) {
                        info.hasSave = true;
                        info.saveType = SaveType::Account;
                        info.saveSize = entry.size;
                        break;
                    }
                }
            }
        }
    } else if (m_selectedUser && m_selectedUser->id != "device") {
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
    return result;
}

bool SaveManager::backupSave(TitleInfo* title, const std::string& backupName) {
    if (!title || !m_selectedUser) {
        LOG_ERROR("Invalid title or no user selected");
        return false;
    }
    if (!title->hasSave) {
        LOG_ERROR("Backup is only supported for existing save data");
        return false;
    }
    
    // Create backup directory first
    std::string backupDir = getBackupPath(title);
    mkdir(backupDir.c_str(), 0777);
    
    std::string backupPath = backupDir + "/" + backupName;
    LOG_INFO("Backup: Starting backup for %s (%016lX) to %s", title->name.c_str(), title->titleId, backupPath.c_str());
    mkdir(backupPath.c_str(), 0777);
    
#ifdef __SWITCH__
    // Get journal size for proper commits
    int64_t journalSize = fs::getSaveJournalSize(title->titleId);
    
    // Use scoped mount for RAII safety
    bool isDevice = title->actualSaveType == SaveType::Device || title->actualSaveType == SaveType::System;
    fs::ScopedSaveMount saveMount("save", title->titleId, m_selectedUser->uid, isDevice);
    if (!saveMount.isOpen()) {
        LOG_ERROR("Backup: Failed to mount save for %s", title->name.c_str());
        rmdir(backupPath.c_str()); // Clean up empty folder
        return false;
    }
    
    // Copy from save to backup
    std::string savePath = saveMount.getMountPath();
    LOG_INFO("Backup: Copying files from %s to %s", savePath.c_str(), backupPath.c_str());
    if (!fs::copyDirectory(savePath, backupPath, journalSize)) {
        LOG_ERROR("Backup: Failed to copy save data files");
        fs::deleteDirectory(backupPath); // Clean up partial/empty folder
        return false;
    }
    LOG_INFO("Backup: Copy completed successfully");
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
    
    return true;
}

bool SaveManager::restoreSave(TitleInfo* title, const std::string& backupPath) {
    if (!title || !m_selectedUser) {
        LOG_ERROR("Invalid title or no user selected");
        return false;
    }
    if (!title->hasSave) {
        LOG_ERROR("Restore is only supported for existing save data");
        return false;
    }
    
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
    bool isDevice = title->actualSaveType == SaveType::Device || title->actualSaveType == SaveType::System;
    fs::ScopedSaveMount saveMount("save", title->titleId, m_selectedUser->uid, isDevice);
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
#endif
    return true;
}

bool SaveManager::deleteBackup(const std::string& backupPath) {
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

std::string SaveManager::getCloudUserPath() const {
    const std::string userId = sanitizePathComponent(getSelectedUserId().empty() ? "default-user" : getSelectedUserId());
    return "oc-save-keeper/users/" + userId;
}

std::string SaveManager::getCloudDevicesPath() const {
    return getCloudUserPath() + "/devices";
}

std::string SaveManager::getCloudDevicePath(const std::string& deviceId) const {
    const std::string selectedDevice = sanitizePathComponent(deviceId.empty() ? m_deviceId : deviceId);
    return getCloudDevicesPath() + "/" + selectedDevice;
}

std::string SaveManager::getCloudTitlePath(TitleInfo* title) const {
    char titleComponent[32];
    std::snprintf(titleComponent, sizeof(titleComponent), "%016llX",
                  static_cast<unsigned long long>(title ? title->titleId : 0));
    return getCloudUserPath() + "/titles/" + titleComponent;
}

std::string SaveManager::getCloudPath(TitleInfo* title) const {
    return getCloudTitlePath(title) + "/latest.zip";
}

std::string SaveManager::getCloudMetadataPath(TitleInfo* title) const {
    return getCloudTitlePath(title) + "/latest.meta";
}

std::string SaveManager::getCloudRevisionDirectory(TitleInfo* title, const std::string& deviceId) const {
    char titleComponent[32];
    std::snprintf(titleComponent, sizeof(titleComponent), "%016llX",
                  static_cast<unsigned long long>(title ? title->titleId : 0));
    return getCloudDevicePath(deviceId) + "/titles/" + titleComponent + "/revisions";
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
        versions.pop_back();
    }
    
    return true;
}

bool SaveManager::backupAll() {
    auto titles = getTitlesWithSaves();
    bool allSuccess = true;
    int successCount = 0;
    int failCount = 0;
    
    for (auto title : titles) {
        if (createVersionedBackup(title, DEFAULT_MAX_VERSIONS)) {
            successCount++;
        } else {
            LOG_ERROR("Failed to backup: %s", title->name.c_str());
            failCount++;
            allSuccess = false;
        }
    }
    
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

    if (!writeBackupMetadataFile(getBackupMetadataPath(backupPath), meta)) {
        LOG_ERROR("Failed to write metadata for %s", backupPath.c_str());
        return false;
    }
    return true;
}

bool SaveManager::readBackupMetadata(const std::string& backupPath, BackupMetadata& outMeta) {
    return readMetadataFile(getBackupMetadataPath(backupPath), outMeta);
}

bool SaveManager::readMetadataFile(const std::string& metadataPath, BackupMetadata& outMeta) {
    return readBackupMetadataFile(metadataPath, outMeta);
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
    return decideSyncByPolicy(m_priorityPolicy, localMeta, incomingMeta);
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
            copyMetadataFile(incomingMetaPath, getBackupMetadataPath(tempDir));
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

    writeBackupMetadataFile(getBackupMetadataPath(importPath), finalMeta);

    const bool restored = restoreSave(title, importPath);
    fs::deleteDirectory(tempDir);

    if (outReason) {
        *outReason = restored ? decision.reason : "Restore failed after import";
    }
    return restored;
}

} // namespace core
