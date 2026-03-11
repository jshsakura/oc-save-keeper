#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace ui::saves {

enum class SaveSource {
    Local,
    Cloud,
};

enum class SaveAction {
    Backup,
    Restore,
    Upload,
    Download,
    Refresh,
};

struct SaveTitleEntry {
    uint64_t titleId = 0;
    std::string name;
    std::string author;
    std::string iconPath;
    std::string subtitle;
    bool hasLocalBackup = false;
    bool hasCloudBackup = false;
};

struct SaveRevisionEntry {
    std::string id;
    std::string label;
    std::string path;
    std::string deviceLabel;
    std::string userLabel;
    std::string sourceLabel;
    std::time_t timestamp = 0;
    int64_t size = 0;
    SaveSource source = SaveSource::Local;
};

struct SaveActionResult {
    bool ok = false;
    std::string message;
};

class SaveBackend {
public:
    virtual ~SaveBackend() = default;

    virtual std::vector<SaveTitleEntry> listTitles() = 0;
    virtual std::vector<SaveRevisionEntry> listRevisions(uint64_t titleId, SaveSource source) = 0;

    virtual SaveActionResult backup(uint64_t titleId) = 0;
    virtual SaveActionResult restore(uint64_t titleId, const std::string& revisionId, SaveSource source) = 0;
    virtual SaveActionResult upload(uint64_t titleId) = 0;
    virtual SaveActionResult download(uint64_t titleId, const std::string& revisionId) = 0;
    virtual SaveActionResult refresh(uint64_t titleId) = 0;
};

} // namespace ui::saves
