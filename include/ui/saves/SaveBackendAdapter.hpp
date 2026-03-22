#pragma once

#include "ui/saves/SaveBackend.hpp"

namespace core {
class SaveManager;
}

namespace network {
class Dropbox;
}

namespace ui::saves {

class SaveBackendAdapter final : public SaveBackend {
public:
    SaveBackendAdapter(core::SaveManager& saveManager, network::Dropbox& dropbox);

    std::vector<SaveTitleEntry> listTitles() override;
    std::vector<SaveRevisionEntry> listRevisions(uint64_t titleId, SaveSource source) override;
    SaveActionResult backup(uint64_t titleId) override;
    SaveActionResult restore(uint64_t titleId, const std::string& revisionId, SaveSource source) override;
    SaveActionResult upload(uint64_t titleId) override;
    SaveActionResult download(uint64_t titleId, const std::string& revisionId) override;
    SaveActionResult refresh(uint64_t titleId) override;
    SaveActionResult deleteRevision(uint64_t titleId, const std::string& revisionId, SaveSource source) override;
    SaveActionResult toggleFavorite(uint64_t titleId, const std::string& revisionPath, SaveSource source) override;
    void setTargetType(uint64_t titleId, bool isDevice, bool isSystem) override;
    bool isCloudAuthenticated() const override;
    void invalidateCache();
    void invalidateAllCaches();

private:
    core::SaveManager& m_saveManager;
    network::Dropbox& m_dropbox;
};

} // namespace ui::saves
