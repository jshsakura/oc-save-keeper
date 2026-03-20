/**
 * oc-save-keeper - Safe save backup and sync for Nintendo Switch
 * Save Manager implementation
 */

#include "core/SaveManager.hpp"
#include "core/MetadataLogic.hpp"
#include "core/SyncLogic.hpp"
#include "fs/FileUtil.hpp"
#include "fs/ScopedSaveMount.hpp"
#include "utils/Language.hpp"
#include "utils/Paths.hpp"
#include "utils/SettingsStore.hpp"
#include "zip/ZipArchive.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <unordered_set>

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
constexpr int TRASH_RETENTION_DAYS_DEFAULT = 30;

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
    fs::ScopedFile file(fopen(path, "r"));
    if (!file) {
        return "";
    }

    char buffer[128] = {0};
    if (!fgets(buffer, sizeof(buffer), file.get())) {
        return "";
    }

    buffer[strcspn(buffer, "\r\n")] = 0;
    return buffer;
}



bool writeBinaryFile(const char* path, const void* data, size_t size) {
    fs::ScopedFile file(fopen(path, "wb"));
    if (!file) {
        return false;
    }

    return fwrite(data, 1, size, file.get()) == size;
}

bool looksBrokenDeviceToken(const std::string& value) {
    return value.empty() || value.length() < 8;
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
        return "Unknown";
    }

    const size_t dash = deviceId.find('-');
    const std::string compact = dash == std::string::npos ? deviceId : deviceId.substr(dash + 1);
    const size_t nextDash = compact.find('-');
    const std::string token = nextDash == std::string::npos ? compact : compact.substr(0, nextDash);
    return token;
}

struct TitleSaveRecord {
    bool hasAccount = false;
    bool hasDevice = false;
    bool hasSystem = false;
    int64_t accountSize = 0;
    int64_t deviceSize = 0;
    int64_t systemSize = 0;
};

#ifdef __SWITCH__
int64_t measureLiveSaveUsage(uint64_t titleId, AccountUid uid, SaveType saveType) {
    fs::ScopedSaveMount saveMount("sizemount", titleId, uid, saveType);
    if (!saveMount.isOpen()) {
        return -1;
    }
    return fs::getDirectorySize(saveMount.getMountPath());
}
#endif

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
        char generated[24];
        std::snprintf(generated, sizeof(generated), "dev-%08X-%04X",
                      static_cast<unsigned int>(time(nullptr) & 0xFFFFFFFF),
                      static_cast<unsigned int>(clock() & 0xFFFF));
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
                if (m_users.size() >= MAX_USERS) {
                    LOG_WARNING("Max users limit reached (%zu), skipping remaining", MAX_USERS);
                    break;
                }
                m_users.push_back(user);
            }
            accountProfileClose(&profile);
        }
    }
    accountExit();
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
    std::vector<uint64_t> titleIds;
    std::unordered_set<uint64_t> titleIdSet;
    std::map<uint64_t, TitleSaveRecord> titleSaveInfo;
    const bool isDeviceUser = m_selectedUser && m_selectedUser->id == "device";

    AccountUid accountUsageUid{};
    bool hasAccountUsageUid = false;
    if (m_selectedUser && m_selectedUser->id != "device") {
        accountUsageUid = m_selectedUser->uid;
        hasAccountUsageUid = true;
    } else {
        for (const auto& user : m_users) {
            if (user.id != "device") {
                accountUsageUid = user.uid;
                hasAccountUsageUid = true;
                break;
            }
        }
    }

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
                    if (titleIdSet.insert(entry.application_id).second) {
                        titleIds.push_back(entry.application_id);
                    }
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

                    if (title->hasAccountSave && hasAccountUsageUid) {
                        const int64_t usedAccountSize = measureLiveSaveUsage(titleId, accountUsageUid, SaveType::Account);
                        if (usedAccountSize >= 0) {
                            title->accountSize = usedAccountSize;
                        }
                    }
                    if (title->hasDeviceSave) {
                        const SaveType deviceType = record.hasDevice ? SaveType::Device : SaveType::System;
                        const int64_t usedDeviceSize = measureLiveSaveUsage(titleId, AccountUid{}, deviceType);
                        if (usedDeviceSize >= 0) {
                            title->deviceSize = usedDeviceSize;
                        }
                    }

                    // Set hasSave to true if either exists
                    title->hasSave = title->hasAccountSave || title->hasDeviceSave;
                    
                    // Priority: if it's a "device only" save like ACNH, it will correctly pick Device
                    // Otherwise, if current view is Device view, pick Device.
                    if (isDeviceUser && title->hasDeviceSave) {
                        title->saveType = record.hasDevice ? SaveType::Device : SaveType::System;
                        title->actualSaveType = title->saveType;
                        title->saveSize = title->deviceSize;
                    } else if (title->hasAccountSave) {
                        title->saveType = SaveType::Account;
                        title->actualSaveType = SaveType::Account;
                        title->saveSize = title->accountSize;
                    } else if (title->hasDeviceSave) {
                        // Fallback for games that only have Device saves
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
    info.installOrder = static_cast<int>(m_titles.size());
    
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
    if (m_titles.size() >= MAX_TITLES) {
        LOG_WARNING("Max titles limit reached (%zu), skipping remaining", MAX_TITLES);
    }
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
    fs::ScopedSaveMount saveMount("save", title->titleId, m_selectedUser->uid, title->actualSaveType);
    if (!saveMount.isOpen()) {
        LOG_ERROR("Backup: Failed to mount save for %s", title->name.c_str());
        rmdir(backupPath.c_str()); // Clean up empty folder
        return false;
    }
    
    // Copy from save to backup
    std::string savePath = saveMount.getMountPath();
    LOG_INFO("Backup: Copying files from %s to %s", savePath.c_str(), backupPath.c_str());
    // Note: We don't need physical commit for backup (destination is SD), 
    // but passing journalSize is good practice.
    if (!fs::copyDirectoryWithProgress(savePath, backupPath, journalSize)) {
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

bool SaveManager::shouldSkipSafetyRollbackForRestorePath(const std::string& backupPath) {
    if (backupPath.empty()) {
        return false;
    }

    const size_t slash = backupPath.find_last_of('/');
    const std::string entryName = (slash == std::string::npos) ? backupPath : backupPath.substr(slash + 1);
    const std::string suffix = "_autosave";
    return entryName.size() > suffix.size() && entryName.compare(entryName.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool SaveManager::restoreSave(TitleInfo* title, const std::string& backupPath, bool createSafetyRollback) {
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
    std::string rollbackPath;
    const bool skipRollbackForSource = shouldSkipSafetyRollbackForRestorePath(backupPath);
    const bool shouldCreateRollback = createSafetyRollback && !skipRollbackForSource;
    if (shouldCreateRollback) {
        LOG_INFO("Restore: Creating safety rollback backup...");
        createVersionedBackup(title, DEFAULT_MAX_VERSIONS, true);

        auto versions = getBackupVersions(title);
        rollbackPath = versions.empty() ? "" : versions.front().path;
    }

    // Get journal size for proper commits
    int64_t journalSize = fs::getSaveJournalSize(title->titleId);    
    // Use scoped mount for RAII safety
    fs::ScopedSaveMount saveMount("save", title->titleId, m_selectedUser->uid, title->actualSaveType);
    if (!saveMount.isOpen()) {
        LOG_ERROR("Failed to mount save for restore");
        return false;
    }
    
    std::string savePath = saveMount.getMountPath();
    
    auto performRestore = [&](const std::string& srcPath) -> bool {
        if (!fs::clearDirectoryContents(savePath)) {
            LOG_ERROR("Failed to clear existing save data contents safely");
            return false;
        }
        if (!fs::copyDirectoryWithProgress(srcPath, savePath, journalSize, saveMount.getMountName())) {
            LOG_ERROR("Failed to copy save data from %s", srcPath.c_str());
            return false;
        }
        if (!saveMount.commit()) {
            LOG_ERROR("Failed to final commit save data");
            return false;
        }
        return true;
    };

    if (!performRestore(backupPath)) {
        LOG_ERROR("Restore failed! Attempting to rollback from %s", rollbackPath.c_str());
        if (!rollbackPath.empty() && rollbackPath != backupPath) {
            if (performRestore(rollbackPath)) {
                LOG_INFO("Rollback successful.");
            } else {
                LOG_ERROR("CRITICAL: Rollback failed! Save data may be corrupted.");
            }
        }
        return false;
    }
#else
#endif
    return true;
}

bool SaveManager::deleteBackup(const std::string& backupPath) {
    return moveToTrash(backupPath);
}

std::string SaveManager::getTrashPath() const {
    return utils::paths::TRASH;
}

std::string SaveManager::getTrashPath(TitleInfo* title) const {
    char path[256];
    snprintf(path, sizeof(path), "%s/%016lX", utils::paths::TRASH, title ? title->titleId : 0);
    return path;
}

bool SaveManager::moveToTrash(const std::string& backupPath) {
    LOG_INFO("moveToTrash: ENTER path='%s'", backupPath.c_str());

    struct stat st;
    if (stat(backupPath.c_str(), &st) != 0) {
        LOG_ERROR("moveToTrash: backup path does not exist or access denied (errno=%d: %s)", errno, strerror(errno));
        return false;
    }

    BackupMetadata meta;
    bool metaRead = readBackupMetadata(backupPath, meta);
    uint64_t titleId = meta.titleId;
    LOG_DEBUG("moveToTrash: metaRead=%d, titleId_from_meta=%016lX", metaRead, titleId);

    if (titleId == 0) {
        const size_t slash = backupPath.find_last_of('/');
        const size_t prevSlash = (slash != std::string::npos) ? backupPath.find_last_of('/', slash - 1) : std::string::npos;
        if (prevSlash != std::string::npos && slash != std::string::npos) {
            const std::string titleIdStr = backupPath.substr(prevSlash + 1, slash - prevSlash - 1);
            LOG_DEBUG("moveToTrash: attempting to parse titleId from string '%s'", titleIdStr.c_str());
            char* endptr = nullptr;
            titleId = std::strtoull(titleIdStr.c_str(), &endptr, 16);
            if (endptr == titleIdStr.c_str() || *endptr != '\0') {
                LOG_ERROR("moveToTrash: CRITICAL - failed to parse titleId from '%s'", titleIdStr.c_str());
                titleId = 0;
            }
        }
    }

    LOG_INFO("moveToTrash: titleId derived as %016lX", titleId);
    TitleInfo* title = getTitleById(titleId);
    if (!title) {
        LOG_WARNING("moveToTrash: title info not found for ID %016lX, using raw ID for trash folder", titleId);
    }

    const std::string trashBase = getTrashPath(title);
    LOG_DEBUG("moveToTrash: trashBase='%s'", trashBase.c_str());
    
    mkdir(utils::paths::TRASH, 0777);
    mkdir(trashBase.c_str(), 0777);

    const std::string timestamp = std::to_string(static_cast<long long>(time(nullptr)));
    const size_t nameStart = backupPath.find_last_of('/');
    const std::string backupName = (nameStart != std::string::npos) ? backupPath.substr(nameStart + 1) : backupPath;
    const std::string trashEntryName = backupName + "_" + timestamp;
    const std::string trashEntryPath = trashBase + "/" + trashEntryName;

    LOG_INFO("moveToTrash: RENAME '%s' -> '%s'", backupPath.c_str(), trashEntryPath.c_str());
    if (std::rename(backupPath.c_str(), trashEntryPath.c_str()) != 0) {
        LOG_ERROR("moveToTrash: rename FAILED (errno=%d: %s)", errno, strerror(errno));
        return false;
    }

    // Move associated metadata file if it exists
    const std::string metaPath = getBackupMetadataPath(backupPath);
    if (stat(metaPath.c_str(), &st) == 0) {
        const std::string trashMetaPath = getBackupMetadataPath(trashEntryPath);
        LOG_DEBUG("moveToTrash: moving meta file '%s' -> '%s'", metaPath.c_str(), trashMetaPath.c_str());
        if (std::rename(metaPath.c_str(), trashMetaPath.c_str()) != 0) {
            LOG_ERROR("moveToTrash: meta file move FAILED (errno=%d: %s)", errno, strerror(errno));
        }
    }

    // Move associated ZIP file if it exists
    const std::string zipPath = backupPath + ".zip";
    if (stat(zipPath.c_str(), &st) == 0) {
        const std::string trashZipPath = trashEntryPath + ".zip";
        LOG_DEBUG("moveToTrash: moving zip file '%s' -> '%s'", zipPath.c_str(), trashZipPath.c_str());
        if (std::rename(zipPath.c_str(), trashZipPath.c_str()) != 0) {
            LOG_ERROR("moveToTrash: zip file move FAILED (errno=%d: %s)", errno, strerror(errno));
        }
    }

    LOG_INFO("moveToTrash: SUCCESS");
    return true;
}

bool SaveManager::restoreFromTrash(const std::string& trashPath) {
    LOG_INFO("restoreFromTrash: ENTER path='%s'", trashPath.c_str());

    struct stat st;
    if (stat(trashPath.c_str(), &st) != 0) {
        LOG_ERROR("restoreFromTrash: trash entry does not exist (errno=%d: %s)", errno, strerror(errno));
        return false;
    }

    BackupMetadata meta;
    bool metaRead = readBackupMetadata(trashPath, meta);
    uint64_t titleId = meta.titleId;
    LOG_DEBUG("restoreFromTrash: metaRead=%d, titleId_from_meta=%016lX", metaRead, titleId);
    
    if (titleId == 0) {
        const size_t slash = trashPath.find_last_of('/');
        const size_t prevSlash = (slash != std::string::npos) ? trashPath.find_last_of('/', slash - 1) : std::string::npos;
        if (prevSlash != std::string::npos && slash != std::string::npos) {
            const std::string titleIdStr = trashPath.substr(prevSlash + 1, slash - prevSlash - 1);
            LOG_DEBUG("restoreFromTrash: attempting to parse titleId from string '%s'", titleIdStr.c_str());
            char* endptr = nullptr;
            titleId = std::strtoull(titleIdStr.c_str(), &endptr, 16);
            if (endptr == titleIdStr.c_str() || *endptr != '\0') {
                LOG_ERROR("restoreFromTrash: CRITICAL - failed to parse titleId from '%s'", titleIdStr.c_str());
                titleId = 0;
            }
        }
    }

    LOG_INFO("restoreFromTrash: titleId derived as %016lX", titleId);
    TitleInfo* title = getTitleById(titleId);
    if (!title) {
        LOG_ERROR("restoreFromTrash: title not found for ID %016lX. Cannot restore without title metadata.", titleId);
        return false;
    }

    const std::string backupBase = getBackupPath(title);
    LOG_DEBUG("restoreFromTrash: backupBase='%s'", backupBase.c_str());
    mkdir(backupBase.c_str(), 0777);

    const size_t nameStart = trashPath.find_last_of('/');
    const std::string entryName = (nameStart != std::string::npos) ? trashPath.substr(nameStart + 1) : trashPath;
    const size_t underscorePos = entryName.find_last_of('_');
    const std::string originalName = (underscorePos != std::string::npos) ? entryName.substr(0, underscorePos) : entryName;
    const std::string restorePath = backupBase + "/" + originalName + "_restored";

    LOG_INFO("restoreFromTrash: COPYING '%s' -> '%s'", trashPath.c_str(), restorePath.c_str());
    if (!fs::copyDirectoryWithProgress(trashPath, restorePath)) {
        LOG_ERROR("restoreFromTrash: copy FAILED");
        return false;
    }

    const std::string trashMetaPath = getBackupMetadataPath(trashPath);
    if (stat(trashMetaPath.c_str(), &st) == 0) {
        LOG_DEBUG("restoreFromTrash: writing updated metadata to '%s'", restorePath.c_str());
        BackupMetadata restoredMeta = meta;
        restoredMeta.backupName = originalName + "_restored";
        restoredMeta.source = "restored_from_trash";
        writeBackupMetadataFile(getBackupMetadataPath(restorePath), restoredMeta);
    }

    LOG_INFO("restoreFromTrash: DELETING original trash entry '%s'", trashPath.c_str());
    fs::deleteDirectory(trashPath);
    const std::string trashMetaDel = getBackupMetadataPath(trashPath);
    std::remove(trashMetaDel.c_str());
    const std::string trashZipDel = trashPath + ".zip";
    std::remove(trashZipDel.c_str());

    LOG_INFO("restoreFromTrash: SUCCESS");
    return true;
}

bool SaveManager::cleanupExpiredTrashEntries(int retentionDays) {
    const std::string trashPath = getTrashPath();
    DIR* rootDir = opendir(trashPath.c_str());
    if (!rootDir) {
        return true;
    }

    const std::time_t now = std::time(nullptr);
    const int safeDays = retentionDays > 0 ? retentionDays : TRASH_RETENTION_DAYS_DEFAULT;
    const std::time_t retentionSeconds = static_cast<std::time_t>(safeDays) * 24 * 60 * 60;

    int deletedCount = 0;
    struct dirent* titleEntry;
    while ((titleEntry = readdir(rootDir)) != nullptr) {
        if (titleEntry->d_name[0] == '.') {
            continue;
        }

        const std::string titlePath = trashPath + "/" + titleEntry->d_name;
        struct stat titleStat;
        if (stat(titlePath.c_str(), &titleStat) != 0 || !S_ISDIR(titleStat.st_mode)) {
            continue;
        }

        DIR* titleDir = opendir(titlePath.c_str());
        if (!titleDir) {
            continue;
        }

        struct dirent* itemEntry;
        while ((itemEntry = readdir(titleDir)) != nullptr) {
            if (itemEntry->d_name[0] == '.') {
                continue;
            }

            const std::string itemPath = titlePath + "/" + itemEntry->d_name;
            struct stat itemStat;
            if (stat(itemPath.c_str(), &itemStat) != 0) {
                continue;
            }

            if (now - itemStat.st_mtime < retentionSeconds) {
                continue;
            }

            if (S_ISDIR(itemStat.st_mode)) {
                fs::deleteDirectory(itemPath);
                std::remove((itemPath + ".zip").c_str());
                ++deletedCount;
            } else {
                if (std::remove(itemPath.c_str()) == 0) {
                    ++deletedCount;
                }
            }
        }
        closedir(titleDir);
    }

    closedir(rootDir);
    if (deletedCount > 0) {
        LOG_INFO("cleanupExpiredTrashEntries: deleted %d expired trash item(s) older than %d days", deletedCount, safeDays);
    }
    return true;
}

bool SaveManager::hasExpiredTrashEntries(int retentionDays) {
    const std::string trashPath = getTrashPath();
    DIR* rootDir = opendir(trashPath.c_str());
    if (!rootDir) {
        return false;
    }

    const std::time_t now = std::time(nullptr);
    const int safeDays = retentionDays > 0 ? retentionDays : TRASH_RETENTION_DAYS_DEFAULT;
    const std::time_t retentionSeconds = static_cast<std::time_t>(safeDays) * 24 * 60 * 60;

    struct dirent* titleEntry;
    while ((titleEntry = readdir(rootDir)) != nullptr) {
        if (titleEntry->d_name[0] == '.') {
            continue;
        }

        const std::string titlePath = trashPath + "/" + titleEntry->d_name;
        struct stat titleStat;
        if (stat(titlePath.c_str(), &titleStat) != 0 || !S_ISDIR(titleStat.st_mode)) {
            continue;
        }

        DIR* titleDir = opendir(titlePath.c_str());
        if (!titleDir) {
            continue;
        }

        struct dirent* itemEntry;
        while ((itemEntry = readdir(titleDir)) != nullptr) {
            if (itemEntry->d_name[0] == '.') {
                continue;
            }

            const std::string itemPath = titlePath + "/" + itemEntry->d_name;
            struct stat itemStat;
            if (stat(itemPath.c_str(), &itemStat) != 0) {
                continue;
            }

            if (now - itemStat.st_mtime >= retentionSeconds) {
                closedir(titleDir);
                closedir(rootDir);
                return true;
            }
        }
        closedir(titleDir);
    }

    closedir(rootDir);
    return false;
}

bool SaveManager::emptyTrash() {
    LOG_INFO("emptyTrash: clearing all trash contents");
    
    const std::string trashPath = getTrashPath();
    DIR* dir = opendir(trashPath.c_str());
    if (!dir) {
        LOG_INFO("emptyTrash: trash directory empty or does not exist");
        return true;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        
        char entryPath[512];
        snprintf(entryPath, sizeof(entryPath), "%s/%s", trashPath.c_str(), entry->d_name);
        
        struct stat st;
        if (stat(entryPath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                fs::deleteDirectory(entryPath);
            } else {
                std::remove(entryPath);
            }
        }
    }
    closedir(dir);

    LOG_INFO("emptyTrash: completed");
    return true;
}

std::vector<BackupVersion> SaveManager::listTrash(TitleInfo* title) {
    std::vector<BackupVersion> versions;
    
    const std::string trashPath = title ? getTrashPath(title) : getTrashPath();
    DIR* dir = opendir(trashPath.c_str());
    if (!dir) return versions;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
            BackupVersion ver;
            ver.name = entry->d_name;
            ver.path = trashPath + "/" + entry->d_name;
            ver.isTrashed = true;
            ver.isCloudSynced = false;
            ver.size = 0;
            
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
                ver.isAutoBackup = meta.isAutoBackup;
                if (meta.createdAt != 0) {
                    ver.timestamp = meta.createdAt;
                }
            }
            
            versions.push_back(ver);
        }
    }
    
    closedir(dir);
    
    std::sort(versions.begin(), versions.end(), 
        [](const BackupVersion& a, const BackupVersion& b) {
            return a.timestamp > b.timestamp;
        });
    
    return versions;
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
    return "users/" + userId;  // No "oc-save-keeper/" prefix - app folder is already oc-save-keeper
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
    return std::string("titles/") + titleComponent;
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
    return std::string("titles/") + titleComponent + "/revisions";
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
    
    std::sort(versions.begin(), versions.end(),
        [](const BackupVersion& a, const BackupVersion& b) {
            if (a.timestamp != b.timestamp) {
                return a.timestamp > b.timestamp;
            }
            return a.name < b.name;
        });
    
    return versions;
}

bool SaveManager::createVersionedBackup(TitleInfo* title, int maxVersions, bool isAutoBackup) {
    // Create timestamped backup name
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char name[64];
    strftime(name, sizeof(name), "%Y%m%d_%H%M%S", t);
    
    // Create backup
    if (!backupSave(title, name)) {
        return false;
    }

    if (isAutoBackup) {
        const std::string backupPath = getBackupPath(title) + "/" + std::string(name);
        BackupMetadata meta;
        if (readBackupMetadata(backupPath, meta)) {
            meta.isAutoBackup = true;
            writeBackupMetadataFile(getBackupMetadataPath(backupPath), meta);
        }
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
        return {false, utils::Language::instance().get("error.no_title_selected")};
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

    // Validate ZIP file before extraction
    struct stat st;
    if (stat(archivePath.c_str(), &st) != 0) {
        LOG_ERROR("ZIP file not found: %s", archivePath.c_str());
        if (outReason) *outReason = "Archive file not found";
        return false;
    }
    if (st.st_size == 0) {
        LOG_ERROR("ZIP file is empty: %s", archivePath.c_str());
        if (outReason) *outReason = "Archive file is empty";
        return false;
    }
    LOG_DEBUG("ZIP file validated: %s (%ld bytes)", archivePath.c_str(), st.st_size);

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
        ? (skipConflictCheck ? SyncDecision{true, "Restore success"}
                             : evaluateIncomingMetadata(title, incomingMeta))
        : SyncDecision{true, "Incoming backup has no metadata; applying as recovery import"};

    if (!decision.useIncoming) {
        fs::deleteDirectory(tempDir);
        if (outReason) *outReason = decision.reason;
        return true;
    }

    // Keep one rollback point before any incoming cloud restore touches the live save.
    createVersionedBackup(title, DEFAULT_MAX_VERSIONS, true);

    const std::string baseName = hasIncomingMeta && !incomingMeta.backupName.empty()
        ? incomingMeta.backupName
        : ("cloud_" + std::to_string(static_cast<long long>(time(nullptr))));
    std::string importName = sanitizePathComponent(baseName);
    if (importName.empty()) {
        importName = "cloud_" + std::to_string(static_cast<long long>(time(nullptr)));
    }
    std::string importPath = getBackupPath(title) + "/" + importName;
    struct stat importStat;
    if (stat(importPath.c_str(), &importStat) == 0) {
        importName += "_" + std::to_string(static_cast<long long>(time(nullptr)));
        importPath = getBackupPath(title) + "/" + importName;
    }

    if (!fs::copyDirectoryWithProgress(tempDir, importPath, fs::DEFAULT_JOURNAL_SIZE)) {
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
    finalMeta.isAutoBackup = false;

    writeBackupMetadataFile(getBackupMetadataPath(importPath), finalMeta);

    const bool restored = restoreSave(title, importPath, false);
    fs::deleteDirectory(tempDir);

    if (outReason) {
        *outReason = restored ? decision.reason : "Restore failed after import";
    }
    return restored;
}

} // namespace core
