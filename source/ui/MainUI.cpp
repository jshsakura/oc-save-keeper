/**
 * Drop-Keep - Main UI implementation
 */

#include "ui/MainUI.hpp"
#include "utils/Language.hpp"
#include <algorithm>
#include <cstdio>

namespace ui {

MainUI::MainUI(SDL_Renderer* renderer, network::Dropbox& dropbox, core::SaveManager& saveMgr)
    : m_renderer(renderer)
    , m_dropbox(dropbox)
    , m_saveManager(saveMgr)
    , m_fontLarge(nullptr)
    , m_fontMedium(nullptr)
    , m_fontSmall(nullptr)
    , m_state(State::Main)
    , m_shouldExit(false)
    , m_selectedIndex(0)
    , m_selectedVersionIndex(0)
    , m_selectedTitle(nullptr)
    , m_syncProgress(0)
    , m_syncTotal(0)
    , m_touchX(0)
    , m_touchY(0)
    , m_touchPressed(false) {
}

MainUI::~MainUI() {
    if (m_fontLarge) TTF_CloseFont(m_fontLarge);
    if (m_fontMedium) TTF_CloseFont(m_fontMedium);
    if (m_fontSmall) TTF_CloseFont(m_fontSmall);
    TTF_Quit();
}

bool MainUI::initialize() {
    if (TTF_Init() < 0) {
        LOG_ERROR("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    
    // Initialize language system (auto-detects system language)
    utils::Language::instance().init();
    
    // Load fonts
    m_fontLarge = TTF_OpenFont("romfs:/fonts/NotoSansCJK-Bold.ttc", 36);
    m_fontMedium = TTF_OpenFont("romfs:/fonts/NotoSansCJK-Regular.ttc", 24);
    m_fontSmall = TTF_OpenFont("romfs:/fonts/NotoSansCJK-Regular.ttc", 18);
    
    // Fallback paths
    if (!m_fontLarge) {
        m_fontLarge = TTF_OpenFont("/switch/OpenCourse/oc-save-keeper/fonts/NotoSansCJK-Bold.ttc", 36);
        m_fontMedium = TTF_OpenFont("/switch/OpenCourse/oc-save-keeper/fonts/NotoSansCJK-Regular.ttc", 24);
        m_fontSmall = TTF_OpenFont("/switch/OpenCourse/oc-save-keeper/fonts/NotoSansCJK-Regular.ttc", 18);
    }
    
    // Final fallback for development
    if (!m_fontLarge) {
        m_fontLarge = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 36);
        m_fontMedium = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 24);
        m_fontSmall = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18);
    }
    
    // Scan titles
    m_saveManager.scanTitles();
    updateGameCards();
    
    // Check if need auth
    if (m_dropbox.needsAuthentication()) {
        m_state = State::Auth;
    }
    
    return true;
}

void MainUI::updateGameCards() {
    m_gameCards.clear();
    
    auto titles = m_saveManager.getTitlesWithSaves();
    
    int cols = (1280 - CARD_MARGIN) / (CARD_WIDTH + CARD_MARGIN);
    int row = 0, col = 0;
    
    for (auto* title : titles) {
        GameCard card;
        card.title = title;
        card.selected = false;
        card.synced = false; // TODO: Check sync status
        
        int x = CARD_MARGIN + col * (CARD_WIDTH + CARD_MARGIN);
        int y = HEADER_HEIGHT + CARD_MARGIN + row * (CARD_HEIGHT + CARD_MARGIN);
        card.rect = {x, y, CARD_WIDTH, CARD_HEIGHT};
        // Avoid holding a texture per title; icons are loaded on demand for the
        // focused card to keep UI memory predictable on Switch.
        card.icon = nullptr;
        
        m_gameCards.push_back(card);
        
        col++;
        if (col >= cols) {
            col = 0;
            row++;
        }
    }
}

void MainUI::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            m_shouldExit = true;
            break;
            
        case SDL_JOYBUTTONDOWN: {
            int button = event.jbutton.button;
            switch (button) {
                case 0: // A
                    if (m_state == State::Main && m_selectedIndex < (int)m_gameCards.size()) {
                        m_selectedTitle = m_gameCards[m_selectedIndex].title;
                        m_state = State::GameDetail;
                    } else if (m_state == State::VersionHistory &&
                               m_selectedVersionIndex < (int)m_versionItems.size()) {
                        restoreVersion(&m_versionItems[m_selectedVersionIndex]);
                    }
                    break;
                case 1: // B
                    if (m_state == State::GameDetail) {
                        m_state = State::Main;
                        m_selectedTitle = nullptr;
                    } else if (m_state == State::VersionHistory) {
                        m_state = State::GameDetail;
                    }
                    break;
                case 10: // Plus
                    m_shouldExit = true;
                    break;
            }
            break;
        }
            
        case SDL_FINGERDOWN:
        case SDL_MOUSEBUTTONDOWN:
            m_touchPressed = true;
            if (event.type == SDL_FINGERDOWN) {
                m_touchX = event.tfinger.x * 1280;
                m_touchY = event.tfinger.y * 720;
            } else {
                m_touchX = event.button.x;
                m_touchY = event.button.y;
            }
            handleTouch(m_touchX, m_touchY, true);
            break;
    }
}

void MainUI::handleTouch(int x, int y, bool pressed) {
    if (!pressed) return;
    
    if (m_state == State::Main) {
        for (size_t i = 0; i < m_gameCards.size(); i++) {
            SDL_Rect& r = m_gameCards[i].rect;
            if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
                m_selectedIndex = i;
                m_selectedTitle = m_gameCards[i].title;
                m_state = State::GameDetail;
                break;
            }
        }
    } else if (m_state == State::GameDetail || m_state == State::Auth || m_state == State::VersionHistory) {
        if (m_state == State::VersionHistory) {
            for (size_t i = 0; i < m_versionItems.size(); i++) {
                SDL_Rect& r = m_versionItems[i].rect;
                if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
                    m_selectedVersionIndex = i;
                    restoreVersion(&m_versionItems[i]);
                    return;
                }
            }
        }

        for (const auto& btn : m_buttons) {
            if (btn.contains(x, y)) {
                handleButtonPress(btn);
                break;
            }
        }
    }
}

void MainUI::handleButtonPress(const Button& btn) {
    // Use button ID for comparison instead of text
    std::string uploadText = LANG("detail.upload");
    std::string downloadText = LANG("detail.download");
    std::string backupText = LANG("detail.backup");
    std::string historyText = LANG("detail.history");
    std::string backText = LANG("detail.back");
    std::string connectText = LANG("auth.connect");
    std::string cancelText = LANG("auth.cancel");
    
    if (btn.text == uploadText) {
        syncToDropbox();
    } else if (btn.text == downloadText) {
        downloadFromDropbox();
    } else if (btn.text == backupText) {
        backupLocal();
    } else if (btn.text == historyText) {
        showVersionHistory();
    } else if (btn.text == backText) {
        m_state = State::Main;
        m_selectedTitle = nullptr;
    } else if (btn.text == connectText) {
        connectDropbox();
    } else if (btn.text == cancelText) {
        m_state = State::Main;
    }
}

void MainUI::update() {
    // Nothing for now
}

void MainUI::render() {
    SDL_SetRenderDrawColor(m_renderer, Color::Background.r, Color::Background.g,
                          Color::Background.b, Color::Background.a);
    SDL_RenderClear(m_renderer);
    
    switch (m_state) {
        case State::Main:
            renderHeader();
            renderGameList();
            renderFooter();
            break;
            
        case State::GameDetail:
            renderHeader();
            renderGameDetail();
            break;
            
        case State::VersionHistory:
            renderHeader();
            renderVersionHistory();
            break;
            
        case State::Auth:
            renderAuthScreen();
            break;
            
        case State::SyncAll:
            renderSyncProgress();
            break;
            
        default:
            break;
    }
}

void MainUI::renderHeader() {
    SDL_Rect headerRect = {0, 0, 1280, HEADER_HEIGHT};
    SDL_SetRenderDrawColor(m_renderer, Color::Header.r, Color::Header.g,
                          Color::Header.b, Color::Header.a);
    SDL_RenderFillRect(m_renderer, &headerRect);
    
    // Logo with localized slogan
    renderText(LANG("app.name"), 20, 12, m_fontLarge, Color::Accent);
    renderText(LANG("app.slogan"), 20, 48, m_fontSmall, Color::TextDim);
    
    // Connection status
    bool connected = m_dropbox.isAuthenticated();
    std::string status = connected ? LANG("status.connected") : LANG("status.disconnected");
    SDL_Color statusColor = connected ? Color::Synced : Color::NotSynced;
    renderText(status, 1100, 30, m_fontMedium, statusColor);
}

void MainUI::renderFooter() {
    SDL_Rect footerRect = {0, 720 - FOOTER_HEIGHT, 1280, FOOTER_HEIGHT};
    SDL_SetRenderDrawColor(m_renderer, Color::Header.r, Color::Header.g,
                          Color::Header.b, Color::Header.a);
    SDL_RenderFillRect(m_renderer, &footerRect);
    
    renderText(LANG("footer.controls"), 
              20, 720 - FOOTER_HEIGHT + 20, m_fontSmall, Color::TextDim);
    
    std::string count = std::to_string(m_gameCards.size()) + LANG("footer.game_count");
    renderText(count, 1150, 720 - FOOTER_HEIGHT + 20, m_fontSmall, Color::TextDim);
}

void MainUI::renderGameList() {
    for (size_t i = 0; i < m_gameCards.size(); i++) {
        GameCard& card = m_gameCards[i];
        card.selected = ((int)i == m_selectedIndex);
        
        if (card.rect.y + card.rect.h < HEADER_HEIGHT ||
            card.rect.y > 720 - FOOTER_HEIGHT) {
            continue;
        }
        
        renderCard(card);
    }
}

void MainUI::renderCard(const GameCard& card) {
    SDL_Color bgColor = card.selected ? Color::CardHover : Color::Card;
    SDL_SetRenderDrawColor(m_renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(m_renderer, &card.rect);
    
    if (card.selected) {
        SDL_SetRenderDrawColor(m_renderer, Color::Accent.r, Color::Accent.g,
                              Color::Accent.b, Color::Accent.a);
        SDL_RenderDrawRect(m_renderer, &card.rect);
    }
    
    // Icon placeholder
    SDL_Rect iconRect = {card.rect.x + 10, card.rect.y + 10, 180, 180};
    SDL_Texture* iconTexture = card.icon;
    if (!iconTexture && card.selected) {
        iconTexture = loadIcon(card.title->iconPath);
    }

    if (iconTexture) {
        SDL_RenderCopy(m_renderer, iconTexture, nullptr, &iconRect);
    } else {
        SDL_SetRenderDrawColor(m_renderer, 70, 70, 90, 255);
        SDL_RenderFillRect(m_renderer, &iconRect);
        renderTextCentered("🎮", iconRect.x, iconRect.y + 70, 180, m_fontLarge, Color::Text);
    }

    if (iconTexture && iconTexture != card.icon) {
        SDL_DestroyTexture(iconTexture);
    }
    
    // Name
    std::string name = card.title->name;
    if (name.length() > 18) name = name.substr(0, 15) + "...";
    renderTextCentered(name, card.rect.x, card.rect.y + 200, CARD_WIDTH, m_fontMedium, Color::Text);
    
    // Sync badge
    renderSyncBadge(card.rect.x + CARD_WIDTH - 40, card.rect.y + 10, card.synced);
}

void MainUI::renderSyncBadge(int x, int y, bool synced) {
    SDL_Color color = synced ? Color::Synced : Color::NotSynced;
    SDL_Rect badgeRect = {x, y, 30, 30};
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, 200);
    SDL_RenderFillRect(m_renderer, &badgeRect);
    
    const char* icon = synced ? "✓" : "○";
    renderTextCentered(icon, x, y + 5, 30, m_fontSmall, Color::Text);
}

void MainUI::renderGameDetail() {
    if (!m_selectedTitle) return;
    
    SDL_Rect detailRect = {100, HEADER_HEIGHT + 50, 1080, 500};
    SDL_SetRenderDrawColor(m_renderer, Color::Card.r, Color::Card.g,
                          Color::Card.b, Color::Card.a);
    SDL_RenderFillRect(m_renderer, &detailRect);
    
    int y = HEADER_HEIGHT + 80;
    
    renderText(m_selectedTitle->name, 150, y, m_fontLarge, Color::Text);
    y += 50;
    
    renderText(m_selectedTitle->publisher, 150, y, m_fontMedium, Color::TextDim);
    y += 40;
    
    char idStr[32];
    snprintf(idStr, sizeof(idStr), "Title ID: %016lX", m_selectedTitle->titleId);
    renderText(idStr, 150, y, m_fontSmall, Color::TextDim);
    y += 30;
    
    char sizeStr[64];
    snprintf(sizeStr, sizeof(sizeStr), "%s: %.2f MB", LANG("detail.save_size"), m_selectedTitle->saveSize / (1024.0 * 1024.0));
    renderText(sizeStr, 150, y, m_fontSmall, Color::TextDim);
    y += 60;

    auto versions = m_saveManager.getBackupVersions(m_selectedTitle);
    if (!versions.empty()) {
        const auto& latest = versions.front();
        renderText(std::string(LANG("detail.latest_device")) + ": " +
                   (latest.deviceLabel.empty() ? "-" : latest.deviceLabel),
                   150, y, m_fontSmall, Color::TextDim);
        y += 24;
        renderText(std::string(LANG("detail.latest_user")) + ": " +
                   (latest.userName.empty() ? "-" : latest.userName),
                   150, y, m_fontSmall, Color::TextDim);
        y += 24;
        renderText(std::string(LANG("detail.latest_source")) + ": " +
                   (latest.source.empty() ? "-" : latest.source),
                   150, y, m_fontSmall, Color::TextDim);
        y += 36;
    }
    
    std::string syncText = m_dropbox.isAuthenticated() ? 
        std::string("☁️ Dropbox: ") + LANG("status.connected") : 
        std::string("☁️ Dropbox: ") + LANG("status.disconnected");
    renderText(syncText, 150, y, m_fontMedium, m_dropbox.isAuthenticated() ? Color::Synced : Color::NotSynced);
    
    // Buttons
    m_buttons.clear();
    int btnY = HEADER_HEIGHT + 320;
    int btnW = 180;
    int btnH = 50;
    
    m_buttons.emplace_back(140, btnY, btnW, btnH, LANG("detail.upload"));
    m_buttons.emplace_back(340, btnY, btnW, btnH, LANG("detail.download"));
    m_buttons.emplace_back(540, btnY, btnW, btnH, LANG("detail.backup"));
    m_buttons.emplace_back(740, btnY, btnW, btnH, LANG("detail.history"));
    m_buttons.emplace_back(940, btnY, btnW, btnH, LANG("detail.back"));
    
    for (const auto& btn : m_buttons) {
        renderButton(btn);
    }
}

void MainUI::renderButton(const Button& btn) {
    SDL_Color bgColor = btn.hover ? Color::CardHover : Color::Accent;
    SDL_SetRenderDrawColor(m_renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(m_renderer, &btn.rect);
    
    renderTextCentered(btn.text, btn.rect.x, btn.rect.y + 15, btn.rect.w, m_fontMedium, Color::Text);
}

void MainUI::renderAuthScreen() {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(m_renderer, nullptr);
    
    SDL_Rect authRect = {140, 100, 1000, 520};
    SDL_SetRenderDrawColor(m_renderer, Color::Card.r, Color::Card.g, Color::Card.b, 255);
    SDL_RenderFillRect(m_renderer, &authRect);
    
    renderTextCentered(LANG("auth.title"), 140, 120, 1000, m_fontLarge, Color::Accent);
    
    int y = 180;
    
    renderText(LANG("auth.setup_time"), 180, y, m_fontMedium, Color::Warning);
    y += 50;
    
    renderText(LANG("auth.step1"), 180, y, m_fontMedium, Color::Text);
    y += 30;
    renderText("   dropbox.com/developers/apps", 180, y, m_fontMedium, Color::Accent);
    y += 45;
    
    renderText(LANG("auth.step2"), 180, y, m_fontMedium, Color::Text);
    y += 30;
    renderText(LANG("auth.step2_api"), 180, y, m_fontSmall, Color::TextDim);
    y += 25;
    renderText(LANG("auth.step2_access"), 180, y, m_fontSmall, Color::TextDim);
    y += 25;
    renderText(LANG("auth.step2_name"), 180, y, m_fontSmall, Color::TextDim);
    y += 45;
    
    renderText(LANG("auth.step3"), 180, y, m_fontMedium, Color::Text);
    y += 45;
    
    renderText(LANG("auth.step4"), 180, y, m_fontMedium, Color::Text);
    y += 35;
    
    // Token input box
    SDL_Rect tokenBox = {180, y, 920, 50};
    SDL_SetRenderDrawColor(m_renderer, 40, 40, 55, 255);
    SDL_RenderFillRect(m_renderer, &tokenBox);
    SDL_SetRenderDrawColor(m_renderer, Color::Accent.r, Color::Accent.g, Color::Accent.b, 255);
    SDL_RenderDrawRect(m_renderer, &tokenBox);
    
    if (m_authToken.empty()) {
        renderText(LANG("auth.token_placeholder"), 200, y + 15, m_fontMedium, Color::TextDim);
    } else {
        renderText(m_authToken, 200, y + 15, m_fontMedium, Color::Text);
    }
    y += 70;
    
    m_buttons.clear();
    m_buttons.emplace_back(300, y, 200, 50, LANG("auth.connect"));
    m_buttons.emplace_back(520, y, 200, 50, LANG("auth.cancel"));
    
    for (const auto& btn : m_buttons) {
        renderButton(btn);
    }
    
    renderTextCentered(LANG("auth.tip"), 140, y + 70, 1000, m_fontSmall, Color::TextDim);
}

void MainUI::renderVersionHistory() {
    renderText(LANG("history.title"), 100, HEADER_HEIGHT + 20, m_fontLarge, Color::Accent);
    
    if (m_selectedTitle) {
        renderText(m_selectedTitle->name, 350, HEADER_HEIGHT + 25, m_fontMedium, Color::Text);
    }
    
    int y = HEADER_HEIGHT + 80;
    
    for (size_t i = 0; i < m_versionItems.size(); i++) {
        VersionItem& item = m_versionItems[i];
        item.selected = ((int)i == m_selectedVersionIndex);
        item.rect = {100, y, 1080, 60};
        
        SDL_Color bgColor = item.selected ? Color::CardHover : Color::Card;
        SDL_SetRenderDrawColor(m_renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_RenderFillRect(m_renderer, &item.rect);
        
        if (item.selected) {
            SDL_SetRenderDrawColor(m_renderer, Color::Accent.r, Color::Accent.g, Color::Accent.b, Color::Accent.a);
            SDL_RenderDrawRect(m_renderer, &item.rect);
        }
        
        const char* icon = item.isLocal ? "💾" : "☁️";
        renderText(icon, item.rect.x + 20, item.rect.y + 18, m_fontMedium, Color::Text);
        
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&item.timestamp));
        renderText(timeStr, item.rect.x + 70, item.rect.y + 18, m_fontMedium, Color::Text);
        renderText(item.deviceLabel.empty() ? LANG("history.unknown_device") : item.deviceLabel,
                   item.rect.x + 320, item.rect.y + 10, m_fontSmall, Color::TextDim);
        renderText(item.userName.empty() ? LANG("history.unknown_user") : item.userName,
                   item.rect.x + 320, item.rect.y + 30, m_fontSmall, Color::TextDim);
        renderText(item.sourceLabel.empty() ? LANG("history.unknown_source") : item.sourceLabel,
                   item.rect.x + 760, item.rect.y + 18, m_fontSmall, Color::Accent);
        
        y += 70;
    }
    
    m_buttons.clear();
    m_buttons.emplace_back(100, 640, 200, 50, LANG("detail.back"));
    for (const auto& btn : m_buttons) {
        renderButton(btn);
    }
}

void MainUI::renderSyncProgress() {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(m_renderer, nullptr);
    
    SDL_Rect progressRect = {340, 260, 600, 200};
    SDL_SetRenderDrawColor(m_renderer, Color::Card.r, Color::Card.g, Color::Card.b, 255);
    SDL_RenderFillRect(m_renderer, &progressRect);
    
    renderTextCentered(LANG("sync.syncing"), 340, 280, 600, m_fontLarge, Color::Accent);
    renderTextCentered(m_syncStatus, 340, 380, 600, m_fontMedium, Color::Text);
}

void MainUI::renderText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color) {
    if (!font) return;
    
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    if (texture) {
        SDL_Rect dest = {x, y, surface->w, surface->h};
        SDL_RenderCopy(m_renderer, texture, nullptr, &dest);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void MainUI::renderTextCentered(const std::string& text, int x, int y, int w, TTF_Font* font, SDL_Color color) {
    if (!font) return;
    
    int textW, textH;
    TTF_SizeUTF8(font, text.c_str(), &textW, &textH);
    renderText(text, x + (w - textW) / 2, y, font, color);
}

SDL_Texture* MainUI::loadIcon(const std::string& path) {
    if (path.empty()) return nullptr;
    
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) return nullptr;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

// Actions
void MainUI::syncToDropbox() {
    if (!m_selectedTitle || !m_dropbox.isAuthenticated()) return;
    
    m_syncStatus = LANG("sync.uploading");
    
    if (!m_saveManager.createVersionedBackup(m_selectedTitle)) {
        m_syncStatus = "Local backup failed";
        return;
    }

    auto versions = m_saveManager.getBackupVersions(m_selectedTitle);
    if (versions.empty()) {
        m_syncStatus = "No backup version created";
        return;
    }

    std::string archivePath = m_saveManager.exportBackupArchive(m_selectedTitle, versions.front().path);
    if (archivePath.empty()) {
        m_syncStatus = "Archive export failed";
        return;
    }

    std::string dropboxPath = "/" + m_saveManager.getCloudPath(m_selectedTitle);
    std::string localMetaPath = versions.front().path + ".meta";
    // Upload metadata first so another device can reject stale archives without
    // paying the cost of downloading the full ZIP.
    std::string dropboxMetaPath = "/" + m_saveManager.getCloudMetadataPath(m_selectedTitle);

    if (m_dropbox.uploadFile(localMetaPath, dropboxMetaPath) &&
        m_dropbox.uploadFile(archivePath, dropboxPath)) {
        m_syncStatus = "Uploaded with metadata-aware sync";
    } else {
        m_syncStatus = "Dropbox upload failed";
    }
    
    updateGameCards();
}

void MainUI::downloadFromDropbox() {
    if (!m_selectedTitle || !m_dropbox.isAuthenticated()) return;
    
    m_syncStatus = LANG("sync.downloading");
    
    char localMeta[256];
    char localZip[256];
    std::snprintf(localMeta, sizeof(localMeta), "/switch/OpenCourse/oc-save-keeper/temp/%016llX_latest.meta",
                  static_cast<unsigned long long>(m_selectedTitle->titleId));
    std::snprintf(localZip, sizeof(localZip), "/switch/OpenCourse/oc-save-keeper/temp/%016llX_latest.zip",
                  static_cast<unsigned long long>(m_selectedTitle->titleId));

    // The low-memory path pulls the tiny metadata sidecar first and only
    // downloads the archive if the conflict policy accepts it.
    std::string dropboxMetaPath = "/" + m_saveManager.getCloudMetadataPath(m_selectedTitle);
    if (!m_dropbox.downloadFile(dropboxMetaPath, localMeta)) {
        m_syncStatus = "Cloud metadata download failed";
        return;
    }

    core::BackupMetadata incomingMeta;
    if (!m_saveManager.readMetadataFile(localMeta, incomingMeta)) {
        remove(localMeta);
        m_syncStatus = "Cloud metadata invalid";
        return;
    }

    core::SyncDecision precheck = m_saveManager.evaluateIncomingMetadata(m_selectedTitle, incomingMeta);
    remove(localMeta);
    if (!precheck.useIncoming) {
        m_syncStatus = precheck.reason;
        return;
    }

    std::string dropboxPath = "/" + m_saveManager.getCloudPath(m_selectedTitle);
    if (!m_dropbox.downloadFile(dropboxPath, localZip)) {
        m_syncStatus = "Dropbox download failed";
        return;
    }

    std::string reason;
    if (m_saveManager.importBackupArchive(m_selectedTitle, localZip, &reason, true)) {
        m_syncStatus = reason.empty() ? "Download complete" : reason;
    } else {
        m_syncStatus = reason.empty() ? "Import failed" : reason;
    }

    remove(localZip);
}

void MainUI::backupLocal() {
    if (!m_selectedTitle) return;
    m_saveManager.createVersionedBackup(m_selectedTitle);
}

void MainUI::showVersionHistory() {
    if (!m_selectedTitle) return;
    
    m_versionItems.clear();
    auto versions = m_saveManager.getBackupVersions(m_selectedTitle);
    
    for (auto& v : versions) {
        VersionItem item;
        item.name = v.name;
        item.path = v.path;
        item.deviceLabel = v.deviceLabel;
        item.userName = v.userName;
        item.sourceLabel = v.source;
        item.timestamp = v.timestamp;
        item.size = v.size;
        item.isLocal = true;
        item.selected = false;
        m_versionItems.push_back(item);
    }
    
    m_selectedVersionIndex = 0;
    m_state = State::VersionHistory;
}

void MainUI::restoreVersion(VersionItem* item) {
    if (!item || !m_selectedTitle) return;
    m_saveManager.restoreSave(m_selectedTitle, item->path);
    m_state = State::GameDetail;
}

void MainUI::syncAllGames() {
    m_state = State::SyncAll;
    m_syncProgress = 0;
    m_syncTotal = m_gameCards.size();
    
    for (auto& card : m_gameCards) {
        m_syncStatus = card.title->name + " " + LANG("sync.syncing_game");
        m_selectedTitle = card.title;
        syncToDropbox();
        m_syncProgress++;
    }
    
    m_syncStatus = LANG("sync.complete");
    m_state = State::Main;
}

void MainUI::connectDropbox() {
    if (m_authToken.empty()) {
        return;
    }

    if (m_dropbox.setAccessToken(m_authToken)) {
        m_syncStatus = "Dropbox connected";
        m_state = State::Main;
    } else {
        m_syncStatus = "Failed to save Dropbox token";
    }
}

} // namespace ui
