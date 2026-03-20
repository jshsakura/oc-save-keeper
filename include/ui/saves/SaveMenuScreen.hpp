#pragma once

#include "ui/saves/GridMenuBase.hpp"
#include "ui/saves/SaveBackend.hpp"
#include "ui/saves/Sidebar.hpp"

#include <memory>

namespace ui::saves {

class SaveMenuScreen final : public GridMenuBase {
public:
    enum class SortMode {
        Name,
        Install,
        Saved,
    };

    explicit SaveMenuScreen(std::shared_ptr<SaveBackend> backend);

    const char* shortTitle() const override;
    void update(const Controller& controller, const TouchInfo& touch) override;
    void draw() override;
    ObjectKind kind() const override {
        return ObjectKind::SaveMenuScreen;
    }

    const std::vector<SaveTitleEntry>& entries() const {
        return m_entries;
    }

    int index() const {
        return m_index;
    }

    const std::shared_ptr<Sidebar>& sidebar() const {
        return m_sidebar;
    }

    int firstVisibleIndex() const;
    int visibleCount() const;

    SortMode sortMode() const {
        return m_sortMode;
    }

    std::string sortModeLabel() const;

private:
    void reload();
    void applySort();
    void cycleSortMode();
    void openActions();
    void openHistory(SaveSource source);

    std::shared_ptr<SaveBackend> m_backend;
    std::unique_ptr<List> m_list;
    std::shared_ptr<Sidebar> m_sidebar;
    std::vector<SaveTitleEntry> m_entries;
    int m_index = 0;
    int m_layout = LayoutTypeGrid;
    bool m_isOperationInProgress = false;
    SortMode m_sortMode = SortMode::Install;
    bool m_reloadOnNextFocus = false;
};

} // namespace ui::saves
