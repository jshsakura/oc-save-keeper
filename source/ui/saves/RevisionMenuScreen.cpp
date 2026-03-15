#include "ui/saves/RevisionMenuScreen.hpp"

#include "ui/saves/Runtime.hpp"
#include "utils/Language.hpp"

namespace ui::saves {

RevisionMenuScreen::RevisionMenuScreen(std::shared_ptr<SaveBackend> backend, uint64_t titleId, SaveSource source, std::string titleLabel)
    : GridMenuBase(source == SaveSource::Cloud ? "Cloud Saves" : "Backup History", MenuFlagNone)
    , m_backend(std::move(backend))
    , m_titleId(titleId)
    , m_source(source)
    , m_titleLabel(std::move(titleLabel)) {
    const auto& lang = utils::Language::instance();
    onLayoutChange(m_list, m_layout);
    reload();

    setAction(Button::A, Action{source == SaveSource::Cloud ? lang.get("detail.download") : lang.get("history.restore"), [this]() {
        restoreSelected();
    }});
    setAction(Button::B, Action{lang.get("detail.back"), [this]() {
        setPop();
    }});
    setAction(Button::X, Action{lang.get("ui.refresh"), [this]() {
        reload();
    }});
}

const char* RevisionMenuScreen::shortTitle() const {
    if (utils::Language::instance().currentLang() == "ko") {
        return m_source == SaveSource::Cloud ? "클라우드" : "이력";
    }
    return m_source == SaveSource::Cloud ? "Cloud" : "History";
}

int RevisionMenuScreen::firstVisibleIndex() const {
    if (!m_list) {
        return 0;
    }
    const int rowOffset = static_cast<int>(m_list->yoff() / m_list->maxY());
    return std::max(0, rowOffset * m_list->row());
}

int RevisionMenuScreen::visibleCount() const {
    return m_list ? m_list->page() : 0;
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
    const auto& lang = utils::Language::instance();
    if (m_entries.empty() || !m_backend) {
        Runtime::instance().notify(lang.get("ui.no_revision_entries"));
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
