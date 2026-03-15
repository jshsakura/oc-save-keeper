#include "ui/saves/SaveBackendAdapter.hpp"

#include "core/SaveManager.hpp"
#include "network/Dropbox.hpp"
#include "ui/saves/Runtime.hpp"
#include "utils/Language.hpp"
#include "utils/Paths.hpp"

#include <algorithm>
#include <cstdio>
#include <sys/stat.h>
#include <utility>
#include <set>

namespace ui::saves {

namespace {

// Cache for remote title IDs to avoid repeated network calls
static std::set<uint64_t> g_remoteTitleCache;
static bool g_remoteCacheValid = false;

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

    // Efficiently fetch remote titles if not cached
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
    if (source == SaveSource::Local) {
        for (const auto& version : m_saveManager.getBackupVersions(title)) {
            SaveRevisionEntry entry;
            entry.id = version.path;
            entry.label = version.name;
            entry.path = version.path;
            entry.deviceLabel = version.deviceLabel;
            entry.userLabel = version.userName;
            entry.sourceLabel = (version.source == "local") ? lang.get("history.source_local") : version.source;
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
            entry.deviceLabel = incomingMeta.deviceLabel;
            entry.userLabel = incomingMeta.userName;
            
            std::string src = incomingMeta.source.empty() ? "Dropbox" : incomingMeta.source;
            entry.sourceLabel = (src == "Dropbox") ? lang.get("history.source_dropbox") : src;
            
            entry.timestamp = incomingMeta.createdAt != 0 ? incomingMeta.createdAt : file.modifiedTime;
            entry.size = incomingMeta.size > 0 ? incomingMeta.size : static_cast<int64_t>(file.size);
            entry.source = SaveSource::Cloud;
            out.push_back(std::move(entry));
        }
    }

    return out;
}

SaveActionResult SaveBackendAdapter::backup(uint64_t titleId) {
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return {false, "Unknown title"};
    }

    const bool ok = m_saveManager.createVersionedBackup(title);
    if (ok) g_remoteCacheValid = false;
    return {
        ok,
        ok ? "Backup created" : "Backup failed"
    };
}

SaveActionResult SaveBackendAdapter::restore(uint64_t titleId, const std::string& revisionId, SaveSource source) {
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return {false, "Unknown title"};
    }

    if (source != SaveSource::Local) {
        return {false, "Cloud restore must be downloaded first"};
    }

    const bool ok = m_saveManager.restoreSave(title, revisionId);
    if (ok) g_remoteCacheValid = false;
    return {
        ok,
        ok ? "Restore completed" : "Restore failed"
    };
}

SaveActionResult SaveBackendAdapter::upload(uint64_t titleId) {
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return {false, "Unknown title"};
    }
    if (!m_dropbox.isAuthenticated()) {
        return {false, "Dropbox is not connected"};
    }

    Runtime::instance().notify("Creating local backup...");

    if (!m_saveManager.createVersionedBackup(title)) {
        return {false, "Local backup failed"};
    }

    const auto versions = m_saveManager.getBackupVersions(title);
    if (versions.empty()) {
        return {false, "No backup version created"};
    }

    const std::string archivePath = m_saveManager.exportBackupArchive(title, versions.front().path);
    if (archivePath.empty()) {
        return {false, "Archive export failed"};
    }

    Runtime::instance().notify("Uploading to cloud...");

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
        ok ? "Upload completed" : "Upload failed"
    };
}

SaveActionResult SaveBackendAdapter::download(uint64_t titleId, const std::string& revisionId) {
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return {false, "Unknown title"};
    }
    if (!m_dropbox.isAuthenticated()) {
        return {false, "Dropbox is not connected"};
    }

    utils::paths::ensureBaseDirectories();
    const std::string tempArchive = makeTempArchivePath(titleId);
    if (!m_dropbox.downloadFile(revisionId, tempArchive)) {
        return {false, "Download failed"};
    }

    std::string reason;
    const bool ok = m_saveManager.importBackupArchive(title, tempArchive, &reason);
    std::remove(tempArchive.c_str());

    if (ok) g_remoteCacheValid = false;
    if (!ok) {
        return {false, reason.empty() ? "Import failed" : reason};
    }

    return {
        true,
        reason.empty() ? "Download completed" : reason
    };
}

SaveActionResult SaveBackendAdapter::refresh(uint64_t titleId) {
    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return {false, "Unknown title"};
    }

    g_remoteCacheValid = false;
    const auto versions = m_saveManager.getBackupVersions(title);
    return {
        true,
        versions.empty() ? "No local backups found" : "Refresh completed"
    };
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

} // namespace ui::saves
