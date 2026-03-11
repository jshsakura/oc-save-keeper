#include "ui/saves/RevisionMenuScreen.hpp"

#include "ui/saves/Runtime.hpp"

namespace ui::saves {

RevisionMenuScreen::RevisionMenuScreen(std::shared_ptr<SaveBackend> backend, uint64_t titleId, SaveSource source, std::string titleLabel)
    : GridMenuBase(source == SaveSource::Cloud ? "Cloud Saves" : "Backup History", MenuFlagNone)
    , m_backend(std::move(backend))
    , m_titleId(titleId)
    , m_source(source)
    , m_titleLabel(std::move(titleLabel)) {
    onLayoutChange(m_list, m_layout);
    reload();

    setAction(Button::A, Action{source == SaveSource::Cloud ? "Download" : "Restore", [this]() {
        restoreSelected();
    }});
    setAction(Button::B, Action{"Back", [this]() {
        setPop();
    }});
    setAction(Button::X, Action{"Refresh", [this]() {
        reload();
    }});
}

const char* RevisionMenuScreen::shortTitle() const {
    return m_source == SaveSource::Cloud ? "Cloud" : "History";
}

void RevisionMenuScreen::update(const Controller& controller, const TouchInfo& touch) {
    MenuBase::update(controller, touch);

    if (!m_list) {
        return;
    }

    m_list->onUpdate(controller, touch, m_index, static_cast<int>(m_entries.size()), [this](bool tapped, int index) {
        m_index = index;
        if (tapped) {
            restoreSelected();
        }
    });
}

void RevisionMenuScreen::draw() {
    MenuBase::draw();

    if (!m_list) {
        return;
    }

    m_list->drawItems(static_cast<int>(m_entries.size()), [this](const Vec4& rect, int index) {
        const auto& entry = m_entries[index];
        const bool selected = index == m_index;
        drawEntry(m_layout, rect, selected, 0, entry.label.c_str(), entry.deviceLabel.c_str(), entry.sourceLabel.c_str());
    });
}

void RevisionMenuScreen::reload() {
    if (!m_backend) {
        m_entries.clear();
        m_index = 0;
        return;
    }
    setTitleSubHeading(m_titleLabel);
    m_entries = m_backend->listRevisions(m_titleId, m_source);
    if (m_entries.empty()) {
        m_index = 0;
    } else if (m_index >= static_cast<int>(m_entries.size())) {
        m_index = static_cast<int>(m_entries.size()) - 1;
    }
}

void RevisionMenuScreen::restoreSelected() {
    if (m_entries.empty() || !m_backend) {
        Runtime::instance().notify("No revision entries");
        return;
    }

    const auto& entry = m_entries[m_index];
    SaveActionResult result;
    if (m_source == SaveSource::Cloud) {
        result = m_backend->download(m_titleId, entry.id);
    } else {
        result = m_backend->restore(m_titleId, entry.id, m_source);
    }

    if (result.ok) {
        Runtime::instance().notify(result.message);
    } else {
        Runtime::instance().pushError(result.message);
    }
}

} // namespace ui::saves
