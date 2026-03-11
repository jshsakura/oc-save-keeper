#include "ui/saves/SaveBackendAdapter.hpp"

#include "core/SaveManager.hpp"
#include "network/Dropbox.hpp"

#include <algorithm>
#include <cstdio>
#include <sys/stat.h>
#include <utility>

namespace ui::saves {

namespace {

SaveActionResult notImplemented(const char* action) {
    return {
        false,
        std::string(action) + " is not wired yet"
    };
}

std::string directoryFromPath(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string{} : path.substr(0, slash);
}

std::string fileNameFromPath(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string makeTempArchivePath(uint64_t titleId) {
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "/switch/oc-save-keeper/temp/frontend_%016llX_%lld.zip",
                  static_cast<unsigned long long>(titleId),
                  static_cast<long long>(time(nullptr)));
    return buffer;
}

} // namespace

SaveBackendAdapter::SaveBackendAdapter(core::SaveManager& saveManager, network::Dropbox& dropbox)
    : m_saveManager(saveManager)
    , m_dropbox(dropbox) {}

std::vector<SaveTitleEntry> SaveBackendAdapter::listTitles() {
    std::vector<SaveTitleEntry> out;

    for (auto* title : m_saveManager.getTitlesWithSaves()) {
        SaveTitleEntry entry;
        entry.titleId = title->titleId;
        entry.name = title->name;
        entry.author = title->publisher;
        entry.iconPath = title->iconPath;
        entry.subtitle = title->savePath;

        const auto versions = m_saveManager.getBackupVersions(title);
        entry.hasLocalBackup = !versions.empty();
        entry.hasCloudBackup = m_dropbox.isAuthenticated();
        out.push_back(std::move(entry));
    }

    return out;
}

std::vector<SaveRevisionEntry> SaveBackendAdapter::listRevisions(uint64_t titleId, SaveSource source) {
    std::vector<SaveRevisionEntry> out;

    auto* title = m_saveManager.getTitleById(titleId);
    if (!title) {
        return out;
    }

    if (source == SaveSource::Local) {
        for (const auto& version : m_saveManager.getBackupVersions(title)) {
            SaveRevisionEntry entry;
            entry.id = version.path;
            entry.label = version.name;
            entry.path = version.path;
            entry.deviceLabel = version.deviceLabel;
            entry.userLabel = version.userName;
            entry.sourceLabel = version.source;
            entry.timestamp = version.timestamp;
            entry.size = version.size;
            entry.source = SaveSource::Local;
            out.push_back(std::move(entry));
        }
    } else {
        const std::string latestPath = "/" + m_saveManager.getCloudPath(title);
        const std::string cloudDir = directoryFromPath(latestPath);
        auto files = m_dropbox.listFolder(cloudDir);
        std::sort(files.begin(), files.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.modifiedTime > rhs.modifiedTime;
        });

        for (const auto& file : files) {
            if (file.isFolder) {
                continue;
            }
            if (file.name.size() < 4 || file.name.substr(file.name.size() - 4) != ".zip") {
                continue;
            }

            SaveRevisionEntry entry;
            entry.id = file.path;
            entry.label = file.name;
            entry.path = file.path;
            entry.sourceLabel = "Dropbox";
            entry.timestamp = file.modifiedTime;
            entry.size = static_cast<int64_t>(file.size);
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

    const std::string localMetaPath = versions.front().path + ".meta";
    const std::string latestArchivePath = "/" + m_saveManager.getCloudPath(title);
    const std::string latestMetaPath = "/" + m_saveManager.getCloudMetadataPath(title);
    const std::string cloudDir = directoryFromPath(latestArchivePath);
    const std::string archiveName = fileNameFromPath(archivePath);
    const std::string metaName = fileNameFromPath(localMetaPath);
    const std::string revisionArchivePath = cloudDir.empty() ? ("/" + archiveName) : (cloudDir + "/" + archiveName);
    const std::string revisionMetaPath = cloudDir.empty() ? ("/" + metaName) : (cloudDir + "/" + metaName);

    const bool ok = m_dropbox.uploadFile(localMetaPath, revisionMetaPath) &&
                    m_dropbox.uploadFile(archivePath, revisionArchivePath) &&
                    m_dropbox.uploadFile(localMetaPath, latestMetaPath) &&
                    m_dropbox.uploadFile(archivePath, latestArchivePath);

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

    mkdir("/switch/oc-save-keeper/temp", 0777);
    const std::string tempArchive = makeTempArchivePath(titleId);
    if (!m_dropbox.downloadFile(revisionId, tempArchive)) {
        return {false, "Download failed"};
    }

    std::string reason;
    const bool ok = m_saveManager.importBackupArchive(title, tempArchive, &reason);
    std::remove(tempArchive.c_str());

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

    const auto versions = m_saveManager.getBackupVersions(title);
    return {
        true,
        versions.empty() ? "No local backups found" : "Refresh completed"
    };
}

} // namespace ui::saves
