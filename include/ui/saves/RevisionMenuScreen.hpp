#pragma once

#include "ui/saves/GridMenuBase.hpp"
#include "ui/saves/SaveBackend.hpp"

#include <memory>

namespace ui::saves {

class RevisionMenuScreen final : public GridMenuBase {
public:
    RevisionMenuScreen(std::shared_ptr<SaveBackend> backend, uint64_t titleId, SaveSource source, std::string titleLabel);

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

private:
    void reload();
    void restoreSelected();

    std::shared_ptr<SaveBackend> m_backend;
    std::unique_ptr<List> m_list;
    std::vector<SaveRevisionEntry> m_entries;
    uint64_t m_titleId = 0;
    SaveSource m_source = SaveSource::Local;
    std::string m_titleLabel;
    int m_index = 0;
    int m_layout = LayoutTypeGridDetail;
};

} // namespace ui::saves
