#include "ui/saves/SaveBackendAdapter.hpp"

#include "core/SaveManager.hpp"
#include "network/Dropbox.hpp"
#include "ui/saves/Runtime.hpp"
#include "utils/Language.hpp"
#include "utils/Logger.hpp"
#include "utils/Paths.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>
#include <utility>
#include <set>

namespace ui::saves {

namespace {

// Cache for remote title IDs to avoid repeated network calls
static std::set<uint64_t> g_remoteTitleCache;
static bool g_remoteCacheValid = false;

// Maximum cache size to prevent unbounded growth (1000 titles ~= 8KB)
constexpr size_t MAX_REMOTE_CACHE_SIZE = 1000;

static void trimRemoteCache() {
    if (g_remoteTitleCache.size() > MAX_REMOTE_CACHE_SIZE) {
        auto it = g_remoteTitleCache.begin();
        std::advance(it, g_remoteTitleCache.size() - MAX_REMOTE_CACHE_SIZE);
        g_remoteTitleCache.erase(g_remoteTitleCache.begin(), it);
    }
}

const char* saveTypeLabel(core::SaveType type) {
    switch (type) {
        case core::SaveType::System:
            return "System";
        case core::SaveType::Device:
            return "Device";
        case core::SaveType::BCAT:
            return "BCAT";
        case core::SaveType::Cache:
            return "Cache";
        case core::SaveType::Temporary:
            return "Temp";
        case core::SaveType::Account:
        default:
            return "User";
    }
}

std::string shortSizeLabel(int64_t size) {
    if (size <= 0) {
        return "0 B";
    }

    char buffer[32];
    if (size < 1024) {
        std::snprintf(buffer, sizeof(buffer), "%lld B", static_cast<long long>(size));
        return buffer;
    }

    const double kb = static_cast<double>(size) / 1024.0;
    if (kb < 1024.0) {
        std::snprintf(buffer, sizeof(buffer), kb < 10.0 ? "%.1f KB" : "%.0f KB", kb);
        return buffer;
    }

    const double mb = kb / 1024.0;
    std::snprintf(buffer, sizeof(buffer), mb < 10.0 ? "%.1f MB" : "%.0f MB", mb);
    return buffer;
}

std::string fileNameFromPath(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string makeTempArchivePath(uint64_t titleId) {
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s/frontend_%016llX_%lld.zip", utils::paths::TEMP,
                  static_cast<unsigned long long>(titleId),
                  static_cast<long long>(time(nullptr)));
    return buffer;
}

std::string makeTempMetadataPath(uint64_t titleId, const std::string& suffix) {
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s/frontend_%016llX_%s.meta", utils::paths::TEMP,
                  static_cast<unsigned long long>(titleId),
                  suffix.c_str());
    return buffer;
}

std::vector<network::DropboxFile> listRemoteRevisionMetadata(network::Dropbox& dropbox,
                                                             core::SaveManager& saveManager,
                                                             core::TitleInfo* title) {
    std::vector<network::DropboxFile> files;
    if (!title || !dropbox.isAuthenticated()) {
        return files;
    }

    // Use title-centric revision path: titles/{titleId}/revisions
    const std::string revisionDir = "/" + saveManager.getCloudRevisionDirectory(title, "");
    auto revisionFiles = dropbox.listFolder(revisionDir, false);
    revisionFiles.erase(std::remove_if(revisionFiles.begin(), revisionFiles.end(), [](const network::DropboxFile& file) {
        return file.isFolder || !endsWith(file.name, ".meta");
    }), revisionFiles.end());
    files.insert(files.end(), revisionFiles.begin(), revisionFiles.end());

    std::sort(files.begin(), files.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.modifiedTime > rhs.modifiedTime;
    });
    return files;
}

} // namespace

SaveBackendAdapter::SaveBackendAdapter(core::SaveManager& saveManager, network::Dropbox& dropbox)
    : m_saveManager(saveManager)
    , m_dropbox(dropbox) {}

std::vector<SaveTitleEntry> SaveBackendAdapter::listTitles() {
    std::vector<SaveTitleEntry> out;

    if (m_dropbox.isAuthenticated() && !g_remoteCacheValid) {
        const std::string remotePath = "/titles";
        auto remoteFiles = m_dropbox.listFolder(remotePath);

        g_remoteTitleCache.clear();
        for (const auto& file : remoteFiles) {
            if (file.isFolder) {
                char* endptr = nullptr;
                uint64_t tid = std::strtoull(file.name.c_str(), &endptr, 16);
                if (endptr && *endptr == '\0') {
                    g_remoteTitleCache.insert(tid);
                }
            }
        }
        trimRemoteCache();
        g_remoteCacheValid = true;
    }

    for (auto* title : m_saveManager.getTitlesWithSaves()) {
        const auto versions = m_saveManager.getBackupVersions(title);
        std::string vCount = versions.empty() ? "" : (" (" + std::to_string(versions.size()) + ")");

        // 1. Account Save (if exists)
        if (title->hasAccountSave) {
            SaveTitleEntry entry;
            entry.titleId = title->titleId;
            entry.name = title->name;
            entry.author = title->publisher;
            entry.iconPath = title->iconPath;
            entry.subtitle = std::string(saveTypeLabel(core::SaveType::Account)) + "  " + shortSizeLabel(title->accountSize) + vCount;
            entry.hasLocalBackup = !versions.empty();
            entry.hasCloudBackup = m_dropbox.isAuthenticated() && (g_remoteTitleCache.count(title->titleId) > 0);
            entry.isDevice = false;
            entry.isSystem = false;
            out.push_back(std::move(entry));
        }

        // 2. Device/System Save (if exists)
        if (title->hasDeviceSave) {
            SaveTitleEntry entry;
            entry.titleId = title->titleId;
            entry.name = title->name + " (디바이스)";
            entry.author = title->publisher;
            entry.iconPath = title->iconPath;
            entry.subtitle = "Device  " + shortSizeLabel(title->deviceSize);
            entry.hasLocalBackup = false; // Simplified: versions for device saves usually separate
            entry.hasCloudBackup = false;
            entry.isDevice = true;
            entry.isSystem = (title->actualSaveType == core::SaveType::System);
            out.push_back(std::move(entry));
        }
    }
    return out;
}

std::vector<SaveRevisionEntry> SaveBackendAdapter::listRevisions(uint64_t titleId, SaveSource source) {
    std::vector<SaveRevisionEntry> out;

    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return out;
    }

    const auto& lang = utils::Language::instance();
    const std::string currentDeviceId = m_saveManager.getDeviceId();

    if (source == SaveSource::Local) {
        for (const auto& version : m_saveManager.getBackupVersions(title)) {
            SaveRevisionEntry entry;
            entry.id = version.path;
            entry.label = version.name;
            entry.path = version.path;
            
            // If the local backup originated from this device, show '현재 기기'
            if (version.deviceId == currentDeviceId || version.source == "local") {
                entry.deviceLabel = lang.get("history.source_local");
            } else {
                entry.deviceLabel = version.deviceLabel.empty() ? lang.get("history.unknown_device") : version.deviceLabel;
            }

            entry.userLabel = version.userName;
            entry.sourceLabel = (version.source == "local") ? lang.get("history.source_local") : lang.get("history.source_dropbox");
            entry.timestamp = version.timestamp;
            entry.size = version.size;
            entry.source = SaveSource::Local;
            out.push_back(std::move(entry));
        }
    } else {
        const auto files = listRemoteRevisionMetadata(m_dropbox, m_saveManager, title);
        for (const auto& file : files) {
            const std::string tempMeta = makeTempMetadataPath(title->titleId, fileNameFromPath(file.path));
            if (!m_dropbox.downloadFile(file.path, tempMeta)) {
                continue;
            }

            core::BackupMetadata incomingMeta;
            const bool validMeta = m_saveManager.readMetadataFile(tempMeta, incomingMeta);
            std::remove(tempMeta.c_str());
            if (!validMeta) {
                continue;
            }

            SaveRevisionEntry entry;
            entry.id = file.path;
            entry.label = incomingMeta.backupName.empty() ? file.name : incomingMeta.backupName;
            entry.path = file.path.substr(0, file.path.size() - 5) + ".zip";
            
            // For Cloud revisions: if the meta device ID matches current, show '현재 기기'
            if (incomingMeta.deviceId == currentDeviceId) {
                entry.deviceLabel = lang.get("history.source_local");
            } else {
                entry.deviceLabel = incomingMeta.deviceLabel.empty() ? lang.get("history.unknown_device") : incomingMeta.deviceLabel;
            }

            entry.userLabel = incomingMeta.userName;
            entry.sourceLabel = lang.get("history.source_dropbox");
            
            entry.timestamp = incomingMeta.createdAt != 0 ? incomingMeta.createdAt : file.modifiedTime;
            entry.size = incomingMeta.size > 0 ? incomingMeta.size : static_cast<int64_t>(file.size);
            entry.source = SaveSource::Cloud;
            out.push_back(std::move(entry));
        }
    }

    return out;
}

SaveActionResult SaveBackendAdapter::backup(uint64_t titleId) {
    auto& lang = utils::Language::instance();
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return {false, lang.get("error.unknown_title")};
    }

    const bool ok = m_saveManager.createVersionedBackup(title);
    if (ok) g_remoteCacheValid = false;
    return {
        ok,
        ok ? lang.get("sync.success") : lang.get("sync.restore_failed")
    };
}

SaveActionResult SaveBackendAdapter::restore(uint64_t titleId, const std::string& revisionId, SaveSource source) {
    auto& lang = utils::Language::instance();
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return {false, lang.get("error.unknown_title")};
    }

    if (source != SaveSource::Local) {
        return {false, lang.get("error.cloud_restore_download_first")};
    }

    const bool ok = m_saveManager.restoreSave(title, revisionId);
    if (ok) g_remoteCacheValid = false;
    return {
        ok,
        ok ? lang.get("sync.restore_success") : lang.get("sync.restore_failed")
    };
}

SaveActionResult SaveBackendAdapter::upload(uint64_t titleId) {
    auto& lang = utils::Language::instance();
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return {false, lang.get("error.unknown_title")};
    }
    if (!m_dropbox.isAuthenticated()) {
        return {false, lang.get("error.not_authenticated")};
    }

    Runtime::instance().notify(lang.get("sync.creating_local"));

    if (!m_saveManager.createVersionedBackup(title)) {
        return {false, lang.get("error.local_backup_failed")};
    }

    const auto versions = m_saveManager.getBackupVersions(title);
    if (versions.empty()) {
        return {false, lang.get("error.no_backup_version")};
    }

    const std::string archivePath = m_saveManager.exportBackupArchive(title, versions.front().path);
    if (archivePath.empty()) {
        return {false, lang.get("error.archive_export_failed")};
    }

    Runtime::instance().notify(lang.get("sync.uploading_dropbox"));

    const std::string localMetaPath = versions.front().path + ".meta";
    const std::string latestArchivePath = "/" + m_saveManager.getCloudPath(title);
    const std::string latestMetaPath = "/" + m_saveManager.getCloudMetadataPath(title);
    const std::string revisionDir = "/" + m_saveManager.getCloudRevisionDirectory(title);
    const std::string archiveName = fileNameFromPath(archivePath);
    const std::string metaName = fileNameFromPath(localMetaPath);
    const std::string revisionArchivePath = revisionDir + "/" + archiveName;
    const std::string revisionMetaPath = revisionDir + "/" + metaName;

    const bool ok = m_dropbox.uploadFile(localMetaPath, revisionMetaPath) &&
                    m_dropbox.uploadFile(archivePath, revisionArchivePath) &&
                    m_dropbox.uploadFile(localMetaPath, latestMetaPath) &&
                    m_dropbox.uploadFile(archivePath, latestArchivePath);

    if (ok) g_remoteCacheValid = false;
    return {
        ok,
        ok ? lang.get("sync.upload_completed") : lang.get("sync.upload_failed")
    };
}

SaveActionResult SaveBackendAdapter::download(uint64_t titleId, const std::string& revisionId) {
    auto& lang = utils::Language::instance();
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return {false, lang.get("error.unknown_title")};
    }
    if (!m_dropbox.isAuthenticated()) {
        return {false, lang.get("error.not_authenticated")};
    }

    utils::paths::ensureBaseDirectories();
    const std::string tempArchive = makeTempArchivePath(titleId);
    if (!m_dropbox.downloadFile(revisionId, tempArchive)) {
        return {false, lang.get("error.download_failed")};
    }

    std::string reason;
    const bool ok = m_saveManager.importBackupArchive(title, tempArchive, &reason, true);
    std::remove(tempArchive.c_str());

    if (ok) g_remoteCacheValid = false;
    if (!ok) {
        return {false, reason.empty() ? lang.get("error.download_failed") : reason};
    }

    return {
        true,
        reason.empty() ? lang.get("sync.upload_completed") : reason
    };
}

SaveActionResult SaveBackendAdapter::refresh(uint64_t titleId) {
    auto& lang = utils::Language::instance();
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return {false, lang.get("error.unknown_title")};
    }

    g_remoteCacheValid = false;
    const auto versions = m_saveManager.getBackupVersions(title);
    return {
        true,
        versions.empty() ? lang.get("history.no_backup") : lang.get("ui.refresh_completed")
    };
}

SaveActionResult SaveBackendAdapter::deleteRevision(uint64_t titleId, const std::string& revisionId, SaveSource source) {
    auto& lang = utils::Language::instance();
    LOG_INFO("deleteRevision: titleId=%016lX revisionId=%s source=%d", titleId, revisionId.c_str(), static_cast<int>(source));
    
    if (revisionId.empty()) {
        LOG_ERROR("deleteRevision: empty revisionId");
        return {false, lang.get("error.invalid_backup_selection")};
    }
    
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        LOG_ERROR("deleteRevision: unknown title");
        return {false, lang.get("error.unknown_title")};
    }

    if (title->actualSaveType == core::SaveType::System) {
        LOG_ERROR("deleteRevision: system saves cannot be deleted");
        return {false, lang.get("error.system_saves_undeletable")};
    }

    if (source == SaveSource::Local) {
        const std::string backupBase = "/switch/oc-save-keeper/backups/";
        if (revisionId.find(backupBase) != 0) {
            LOG_ERROR("deleteRevision: invalid local path (outside backup directory): %s", revisionId.c_str());
            return {false, lang.get("error.invalid_backup_path")};
        }
        
        if (revisionId == backupBase || revisionId.back() == '/') {
            LOG_ERROR("deleteRevision: cannot delete backup root");
            return {false, lang.get("error.cannot_delete_root")};
        }
        
        LOG_INFO("deleteRevision: deleting local backup");
        const bool ok = m_saveManager.deleteBackup(revisionId);
        if (ok) g_remoteCacheValid = false;
        LOG_INFO("deleteRevision: local delete result=%d", ok);
        return { ok, ok ? lang.get("sync.delete_success") : lang.get("sync.delete_failed") };
    } else {
        LOG_INFO("deleteRevision: deleting cloud backup");
        if (!m_dropbox.isAuthenticated()) {
            LOG_ERROR("deleteRevision: Dropbox not authenticated");
            return {false, lang.get("error.not_authenticated")};
        }

        char titleIdStr[20];
        std::snprintf(titleIdStr, sizeof(titleIdStr), "%016lX", titleId);
        const std::string expectedPrefix = std::string("/titles/") + titleIdStr + "/revisions/";
        
        if (revisionId.find(expectedPrefix) != 0) {
            LOG_ERROR("deleteRevision: invalid cloud path (outside title revisions): %s", revisionId.c_str());
            return {false, lang.get("error.invalid_cloud_path")};
        }

        std::string metaPath, zipPath;
        if (revisionId.size() > 5 && revisionId.substr(revisionId.size() - 5) == ".meta") {
            metaPath = revisionId;
            zipPath = revisionId.substr(0, revisionId.size() - 5) + ".zip";
        } else if (revisionId.size() > 4 && revisionId.substr(revisionId.size() - 4) == ".zip") {
            zipPath = revisionId;
            metaPath = revisionId.substr(0, revisionId.size() - 4) + ".meta";
        } else {
            LOG_ERROR("deleteRevision: unexpected revisionId format: %s", revisionId.c_str());
            return {false, lang.get("error.invalid_backup_format")};
        }

        LOG_INFO("deleteRevision: deleting zip=%s meta=%s", zipPath.c_str(), metaPath.c_str());
        
        bool zipOk = m_dropbox.deleteFile(zipPath);
        if (!zipOk) {
            LOG_ERROR("deleteRevision: failed to delete zip file");
        }
        
        bool metaOk = m_dropbox.deleteFile(metaPath);
        if (!metaOk) {
            LOG_WARNING("deleteRevision: failed to delete meta file");
        }
        
        const bool ok = zipOk || metaOk;
        
        if (ok) g_remoteCacheValid = false;
        LOG_INFO("deleteRevision: cloud delete result=%d", ok);
        return { ok, ok ? lang.get("sync.delete_success") : lang.get("sync.delete_failed") };
    }
}

void SaveBackendAdapter::setTargetType(uint64_t titleId, bool isDevice, bool isSystem) {
    auto* title = m_saveManager.getTitleById(titleId);
    if (title) {
        if (isSystem) title->actualSaveType = core::SaveType::System;
        else if (isDevice) title->actualSaveType = core::SaveType::Device;
        else title->actualSaveType = core::SaveType::Account;
    }
}

bool SaveBackendAdapter::isCloudAuthenticated() const {
    return m_dropbox.isAuthenticated();
}

void SaveBackendAdapter::invalidateCache() {
    g_remoteCacheValid = false;
}

void SaveBackendAdapter::invalidateAllCaches() {
    g_remoteTitleCache.clear();
    g_remoteCacheValid = false;
}

} // namespace ui::saves
