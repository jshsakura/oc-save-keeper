#include "ui/saves/SaveMenuScreen.hpp"

#include "ui/saves/RevisionMenuScreen.hpp"
#include "ui/saves/Runtime.hpp"
#include "utils/Language.hpp"

#include <algorithm>

namespace ui::saves {

SaveMenuScreen::SaveMenuScreen(std::shared_ptr<SaveBackend> backend)
    : GridMenuBase("Saves", MenuFlagNone)
    , m_backend(std::move(backend)) {
    const auto& lang = utils::Language::instance();
    onLayoutChange(m_list, m_layout);
    reload();

    setAction(Button::A, Action{lang.get("footer.controls.open"), [this]() {
        openActions();
    }});
    setAction(Button::X, Action{lang.get("ui.refresh"), [this]() {
        reload();
    }});
    setAction(Button::Minus, Action{lang.get("ui.sort_toggle"), [this]() {
        cycleSortMode();
    }});
    setAction(Button::B, Action{lang.get("detail.back"), [this]() {
        setPop();
    }});
}

const char* SaveMenuScreen::shortTitle() const {
    return utils::Language::instance().currentLang() == "ko" ? "세이브" : "Saves";
}

int SaveMenuScreen::firstVisibleIndex() const {
    if (!m_list) {
        return 0;
    }
    const int rowOffset = static_cast<int>(m_list->yoff() / m_list->maxY());
    return std::max(0, rowOffset * m_list->row());
}

int SaveMenuScreen::visibleCount() const {
    return m_list ? m_list->page() : 0;
}

std::string SaveMenuScreen::sortModeLabel() const {
    const auto& lang = utils::Language::instance();
    switch (m_sortMode) {
        case SortMode::Name:
            return lang.get("ui.sort_mode_name_short");
        case SortMode::Install:
            return lang.get("ui.sort_mode_install_short");
        case SortMode::Saved:
        default:
            return lang.get("ui.sort_mode_saved_short");
    }
}

void SaveMenuScreen::update(const Controller& controller, const TouchInfo& touch) {
    if (m_reloadOnNextFocus) {
        m_reloadOnNextFocus = false;
        reload();
    }

    auto sidebar = m_sidebar;
    if (sidebar) {
        sidebar->update(controller, touch);
        if (m_sidebar == sidebar && sidebar->shouldPop()) {
            m_sidebar.reset();
        }
        return;
    }

    MenuBase::update(controller, touch);

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
    applySort();
    if (m_entries.empty()) {
        m_index = 0;
    } else if (m_index >= static_cast<int>(m_entries.size())) {
        m_index = static_cast<int>(m_entries.size()) - 1;
    }
    
    Runtime::instance().notify(utils::Language::instance().get("ui.refresh_completed"));
}

void SaveMenuScreen::applySort() {
    std::sort(m_entries.begin(), m_entries.end(), [this](const SaveTitleEntry& lhs, const SaveTitleEntry& rhs) {
        if (m_sortMode == SortMode::Name) {
            if (lhs.name != rhs.name) {
                return lhs.name < rhs.name;
            }
        } else if (m_sortMode == SortMode::Install) {
            if (lhs.installOrder != rhs.installOrder) {
                return lhs.installOrder > rhs.installOrder;
            }
        } else {
            if (lhs.latestBackupTimestamp != rhs.latestBackupTimestamp) {
                return lhs.latestBackupTimestamp > rhs.latestBackupTimestamp;
            }
        }

        if (lhs.titleId != rhs.titleId) {
            return lhs.titleId < rhs.titleId;
        }
        return lhs.sourceOrder < rhs.sourceOrder;
    });
}

void SaveMenuScreen::cycleSortMode() {
    uint64_t selectedTitleId = 0;
    bool selectedIsDevice = false;
    bool selectedIsSystem = false;
    if (!m_entries.empty() && m_index >= 0 && m_index < static_cast<int>(m_entries.size())) {
        const auto& current = m_entries[m_index];
        selectedTitleId = current.titleId;
        selectedIsDevice = current.isDevice;
        selectedIsSystem = current.isSystem;
    }

    if (m_sortMode == SortMode::Name) {
        m_sortMode = SortMode::Install;
    } else if (m_sortMode == SortMode::Install) {
        m_sortMode = SortMode::Saved;
    } else {
        m_sortMode = SortMode::Name;
    }

    applySort();

    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
        const auto& entry = m_entries[i];
        if (entry.titleId == selectedTitleId && entry.isDevice == selectedIsDevice && entry.isSystem == selectedIsSystem) {
            m_index = i;
            break;
        }
    }

    const auto& lang = utils::Language::instance();
    Runtime::instance().notify(lang.get("ui.sort_mode_prefix") + " " + sortModeLabel());
}

void SaveMenuScreen::openActions() {
    const auto& lang = utils::Language::instance();
    if (m_entries.empty()) {
        Runtime::instance().notify(lang.get("ui.no_save_entries"));
        return;
    }

    const auto entry = m_entries[m_index];
    const bool isAuthed = m_backend->isCloudAuthenticated();

    m_sidebar = std::make_shared<Sidebar>(entry.name, Sidebar::Side::Right);
    
    m_sidebar->add<SidebarEntryCallback>(lang.get("detail.backup"), [this, entry]() {
        const auto& lang = utils::Language::instance();
        Runtime::instance().setLoading(true, lang.get("sync.creating_local"));
        Runtime::instance().forceRender();
        m_isOperationInProgress = true;
        
        m_backend->setTargetType(entry.titleId, entry.isDevice, entry.isSystem);
        const auto result = m_backend->backup(entry.titleId);

        m_isOperationInProgress = false;
        Runtime::instance().setLoading(false);
        if (result.ok) {
            Runtime::instance().notify(lang.get("sync.success"));
            reload();
        } else {
            Runtime::instance().pushError(result.message);
        }
    }, false, lang.get("ui.action_backup_hint"));

    // 2. 로컬 버전 (Local Versions)
    m_sidebar->add<SidebarEntryCallback>(lang.get("detail.history"), [this, entry]() {
        m_backend->setTargetType(entry.titleId, entry.isDevice, entry.isSystem);
        openHistory(SaveSource::Local);
    }, true, lang.get("ui.action_history_hint"));

    auto uploadBtn = m_sidebar->add<SidebarEntryCallback>(lang.get("detail.upload"), [this, entry]() {
        const auto& lang = utils::Language::instance();
        Runtime::instance().setLoading(true, lang.get("sync.uploading"));
        Runtime::instance().forceRender();
        m_isOperationInProgress = true;
        
        m_backend->setTargetType(entry.titleId, entry.isDevice, entry.isSystem);
        const auto result = m_backend->upload(entry.titleId);
        
        m_isOperationInProgress = false;
        Runtime::instance().setLoading(false);
        if (result.ok) {
            Runtime::instance().notify(lang.get("sync.success"));
            reload(); 
        } else {
            Runtime::instance().pushError(lang.get("sync.upload_failed"));
        }
    }, false, lang.get("ui.action_upload_hint"));

    if (!isAuthed) {
        uploadBtn->setEnabled(false);
        uploadBtn->setInfo(lang.get("history.login_needed"));
    }

    // 4. 드롭박스 이력 (Dropbox History)
    auto cloudHistoryBtn = m_sidebar->add<SidebarEntryCallback>(lang.get("history.cloud"), [this, entry]() {
        m_backend->setTargetType(entry.titleId, entry.isDevice, entry.isSystem);
        openHistory(SaveSource::Cloud);
    }, true, lang.get("ui.action_cloud_hint"));

    if (!isAuthed) {
        cloudHistoryBtn->setEnabled(false);
        cloudHistoryBtn->setInfo(lang.get("history.login_needed"));
    }
}

void SaveMenuScreen::openHistory(SaveSource source) {
    if (m_entries.empty()) {
        return;
    }

    const auto& entry = m_entries[m_index];
    m_reloadOnNextFocus = true;
    Runtime::instance().push(std::make_shared<RevisionMenuScreen>(m_backend, entry.titleId, source, entry.name, entry.isSystem));
}

} // namespace ui::saves
