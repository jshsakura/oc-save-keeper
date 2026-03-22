#pragma once

#include "ui/saves/GridMenuBase.hpp"
#include "ui/saves/SaveBackend.hpp"
#include "ui/saves/Sidebar.hpp"

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

namespace ui::saves {

struct DeleteTaskData {
    uint64_t titleId = 0;
    std::string entryId;
    std::string entryPath;
    std::string entryLabel;
    SaveSource source = SaveSource::Local;
    bool isSystem = false;
};

struct RestoreTaskData {
    uint64_t titleId = 0;
    std::string entryId;
    std::string entryPath;
    std::string entryLabel;
    SaveSource source = SaveSource::Local;
};

struct FavoriteTaskData {
    uint64_t titleId = 0;
    std::string entryId;
    std::string entryPath;
    SaveSource source = SaveSource::Local;
    bool newFavoriteState = false;
    int entryIndex = 0;
};

class RevisionMenuScreen final : public GridMenuBase {
public:
    RevisionMenuScreen(std::shared_ptr<SaveBackend> backend, uint64_t titleId, SaveSource source, std::string titleLabel, bool isSystem = false);
    ~RevisionMenuScreen();

    const char* shortTitle() const override;
    void update(const Controller& controller, const TouchInfo& touch) override;
    void draw() override;
    ObjectKind kind() const override {
        return ObjectKind::RevisionMenuScreen;
    }

    const std::vector<SaveRevisionEntry>& entries() const {
        return m_entries;
    }

    int index() const {
        return m_index;
    }

    const std::string& titleLabel() const {
        return m_titleLabel;
    }

    const std::shared_ptr<Sidebar>& sidebar() const {
        return m_sidebar;
    }

    int firstVisibleIndex() const;
    int visibleCount() const;

private:
    void reload();
    void openActions();
    void restoreSelected();
    void deleteSelected();
    void toggleFavoriteSelected();

    std::shared_ptr<SaveBackend> m_backend;
    std::unique_ptr<List> m_list;
    std::shared_ptr<Sidebar> m_sidebar;
    std::vector<SaveRevisionEntry> m_entries;
    uint64_t m_titleId = 0;
    SaveSource m_source = SaveSource::Local;
    std::string m_titleLabel;
    int m_index = 0;
    int m_layout = LayoutTypeVertical;
    bool m_isSystem = false;

    std::thread m_deleteThread;
    std::atomic<bool> m_deleteInProgress{false};
    std::atomic<bool> m_deleteSuccess{false};
    std::atomic<bool> m_cancelDelete{false};
    std::string m_deleteMessage;
    std::mutex m_deleteMutex;
    std::shared_ptr<DeleteTaskData> m_deleteData;
    
    std::shared_ptr<RestoreTaskData> m_restoreData;
    
    // Favorite toggle async state
    std::thread m_favoriteThread;
    std::atomic<bool> m_favoriteInProgress{false};
    std::atomic<bool> m_favoriteSuccess{false};
    std::atomic<bool> m_cancelFavorite{false};
    std::string m_favoriteMessage;
    std::mutex m_favoriteMutex;
    std::shared_ptr<FavoriteTaskData> m_favoriteData;
};

} // namespace ui::saves
