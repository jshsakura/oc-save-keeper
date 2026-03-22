#include "ui/saves/RevisionMenuScreen.hpp"

#include "ui/saves/Runtime.hpp"
#include "utils/Language.hpp"
#include "utils/Logger.hpp"

namespace ui::saves {

RevisionMenuScreen::RevisionMenuScreen(std::shared_ptr<SaveBackend> backend, uint64_t titleId, SaveSource source, std::string titleLabel, bool isSystem)
    : GridMenuBase(source == SaveSource::Cloud ? "Cloud Saves" : "Backup History", MenuFlagNone)
    , m_backend(std::move(backend))
    , m_titleId(titleId)
    , m_source(source)
    , m_titleLabel(std::move(titleLabel))
    , m_isSystem(isSystem) {
    const auto& lang = utils::Language::instance();
    onLayoutChange(m_list, m_layout);
    reload();

    setAction(Button::A, Action{lang.get("footer.controls.open"), [this]() {
        openActions();
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
    // Check for async delete completion
    if (m_deleteThread.joinable() && !m_deleteInProgress) {
        m_deleteThread.join();
        Runtime::instance().setLoading(false);
        
        std::lock_guard<std::mutex> lock(m_deleteMutex);
        if (m_deleteSuccess) {
            Runtime::instance().notify(utils::Language::instance().get("sync.delete_success"));
            m_deleteData.reset();
            reload();
        } else {
            Runtime::instance().pushError(m_deleteMessage.empty() 
                ? utils::Language::instance().get("sync.delete_failed") 
                : m_deleteMessage);
        }
    }

    // Check for async favorite completion
    if (m_favoriteThread.joinable() && !m_favoriteInProgress) {
        m_favoriteThread.join();
        Runtime::instance().setLoading(false);
        
        std::lock_guard<std::mutex> lock(m_favoriteMutex);
        if (m_favoriteSuccess) {
            Runtime::instance().notify(utils::Language::instance().get("sync.favorite_success"));
        } else {
            Runtime::instance().pushError(m_favoriteMessage.empty() 
                ? utils::Language::instance().get("sync.favorite_failed") 
                : m_favoriteMessage);
        }
        m_favoriteData.reset();
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
    
    const auto& lang = utils::Language::instance();
    
    Runtime::instance().setLoading(true, lang.get("sync.querying"));
    Runtime::instance().forceRender();
    
    setTitleSubHeading(m_titleLabel);
    m_entries = m_backend->listRevisions(m_titleId, m_source);
    
    Runtime::instance().setLoading(false);
    
    if (m_entries.empty()) {
        m_index = 0;
    } else if (m_index >= static_cast<int>(m_entries.size())) {
        m_index = static_cast<int>(m_entries.size()) - 1;
    }

    Runtime::instance().notify(lang.get("ui.refresh_completed"));
}

void RevisionMenuScreen::openActions() {
    if (m_sidebar) {
        return;
    }

    if (m_entries.empty()) {
        Runtime::instance().notify(utils::Language::instance().get("ui.no_revision_entries"));
        return;
    }

    if (m_index < 0 || m_index >= static_cast<int>(m_entries.size())) {
        return;
    }

    const auto& entry = m_entries[m_index];
    const bool isKorean = utils::Language::instance().currentLang() == "ko";
    
    m_sidebar = std::make_shared<Sidebar>(
        isKorean ? "동작 선택" : "Select Action", 
        Sidebar::Side::Right
    );
    
    m_sidebar->add<SidebarEntryCallback>(
        isKorean ? "복원" : utils::Language::instance().get("history.restore"),
        [this]() { restoreSelected(); },
        false
    );
    
    m_sidebar->add<SidebarEntryCallback>(
        isKorean ? "삭제" : "Delete",
        [this]() { deleteSelected(); },
        false
    );
    
    const std::string favoriteLabel = entry.isFavorite
        ? (isKorean ? "즐겨찾기 해제" : "Remove from Favorites")
        : (isKorean ? "즐겨찾기 등록" : "Add to Favorites");
    
    m_sidebar->add<SidebarEntryCallback>(
        favoriteLabel,
        [this]() { toggleFavoriteSelected(); },
        false
    );
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
    m_sidebar->setInitialIndex(1);
    const std::string yesHint = lang.get("ui.confirm_restore_hint");
    
    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.yes"), [this]() {
        LOG_INFO("restoreSelected: YES clicked");
        const auto& lang = utils::Language::instance();
        
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
    }, true, yesHint);

    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.no"), [this]() {
        LOG_INFO("restoreSelected: NO clicked");
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
    m_sidebar->setInitialIndex(1);
    const std::string yesHint = lang.get("ui.confirm_delete_hint");
    
    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.yes"), [this]() {
        LOG_INFO("deleteSelected: YES clicked");
        const auto& lang = utils::Language::instance();
        
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
            m_cancelDelete = false;
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
            
            if (m_cancelDelete) {
                LOG_INFO("deleteSelected: cancelled, skipping result handling");
                return;
            }
            
            std::lock_guard<std::mutex> lock(m_deleteMutex);
            m_deleteSuccess = result.ok;
            m_deleteMessage = result.message;
            m_deleteInProgress = false;
        });
    }, true, yesHint);

    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.no"), [this]() {
        LOG_INFO("deleteSelected: NO clicked");
        m_deleteData.reset();
    }, true);
}

void RevisionMenuScreen::toggleFavoriteSelected() {
    LOG_INFO("toggleFavoriteSelected() called");

    if (m_sidebar) {
        LOG_WARNING("toggleFavoriteSelected: sidebar already open");
        return;
    }

    const auto& lang = utils::Language::instance();
    
    if (m_entries.empty()) {
        LOG_WARNING("toggleFavoriteSelected: no entries");
        return;
    }

    if (m_favoriteInProgress) {
        LOG_WARNING("toggleFavoriteSelected: already in progress");
        return;
    }

    if (m_index < 0 || m_index >= static_cast<int>(m_entries.size())) {
        LOG_ERROR("toggleFavoriteSelected: invalid index %d (size=%zu)", m_index, m_entries.size());
        return;
    }

    const auto& entry = m_entries[m_index];
    const bool newFavoriteState = !entry.isFavorite;
    const std::string confirmMsg = newFavoriteState 
        ? lang.get("ui.confirm_favorite_add") 
        : lang.get("ui.confirm_favorite_remove");
    
    LOG_INFO("toggleFavoriteSelected: creating sidebar for entry %s (current=%d, new=%d)", 
             entry.id.c_str(), entry.isFavorite, newFavoriteState);

    m_favoriteData = std::make_shared<FavoriteTaskData>();
    m_favoriteData->titleId = m_titleId;
    m_favoriteData->entryId = entry.id;
    m_favoriteData->entryPath = entry.path;
    m_favoriteData->source = m_source;
    m_favoriteData->newFavoriteState = newFavoriteState;
    m_favoriteData->entryIndex = m_index;

    m_sidebar = std::make_shared<Sidebar>(confirmMsg, Sidebar::Side::Right);
    m_sidebar->setInitialIndex(1);
    const std::string yesHint = newFavoriteState 
        ? lang.get("ui.confirm_favorite_add_hint") 
        : lang.get("ui.confirm_favorite_remove_hint");
    
    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.yes"), [this]() {
        LOG_INFO("toggleFavoriteSelected: YES clicked");
        const auto& lang = utils::Language::instance();
        
        if (!m_favoriteData) {
            LOG_ERROR("toggleFavoriteSelected: m_favoriteData is null");
            Runtime::instance().pushError(lang.get("sync.favorite_failed"));
            return;
        }
        
        const int idx = m_favoriteData->entryIndex;
        if (idx < 0 || idx >= static_cast<int>(m_entries.size())) {
            LOG_ERROR("toggleFavoriteSelected: invalid stored index");
            Runtime::instance().pushError(lang.get("sync.favorite_failed"));
            m_favoriteData.reset();
            return;
        }
        
        const bool previousState = m_entries[idx].isFavorite;
        m_entries[idx].isFavorite = m_favoriteData->newFavoriteState;
        
        LOG_INFO("toggleFavoriteSelected: entryId=%s, toggling to %d", 
                 m_favoriteData->entryId.c_str(), m_favoriteData->newFavoriteState);
        
        {
            std::lock_guard<std::mutex> lock(m_favoriteMutex);
            m_favoriteInProgress = true;
            m_favoriteSuccess = false;
            m_cancelFavorite = false;
            m_favoriteMessage.clear();
        }
        
        Runtime::instance().setLoading(true, lang.get("sync.favorite_updating"));
        
        auto dataPtr = m_favoriteData;
        auto backendPtr = m_backend;
        
        m_favoriteThread = std::thread([this, dataPtr, backendPtr, previousState, idx]() {
            LOG_INFO("toggleFavoriteSelected: worker started");
            
            auto result = backendPtr->toggleFavorite(dataPtr->titleId, dataPtr->entryPath, dataPtr->source);
            LOG_INFO("toggleFavoriteSelected: result ok=%d msg=%s", result.ok, result.message.c_str());
            
            if (m_cancelFavorite) {
                LOG_INFO("toggleFavoriteSelected: cancelled, skipping result handling");
                return;
            }
            
            std::lock_guard<std::mutex> lock(m_favoriteMutex);
            m_favoriteSuccess = result.ok;
            m_favoriteMessage = result.message;
            m_favoriteInProgress = false;
            
            if (!result.ok && idx >= 0 && idx < static_cast<int>(m_entries.size())) {
                m_entries[idx].isFavorite = previousState;
            }
        });
    }, true, yesHint);

    m_sidebar->add<SidebarEntryCallback>(lang.get("ui.no"), [this]() {
        LOG_INFO("toggleFavoriteSelected: NO clicked");
        m_favoriteData.reset();
    }, true);
}

RevisionMenuScreen::~RevisionMenuScreen() {
    m_cancelDelete = true;
    if (m_deleteThread.joinable()) {
        m_deleteThread.join();
    }
    m_cancelFavorite = true;
    if (m_favoriteThread.joinable()) {
        m_favoriteThread.join();
    }
}

} // namespace ui::saves
