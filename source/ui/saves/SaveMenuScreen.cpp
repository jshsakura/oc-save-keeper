#include "ui/saves/SaveMenuScreen.hpp"

#include "ui/saves/RevisionMenuScreen.hpp"
#include "ui/saves/Runtime.hpp"
#include "utils/Language.hpp"

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

void SaveMenuScreen::update(const Controller& controller, const TouchInfo& touch) {
    if (m_sidebar) {
        m_sidebar->update(controller, touch);
        if (m_sidebar->shouldPop()) {
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
    if (m_entries.empty()) {
        m_index = 0;
    } else if (m_index >= static_cast<int>(m_entries.size())) {
        m_index = static_cast<int>(m_entries.size()) - 1;
    }
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
    
    // 1. 로컬 백업 (Local Backup)
    m_sidebar->add<SidebarEntryCallback>(lang.get("detail.backup"), [this, entry]() {
        const auto& lang = utils::Language::instance();
        Runtime::instance().notify(lang.get("sync.syncing"));
        m_isOperationInProgress = true;
        
        m_backend->setTargetType(entry.titleId, entry.isDevice, entry.isSystem);
        const auto result = m_backend->backup(entry.titleId);

        m_isOperationInProgress = false;
        if (result.ok) {
            Runtime::instance().notify(lang.get("sync.complete"));
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

    // 3. 드롭박스 업로드 (Dropbox Upload)
    auto uploadBtn = m_sidebar->add<SidebarEntryCallback>(lang.get("detail.upload"), [this, entry]() {
        const auto& lang = utils::Language::instance();
        Runtime::instance().notify(lang.get("sync.uploading"));
        m_isOperationInProgress = true;
        
        m_backend->setTargetType(entry.titleId, entry.isDevice, entry.isSystem);
        const auto result = m_backend->upload(entry.titleId);
        
        m_isOperationInProgress = false;
        if (result.ok) {
            Runtime::instance().notify(lang.get("sync.upload_completed"));
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
    Runtime::instance().push(std::make_shared<RevisionMenuScreen>(m_backend, entry.titleId, source, entry.name));
}

} // namespace ui::saves
