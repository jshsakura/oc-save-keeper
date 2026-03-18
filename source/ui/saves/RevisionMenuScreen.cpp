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
    LOG_INFO("restoreSelected() called");

    // Guard: 이미 sidebar가 열려있으면 무시
    if (m_sidebar) {
        LOG_WARNING("restoreSelected: sidebar already open");
        return;
    }

    const auto& lang = utils::Language::instance();
    
    if (m_entries.empty()) {
        LOG_WARNING("restoreSelected: no entries");
        Runtime::instance().notify(lang.get("ui.no_revision_entries"));
        return;
    }

    if (m_index < 0 || m_index >= static_cast<int>(m_entries.size())) {
        LOG_ERROR("restoreSelected: invalid index %d (size=%zu)", m_index, m_entries.size());
        return;
    }

    const auto& entry = m_entries[m_index];
    LOG_INFO("restoreSelected: creating sidebar for entry %s (id=%s)", entry.label.c_str(), entry.id.c_str());

    m_restoreData = std::make_shared<RestoreTaskData>();
    m_restoreData->titleId = m_titleId;
    m_restoreData->entryId = entry.id;
    m_restoreData->entryPath = entry.path;
    m_restoreData->entryLabel = entry.label;
    m_restoreData->source = m_source;

    m_sidebar = std::make_shared<Sidebar>(lang.get("ui.confirm_restore"), Sidebar::Side::Right);
    
    const bool holdRequired = true;
    const std::string yesHint = holdRequired ? lang.get("ui.hold_to_confirm") : lang.get("ui.confirm_restore_hint");
    
    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.yes"), [this]() {
        LOG_INFO("restoreSelected: YES clicked");
        const auto& lang = utils::Language::instance();
        this->m_sidebar.reset();
        
        if (!m_restoreData) {
            LOG_ERROR("restoreSelected: m_restoreData is null");
            Runtime::instance().pushError(lang.get("sync.restore_failed"));
            return;
        }
        
        LOG_INFO("restoreSelected: entryId=%s", m_restoreData->entryId.c_str());
        
        Runtime::instance().setLoading(true, lang.get("sync.restoring"));
        Runtime::instance().forceRender();
        
        SaveActionResult result;
        
        if (m_restoreData->source == SaveSource::Cloud) {
            result = m_backend->download(m_restoreData->titleId, m_restoreData->entryPath);
        } else {
            result = m_backend->restore(m_restoreData->titleId, m_restoreData->entryId, m_restoreData->source);
        }
        
        Runtime::instance().setLoading(false);
        
        if (result.ok) {
            Runtime::instance().notify(lang.get("sync.restore_success"));
        } else {
            Runtime::instance().pushError(result.message.empty() ? lang.get("sync.restore_failed") : result.message);
        }
        
        m_restoreData.reset();
    }, false, yesHint, holdRequired);

    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.no"), [this]() {
        LOG_INFO("restoreSelected: NO clicked");
        this->m_sidebar.reset();
        m_restoreData.reset();
    }, true);
}

void RevisionMenuScreen::deleteSelected() {
    LOG_INFO("deleteSelected() called");

    // Guard: 이미 sidebar가 열려있으면 무시
    if (m_sidebar) {
        LOG_WARNING("deleteSelected: sidebar already open");
        return;
    }

    const auto& lang = utils::Language::instance();
    
    if (m_entries.empty()) {
        LOG_WARNING("deleteSelected: no entries");
        return;
    }

    if (m_deleteInProgress) {
        LOG_WARNING("deleteSelected: already in progress");
        return;
    }

    if (m_index < 0 || m_index >= static_cast<int>(m_entries.size())) {
        LOG_ERROR("deleteSelected: invalid index %d (size=%zu)", m_index, m_entries.size());
        return;
    }

    const auto& entry = m_entries[m_index];
    LOG_INFO("deleteSelected: creating sidebar for entry %s (id=%s)", entry.label.c_str(), entry.id.c_str());

    m_deleteData = std::make_shared<DeleteTaskData>();
    m_deleteData->titleId = m_titleId;
    m_deleteData->entryId = entry.id;
    m_deleteData->entryPath = entry.path;
    m_deleteData->entryLabel = entry.label;
    m_deleteData->source = m_source;
    m_deleteData->isSystem = m_isSystem;

    m_sidebar = std::make_shared<Sidebar>(lang.get("ui.confirm_delete"), Sidebar::Side::Right);
    
    const bool holdRequired = true;
    const std::string yesHint = holdRequired ? lang.get("ui.hold_to_confirm") : lang.get("ui.confirm_delete_hint");
    
    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.yes"), [this]() {
        LOG_INFO("deleteSelected: YES clicked");
        const auto& lang = utils::Language::instance();
        this->m_sidebar.reset();
        
        if (!m_deleteData) {
            LOG_ERROR("deleteSelected: m_deleteData is null");
            Runtime::instance().pushError(lang.get("sync.delete_failed"));
            return;
        }
        
        LOG_INFO("deleteSelected: entryId=%s", m_deleteData->entryId.c_str());
        
        {
            std::lock_guard<std::mutex> lock(m_deleteMutex);
            m_deleteInProgress = true;
            m_deleteSuccess = false;
            m_deleteMessage.clear();
        }
        
        Runtime::instance().setLoading(true, lang.get("sync.deleting"));
        
        auto dataPtr = m_deleteData;
        auto backendPtr = m_backend;
        
        m_deleteThread = std::thread([this, dataPtr, backendPtr]() {
            LOG_INFO("deleteSelected: worker started");
            LOG_INFO("deleteSelected: entryId=%s len=%zu", dataPtr->entryId.c_str(), dataPtr->entryId.size());
            
            auto result = backendPtr->deleteRevision(dataPtr->titleId, dataPtr->entryId, dataPtr->source);
            LOG_INFO("deleteSelected: result ok=%d msg=%s", result.ok, result.message.c_str());
            
            std::lock_guard<std::mutex> lock(m_deleteMutex);
            m_deleteSuccess = result.ok;
            m_deleteMessage = result.message;
            m_deleteInProgress = false;
        });
    }, false, yesHint, holdRequired);

    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.no"), [this]() {
        LOG_INFO("deleteSelected: NO clicked");
        this->m_sidebar.reset();
        m_deleteData.reset();
    }, true);
}

RevisionMenuScreen::~RevisionMenuScreen() {
    if (m_deleteThread.joinable()) {
        m_deleteThread.join();
    }
}

} // namespace ui::saves
