#pragma once

#include "core/SaveManager.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace core {

inline std::string serializeBackupMetadata(const BackupMetadata& meta) {
    std::string text;
    text.reserve(512);
    text += "title_id=" + std::to_string(static_cast<unsigned long long>(meta.titleId)) + "\n";
    text += "title_name=" + meta.titleName + "\n";
    text += "backup_name=" + meta.backupName + "\n";
    text += "revision_id=" + meta.revisionId + "\n";
    text += "device_id=" + meta.deviceId + "\n";
    text += "device_label=" + meta.deviceLabel + "\n";
    text += "user_id=" + meta.userId + "\n";
    text += "user_name=" + meta.userName + "\n";
    text += "source=" + meta.source + "\n";
    text += "created_at=" + std::to_string(static_cast<long long>(meta.createdAt)) + "\n";
    text += "device_priority=" + std::to_string(meta.devicePriority) + "\n";
    text += "size=" + std::to_string(static_cast<long long>(meta.size)) + "\n";
    text += "is_auto_backup=" + std::string(meta.isAutoBackup ? "1" : "0") + "\n";
    return text;
}

inline bool parseBackupMetadata(const std::string& text, BackupMetadata& outMeta) {
    outMeta = {};

    std::size_t lineStart = 0;
    while (lineStart <= text.size()) {
        const std::size_t lineEnd = text.find('\n', lineStart);
        std::string line = (lineEnd == std::string::npos)
            ? text.substr(lineStart)
            : text.substr(lineStart, lineEnd - lineStart);

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const std::size_t delimiter = line.find('=');
        if (delimiter != std::string::npos) {
            const std::string key = line.substr(0, delimiter);
            const std::string value = line.substr(delimiter + 1);

            if (key == "title_id") {
                outMeta.titleId = std::strtoull(value.c_str(), nullptr, 10);
            } else if (key == "title_name") {
                outMeta.titleName = value;
            } else if (key == "backup_name") {
                outMeta.backupName = value;
            } else if (key == "revision_id") {
                outMeta.revisionId = value;
            } else if (key == "device_id") {
                outMeta.deviceId = value;
            } else if (key == "device_label") {
                outMeta.deviceLabel = value;
            } else if (key == "user_id") {
                outMeta.userId = value;
            } else if (key == "user_name") {
                outMeta.userName = value;
            } else if (key == "source") {
                outMeta.source = value;
            } else if (key == "created_at") {
                outMeta.createdAt = static_cast<std::time_t>(std::atoll(value.c_str()));
            } else if (key == "device_priority") {
                outMeta.devicePriority = std::atoi(value.c_str());
            } else if (key == "size") {
                outMeta.size = std::atoll(value.c_str());
            } else if (key == "is_auto_backup") {
                outMeta.isAutoBackup = (value == "1" || value == "true");
            } else if (key == "isAutoBackup") {
                outMeta.isAutoBackup = (value == "1" || value == "true");
            }
        }

        if (lineEnd == std::string::npos) {
            break;
        }
        lineStart = lineEnd + 1;
    }

    return outMeta.titleId != 0 || !outMeta.backupName.empty();
}

inline bool writeBackupMetadataFile(const std::string& metadataPath, const BackupMetadata& meta) {
    FILE* file = std::fopen(metadataPath.c_str(), "w");
    if (!file) {
        return false;
    }

    const std::string text = serializeBackupMetadata(meta);
    const bool ok = std::fputs(text.c_str(), file) >= 0;
    std::fclose(file);
    return ok;
}

inline bool readBackupMetadataFile(const std::string& metadataPath, BackupMetadata& outMeta) {
    FILE* file = std::fopen(metadataPath.c_str(), "r");
    if (!file) {
        return false;
    }

    std::string text;
    char line[512];
    while (std::fgets(line, sizeof(line), file)) {
        text += line;
    }

    std::fclose(file);
    return parseBackupMetadata(text, outMeta);
}

inline bool copyMetadataFile(const std::string& sourcePath, const std::string& destinationPath) {
    FILE* source = std::fopen(sourcePath.c_str(), "r");
    if (!source) {
        return false;
    }

    FILE* destination = std::fopen(destinationPath.c_str(), "w");
    if (!destination) {
        std::fclose(source);
        return false;
    }

    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), source)) {
        if (std::fputs(buffer, destination) < 0) {
            std::fclose(source);
            std::fclose(destination);
            return false;
        }
    }

    std::fclose(source);
    std::fclose(destination);
    return true;
}

}
