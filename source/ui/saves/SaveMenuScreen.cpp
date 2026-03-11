#include "ui/saves/SaveMenuScreen.hpp"

#include "ui/saves/RevisionMenuScreen.hpp"
#include "ui/saves/Runtime.hpp"

namespace ui::saves {

SaveMenuScreen::SaveMenuScreen(std::shared_ptr<SaveBackend> backend)
    : GridMenuBase("Saves", MenuFlagNone)
    , m_backend(std::move(backend)) {
    onLayoutChange(m_list, m_layout);
    reload();

    setAction(Button::A, Action{"Open", [this]() {
        openActions();
    }});
    setAction(Button::X, Action{"Refresh", [this]() {
        reload();
    }});
    setAction(Button::B, Action{"Back", [this]() {
        setPop();
    }});
}

const char* SaveMenuScreen::shortTitle() const {
    return "Saves";
}

void SaveMenuScreen::update(const Controller& controller, const TouchInfo& touch) {
    MenuBase::update(controller, touch);

    if (m_sidebar) {
        m_sidebar->update(controller, touch);
        if (m_sidebar->shouldPop()) {
            m_sidebar.reset();
        }
        return;
    }

    if (!m_list) {
        return;
    }

    m_list->onUpdate(controller, touch, m_index, static_cast<int>(m_entries.size()), [this](bool tapped, int index) {
        m_index = index;
        if (tapped) {
            openActions();
        }
    });
}

void SaveMenuScreen::draw() {
    MenuBase::draw();

    if (!m_list) {
        return;
    }

    m_list->drawItems(static_cast<int>(m_entries.size()), [this](const Vec4& rect, int index) {
        const bool selected = index == m_index;
        const auto& entry = m_entries[index];
        drawEntry(m_layout, rect, selected, 0, entry.name.c_str(), entry.author.c_str(), entry.subtitle.c_str());
    });

    if (m_sidebar) {
        m_sidebar->draw();
    }
}

void SaveMenuScreen::reload() {
    if (!m_backend) {
        m_entries.clear();
        m_index = 0;
        return;
    }

    m_entries = m_backend->listTitles();
    if (m_entries.empty()) {
        m_index = 0;
    } else if (m_index >= static_cast<int>(m_entries.size())) {
        m_index = static_cast<int>(m_entries.size()) - 1;
    }
}

void SaveMenuScreen::openActions() {
    if (m_entries.empty()) {
        Runtime::instance().notify("No save entries");
        return;
    }

    const auto entry = m_entries[m_index];
    m_sidebar = std::make_shared<Sidebar>(entry.name, Sidebar::Side::Right);
    m_sidebar->add<SidebarEntryCallback>("Backup", [this, entry]() {
        const auto result = m_backend->backup(entry.titleId);
        if (result.ok) {
            Runtime::instance().notify(result.message);
            reload();
        } else {
            Runtime::instance().pushError(result.message);
        }
    }, false, "Create a local backup");
    m_sidebar->add<SidebarEntryCallback>("Upload", [this, entry]() {
        const auto result = m_backend->upload(entry.titleId);
        if (result.ok) {
            Runtime::instance().notify(result.message);
        } else {
            Runtime::instance().pushError(result.message);
        }
    }, false, "Upload latest backup to Dropbox");
    m_sidebar->add<SidebarEntryCallback>("History", [this]() {
        openHistory(SaveSource::Local);
    }, true, "Open local revision history");
    m_sidebar->add<SidebarEntryCallback>("Cloud", [this]() {
        openHistory(SaveSource::Cloud);
    }, true, "Open cloud revisions");
}

void SaveMenuScreen::openHistory(SaveSource source) {
    if (m_entries.empty()) {
        return;
    }

    const auto& entry = m_entries[m_index];
    Runtime::instance().push(std::make_shared<RevisionMenuScreen>(m_backend, entry.titleId, source, entry.name));
}

} // namespace ui::saves
