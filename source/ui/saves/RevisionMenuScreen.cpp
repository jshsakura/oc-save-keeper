#include "ui/saves/RevisionMenuScreen.hpp"

#include "ui/saves/Runtime.hpp"
#include "utils/Language.hpp"
#include "utils/Logger.hpp"

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
    setAction(Button::Minus, Action{lang.get("history.restore_delete_hint"), [this]() {
        deleteSelected();
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
    // Check for async delete completion
    if (m_deleteThread.joinable() && !m_deleteInProgress) {
        m_deleteThread.join();
        Runtime::instance().setLoading(false);
        
        std::lock_guard<std::mutex> lock(m_deleteMutex);
        if (m_deleteSuccess) {
            Runtime::instance().notify(utils::Language::instance().get("sync.delete_success"));
            reload();
        } else {
            Runtime::instance().pushError(m_deleteMessage.empty() 
                ? utils::Language::instance().get("sync.delete_failed") 
                : m_deleteMessage);
        }
    }

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

    if (m_sidebar) {
        m_sidebar->draw();
    }
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

    Runtime::instance().notify(utils::Language::instance().get("ui.refresh_completed"));
}

void RevisionMenuScreen::restoreSelected() {
    const auto& lang = utils::Language::instance();
    
    if (m_entries.empty()) {
        Runtime::instance().notify(lang.get("ui.no_revision_entries"));
        return;
    }

    Runtime::instance().setLoading(true, lang.get("sync.restoring"));
    Runtime::instance().forceRender();

    const auto& entry = m_entries[m_index];
    SaveActionResult result;
    
    if (m_source == SaveSource::Cloud) {
        result = m_backend->download(m_titleId, entry.path);
    } else {
        result = m_backend->restore(m_titleId, entry.id, m_source);
    }

    Runtime::instance().setLoading(false);

    if (result.ok) {
        Runtime::instance().notify(lang.get("sync.restore_success"));
    } else {
        Runtime::instance().pushError(result.message.empty() ? lang.get("sync.restore_failed") : result.message);
    }
}

void RevisionMenuScreen::deleteSelected() {
    LOG_INFO("deleteSelected() called");
    const auto& lang = utils::Language::instance();
    
    if (m_entries.empty()) {
        LOG_WARNING("deleteSelected: no entries");
        return;
    }

    // Already running? Don't start another
    if (m_deleteInProgress) {
        LOG_WARNING("deleteSelected: already in progress");
        return;
    }

    const auto& entry = m_entries[m_index];
    LOG_INFO("deleteSelected: creating sidebar for entry %s", entry.label.c_str());

    m_sidebar = std::make_shared<Sidebar>(lang.get("ui.confirm_delete"), Sidebar::Side::Right);
    
    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.yes"), [this, entry]() {
        const auto& lang = utils::Language::instance();
        this->m_sidebar.reset();
        
        // Initialize state
        m_deleteInProgress = true;
        m_deleteSuccess = false;
        m_deleteMessage.clear();
        
        // Show loading (main thread) - NO forceRender!
        Runtime::instance().setLoading(true, lang.get("sync.deleting"));
        
        // Spawn worker thread
        m_deleteThread = std::thread([this, titleId = m_titleId, entryId = entry.id, source = m_source]() {
            auto result = m_backend->deleteRevision(titleId, entryId, source);
            
            std::lock_guard<std::mutex> lock(m_deleteMutex);
            m_deleteSuccess = result.ok;
            m_deleteMessage = result.message;
            m_deleteInProgress = false;
        });
        
        // Main thread returns immediately (non-blocking)
    }, false, lang.get("ui.confirm_delete_hint"));

    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.no"), [this]() {
        this->m_sidebar.reset();
    }, true);
}

RevisionMenuScreen::~RevisionMenuScreen() {
    if (m_deleteThread.joinable()) {
        m_deleteThread.join();
    }
}

} // namespace ui::saves
