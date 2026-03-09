/**
 * oc-save-keeper - Main UI implementation
 */

#include "ui/MainUI.hpp"
#include "utils/Language.hpp"
#include <algorithm>
#include <cstdio>

namespace ui {

#ifdef __SWITCH__
namespace {
TTF_Font* openSharedFont(const PlFontData& font, int size) {
    SDL_RWops* rw = SDL_RWFromConstMem(font.address, static_cast<int>(font.size));
    if (!rw) {
        LOG_ERROR("SDL_RWFromConstMem failed for shared font: %s", SDL_GetError());
        return nullptr;
    }

    TTF_Font* ttfFont = TTF_OpenFontRW(rw, 1, size);
    if (!ttfFont) {
        LOG_ERROR("TTF_OpenFontRW failed for shared font: %s", TTF_GetError());
    }

    return ttfFont;
}
}
#endif

MainUI::MainUI(SDL_Renderer* renderer, network::Dropbox& dropbox, core::SaveManager& saveMgr)
    : m_renderer(renderer)
    , m_dropbox(dropbox)
    , m_saveManager(saveMgr)
    , m_fontLarge(nullptr)
    , m_fontMedium(nullptr)
    , m_fontSmall(nullptr)
#ifdef __SWITCH__
    , m_plInitialized(false)
#endif
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
    m_screenWidth = 1280;
    m_screenHeight = 720;
    m_gridCols = 1;
    m_gridRows = 1;
    m_currentPage = 0;
    m_authTokenBox = {0, 0, 0, 0};
    m_languageButton = {0, 0, 0, 0};
}

MainUI::~MainUI() {
    if (m_fontLarge) TTF_CloseFont(m_fontLarge);
    if (m_fontMedium) TTF_CloseFont(m_fontMedium);
    if (m_fontSmall) TTF_CloseFont(m_fontSmall);
#ifdef __SWITCH__
    if (m_plInitialized) {
        plExit();
    }
#endif
    TTF_Quit();
}

bool MainUI::initialize() {
    SDL_GetRendererOutputSize(m_renderer, &m_screenWidth, &m_screenHeight);

    if (TTF_Init() < 0) {
        LOG_ERROR("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    
    // Initialize language system (auto-detects system language)
    utils::Language::instance().init();

#ifdef __SWITCH__
    Result rc = plInitialize(PlServiceType_User);
    if (R_SUCCEEDED(rc)) {
        m_plInitialized = true;

        PlFontData sharedFont{};
        PlSharedFontType fontType =
            utils::Language::instance().currentLang() == "ko"
                ? PlSharedFontType_KO
                : PlSharedFontType_Standard;

        rc = plGetSharedFontByType(&sharedFont, fontType);
        if (R_FAILED(rc) && fontType != PlSharedFontType_KO) {
            rc = plGetSharedFontByType(&sharedFont, PlSharedFontType_KO);
        }

        if (R_SUCCEEDED(rc)) {
            m_fontLarge = openSharedFont(sharedFont, 44);
            m_fontMedium = openSharedFont(sharedFont, 28);
            m_fontSmall = openSharedFont(sharedFont, 22);
        } else {
            LOG_ERROR("plGetSharedFontByType failed: 0x%x", rc);
        }
    } else {
        LOG_ERROR("plInitialize failed: 0x%x", rc);
    }
#endif

    // Desktop fallback only. Real hardware should use the built-in shared font.
    if (!m_fontLarge || !m_fontMedium || !m_fontSmall) {
        if (m_fontLarge) {
            TTF_CloseFont(m_fontLarge);
            m_fontLarge = nullptr;
        }
        if (m_fontMedium) {
            TTF_CloseFont(m_fontMedium);
            m_fontMedium = nullptr;
        }
        if (m_fontSmall) {
            TTF_CloseFont(m_fontSmall);
            m_fontSmall = nullptr;
        }

        m_fontLarge = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 44);
        m_fontMedium = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 28);
        m_fontSmall = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 22);
    }

    if (!m_fontLarge || !m_fontMedium || !m_fontSmall) {
        LOG_ERROR("Font initialization failed");
        return false;
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
    
    const int contentHeight = m_screenHeight - HEADER_HEIGHT - FOOTER_HEIGHT - CARD_MARGIN * 2;
    m_gridCols = std::max(1, (m_screenWidth - CARD_MARGIN) / (CARD_WIDTH + CARD_MARGIN));
    m_gridRows = std::max(1, contentHeight / (CARD_HEIGHT + CARD_MARGIN));
    const int itemsPerPage = getItemsPerPage();
    if (itemsPerPage > 0) {
        m_currentPage = std::clamp(m_selectedIndex / itemsPerPage, 0, std::max(0, getPageCount() - 1));
    }
    
    for (size_t i = 0; i < titles.size(); i++) {
        auto* title = titles[i];
        GameCard card;
        card.title = title;
        card.selected = false;
        card.synced = false; // TODO: Check sync status

        const int indexInPage = static_cast<int>(i) % std::max(1, itemsPerPage);
        const int row = indexInPage / m_gridCols;
        const int col = indexInPage % m_gridCols;
        int x = CARD_MARGIN + col * (CARD_WIDTH + CARD_MARGIN);
        int y = HEADER_HEIGHT + CARD_MARGIN + row * (CARD_HEIGHT + CARD_MARGIN);
        card.rect = {x, y, CARD_WIDTH, CARD_HEIGHT};
        // Avoid holding a texture per title; icons are loaded on demand for the
        // focused card to keep UI memory predictable on Switch.
        card.icon = nullptr;
        
        m_gameCards.push_back(card);
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
                    if (m_state == State::Auth) {
                        m_state = State::Main;
                    } else if (m_state == State::GameDetail) {
                        m_state = State::Main;
                        m_selectedTitle = nullptr;
                    } else if (m_state == State::VersionHistory) {
                        m_state = State::GameDetail;
                    }
                    break;
                case 2: // X
                    if (m_state == State::Main) {
                        syncAllGames();
                    } else if (m_state == State::Auth) {
                        connectDropbox();
                    }
                    break;
                case 3: // Y
                    if (m_state == State::Main) {
                        toggleLanguage();
                    }
                    break;
                case 4: // L
                    if (m_state == State::Main && m_currentPage > 0) {
                        m_currentPage--;
                        m_selectedIndex = m_currentPage * getItemsPerPage();
                        clampSelection();
                    }
                    break;
                case 5: // R
                    if (m_state == State::Main && m_currentPage + 1 < getPageCount()) {
                        m_currentPage++;
                        m_selectedIndex = m_currentPage * getItemsPerPage();
                        clampSelection();
                    }
                    break;
                case 12: // D-pad up
                case 13: // D-pad down
                case 14: // D-pad left
                case 15: // D-pad right
                    if (m_state == State::Main && !m_gameCards.empty()) {
                        int nextIndex = m_selectedIndex;
                        if (button == 12) nextIndex -= m_gridCols;
                        if (button == 13) nextIndex += m_gridCols;
                        if (button == 14) nextIndex -= 1;
                        if (button == 15) nextIndex += 1;
                        nextIndex = std::clamp(nextIndex, 0, (int)m_gameCards.size() - 1);
                        m_selectedIndex = nextIndex;
                        clampSelection();
                    }
                    break;
                case 10: // Plus
                case 11: // Minus
                    m_shouldExit = true;
                    break;
            }

            if (m_state == State::Auth && button == 0) {
                openTokenKeyboard();
            }
            break;
        }
            
        case SDL_FINGERDOWN:
        case SDL_MOUSEBUTTONDOWN:
            m_touchPressed = true;
            if (event.type == SDL_FINGERDOWN) {
                m_touchX = static_cast<int>(event.tfinger.x * m_screenWidth);
                m_touchY = static_cast<int>(event.tfinger.y * m_screenHeight);
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
        if (x >= m_languageButton.x && x < m_languageButton.x + m_languageButton.w &&
            y >= m_languageButton.y && y < m_languageButton.y + m_languageButton.h) {
            toggleLanguage();
            return;
        }

        for (size_t i = 0; i < m_gameCards.size(); i++) {
            const int startIndex = getVisibleStartIndex();
            const int endIndex = std::min((int)m_gameCards.size(), startIndex + getItemsPerPage());
            if ((int)i < startIndex || (int)i >= endIndex) {
                continue;
            }
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

        if (m_state == State::Auth &&
            x >= m_authTokenBox.x && x < m_authTokenBox.x + m_authTokenBox.w &&
            y >= m_authTokenBox.y && y < m_authTokenBox.y + m_authTokenBox.h) {
            openTokenKeyboard();
            return;
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
    SDL_GetRendererOutputSize(m_renderer, &m_screenWidth, &m_screenHeight);
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
    SDL_Rect headerRect = {0, 0, m_screenWidth, HEADER_HEIGHT};
    SDL_SetRenderDrawColor(m_renderer, Color::Header.r, Color::Header.g,
                          Color::Header.b, Color::Header.a);
    SDL_RenderFillRect(m_renderer, &headerRect);
    
    renderText(LANG("app.name"), 28, 22, m_fontLarge, Color::Accent);
    
    // Connection status
    bool connected = m_dropbox.isAuthenticated();
    std::string status = connected ? LANG("status.connected") : LANG("status.disconnected");
    SDL_Color statusColor = connected ? Color::Synced : Color::NotSynced;
    const std::string slogan = LANG("app.slogan");
    m_languageButton = {m_screenWidth - 98, 24, 70, 44};

    SDL_SetRenderDrawColor(m_renderer, 58, 58, 78, 255);
    SDL_RenderFillRect(m_renderer, &m_languageButton);
    SDL_SetRenderDrawColor(m_renderer, Color::Accent.r, Color::Accent.g, Color::Accent.b, 255);
    SDL_RenderDrawRect(m_renderer, &m_languageButton);
    renderTextCentered(utils::Language::instance().currentLang() == "ko" ? "KO" : "EN",
                       m_languageButton.x, m_languageButton.y + 8, m_languageButton.w, m_fontSmall, Color::Text);

    int statusWidth = 0;
    int statusHeight = 0;
    TTF_SizeUTF8(m_fontMedium, status.c_str(), &statusWidth, &statusHeight);
    renderText(status, m_languageButton.x - statusWidth - 24, 22, m_fontMedium, statusColor);

    int sloganWidth = 0;
    int sloganHeight = 0;
    TTF_SizeUTF8(m_fontSmall, slogan.c_str(), &sloganWidth, &sloganHeight);
    renderText(slogan, m_languageButton.x - sloganWidth - 24, 58, m_fontSmall, Color::TextDim);
}

void MainUI::renderFooter() {
    SDL_Rect footerRect = {0, m_screenHeight - FOOTER_HEIGHT, m_screenWidth, FOOTER_HEIGHT};
    SDL_SetRenderDrawColor(m_renderer, Color::Header.r, Color::Header.g,
                          Color::Header.b, Color::Header.a);
    SDL_RenderFillRect(m_renderer, &footerRect);
    
    renderText(LANG("footer.controls"), 
              20, m_screenHeight - FOOTER_HEIGHT + 20, m_fontSmall, Color::TextDim);
    
    std::string count = std::to_string(m_gameCards.size()) + LANG("footer.game_count");
    int countWidth = 0;
    int countHeight = 0;
    TTF_SizeUTF8(m_fontSmall, count.c_str(), &countWidth, &countHeight);
    renderText(count, m_screenWidth - countWidth - 20, m_screenHeight - FOOTER_HEIGHT + 20, m_fontSmall, Color::TextDim);
}

void MainUI::renderGameList() {
    const int startIndex = getVisibleStartIndex();
    const int endIndex = std::min((int)m_gameCards.size(), startIndex + getItemsPerPage());
    for (int i = startIndex; i < endIndex; i++) {
        GameCard& card = m_gameCards[i];
        card.selected = ((int)i == m_selectedIndex);
        
        if (card.rect.y + card.rect.h < HEADER_HEIGHT ||
            card.rect.y > m_screenHeight - FOOTER_HEIGHT) {
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
    SDL_Rect iconRect = {card.rect.x + 20, card.rect.y + 18, 180, 180};
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
    std::string name = fitText(m_fontSmall, card.title->name, CARD_WIDTH - 24);
    renderTextCentered(name, card.rect.x + 12, card.rect.y + 222, CARD_WIDTH - 24, m_fontSmall, Color::Text);
    
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
    
    SDL_Rect detailRect = {72, HEADER_HEIGHT + 36, m_screenWidth - 144, m_screenHeight - HEADER_HEIGHT - 128};
    SDL_SetRenderDrawColor(m_renderer, Color::Card.r, Color::Card.g,
                          Color::Card.b, Color::Card.a);
    SDL_RenderFillRect(m_renderer, &detailRect);
    
    int left = detailRect.x + 48;
    int y = detailRect.y + 40;
    
    renderText(fitText(m_fontLarge, m_selectedTitle->name, detailRect.w - 96), left, y, m_fontLarge, Color::Text);
    y += 50;
    
    renderText(m_selectedTitle->publisher, left, y, m_fontMedium, Color::TextDim);
    y += 40;
    
    char idStr[32];
    snprintf(idStr, sizeof(idStr), "Title ID: %016lX", m_selectedTitle->titleId);
    renderText(idStr, left, y, m_fontSmall, Color::TextDim);
    y += 30;
    
    char sizeStr[64];
    snprintf(sizeStr, sizeof(sizeStr), "%s: %.2f MB", LANG("detail.save_size"), m_selectedTitle->saveSize / (1024.0 * 1024.0));
    renderText(sizeStr, left, y, m_fontSmall, Color::TextDim);
    y += 60;

    auto versions = m_saveManager.getBackupVersions(m_selectedTitle);
    if (!versions.empty()) {
        const auto& latest = versions.front();
        renderText(std::string(LANG("detail.latest_device")) + ": " +
                   (latest.deviceLabel.empty() ? "-" : latest.deviceLabel),
                   left, y, m_fontSmall, Color::TextDim);
        y += 24;
        renderText(std::string(LANG("detail.latest_user")) + ": " +
                   (latest.userName.empty() ? "-" : latest.userName),
                   left, y, m_fontSmall, Color::TextDim);
        y += 24;
        renderText(std::string(LANG("detail.latest_source")) + ": " +
                   (latest.source.empty() ? "-" : latest.source),
                   left, y, m_fontSmall, Color::TextDim);
        y += 36;
    }
    
    std::string syncText = m_dropbox.isAuthenticated() ? 
        std::string("☁️ Dropbox: ") + LANG("status.connected") : 
        std::string("☁️ Dropbox: ") + LANG("status.disconnected");
    renderText(syncText, left, y, m_fontMedium, m_dropbox.isAuthenticated() ? Color::Synced : Color::NotSynced);
    
    // Buttons
    m_buttons.clear();
    int btnW = std::min(280, (detailRect.w - 96) / 3);
    int btnH = 58;
    int gap = 18;
    int row1Width = btnW * 3 + gap * 2;
    int row1X = detailRect.x + (detailRect.w - row1Width) / 2;
    int row1Y = detailRect.y + detailRect.h - 150;
    int row2Width = btnW * 2 + gap;
    int row2X = detailRect.x + (detailRect.w - row2Width) / 2;
    int row2Y = row1Y + btnH + 16;
    m_buttons.emplace_back(row1X, row1Y, btnW, btnH, LANG("detail.upload"));
    m_buttons.emplace_back(row1X + btnW + gap, row1Y, btnW, btnH, LANG("detail.download"));
    m_buttons.emplace_back(row1X + (btnW + gap) * 2, row1Y, btnW, btnH, LANG("detail.backup"));
    m_buttons.emplace_back(row2X, row2Y, btnW, btnH, LANG("detail.history"));
    m_buttons.emplace_back(row2X + btnW + gap, row2Y, btnW, btnH, LANG("detail.back"));
    
    for (const auto& btn : m_buttons) {
        renderButton(btn);
    }
}

void MainUI::renderButton(const Button& btn) {
    SDL_Color bgColor = btn.hover ? Color::CardHover : Color::Accent;
    SDL_SetRenderDrawColor(m_renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(m_renderer, &btn.rect);
    
    TTF_Font* font = m_fontMedium;
    int textW = 0;
    int textH = 0;
    TTF_SizeUTF8(font, btn.text.c_str(), &textW, &textH);
    if (textW > btn.rect.w - 24) {
        font = m_fontSmall;
        TTF_SizeUTF8(font, btn.text.c_str(), &textW, &textH);
    }
    renderTextCentered(fitText(font, btn.text, btn.rect.w - 24),
                       btn.rect.x, btn.rect.y + (btn.rect.h - textH) / 2 - 2,
                       btn.rect.w, font, Color::Text);
}

void MainUI::renderAuthScreen() {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(m_renderer, nullptr);
    
    int panelWidth = std::min(m_screenWidth - 80, 1180);
    int panelHeight = std::min(m_screenHeight - 80, 620);
    SDL_Rect authRect = {(m_screenWidth - panelWidth) / 2, (m_screenHeight - panelHeight) / 2, panelWidth, panelHeight};
    SDL_SetRenderDrawColor(m_renderer, Color::Card.r, Color::Card.g, Color::Card.b, 255);
    SDL_RenderFillRect(m_renderer, &authRect);
    
    renderTextCentered(LANG("auth.title"), authRect.x, authRect.y + 20, authRect.w, m_fontLarge, Color::Accent);

    int left = authRect.x + 50;
    int y = authRect.y + 90;

    renderText(LANG("auth.setup_time"), left, y, m_fontMedium, Color::Warning);
    y += 48;
    renderText("1. dropbox.com/developers/apps", left, y, m_fontMedium, Color::Text);
    y += 42;
    renderText("2. Create App -> Dropbox API -> App folder", left, y, m_fontMedium, Color::Text);
    y += 42;
    renderText("3. Generate access token on your phone", left, y, m_fontMedium, Color::Text);
    y += 56;
    renderText("4. Press A or tap the token box to open the keyboard", left, y, m_fontMedium, Color::Accent);
    y += 50;

    m_authTokenBox = {left, y, authRect.w - 100, 76};
    SDL_SetRenderDrawColor(m_renderer, 40, 40, 55, 255);
    SDL_RenderFillRect(m_renderer, &m_authTokenBox);
    SDL_SetRenderDrawColor(m_renderer, Color::Accent.r, Color::Accent.g, Color::Accent.b, 255);
    SDL_RenderDrawRect(m_renderer, &m_authTokenBox);
    
    if (m_authToken.empty()) {
        renderText(LANG("auth.token_placeholder"), m_authTokenBox.x + 18, y + 24, m_fontMedium, Color::TextDim);
    } else {
        std::string preview = m_authToken;
        if (preview.size() > 56) {
            preview = preview.substr(0, 53) + "...";
        }
        renderText(preview, m_authTokenBox.x + 18, y + 24, m_fontMedium, Color::Text);
    }
    y += 100;
    
    m_buttons.clear();
    int buttonWidth = 240;
    int gap = 28;
    int totalWidth = buttonWidth * 2 + gap;
    int buttonX = authRect.x + (authRect.w - totalWidth) / 2;
    m_buttons.emplace_back(buttonX, y, buttonWidth, 58, LANG("auth.connect"));
    m_buttons.emplace_back(buttonX + buttonWidth + gap, y, buttonWidth, 58, LANG("auth.cancel"));
    
    for (const auto& btn : m_buttons) {
        renderButton(btn);
    }
}

void MainUI::renderVersionHistory() {
    renderText(LANG("history.title"), 100, HEADER_HEIGHT + 20, m_fontLarge, Color::Accent);
    
    if (m_selectedTitle) {
        renderText(fitText(m_fontMedium, m_selectedTitle->name, m_screenWidth - 470), 350, HEADER_HEIGHT + 25, m_fontMedium, Color::Text);
    }
    
    int y = HEADER_HEIGHT + 80;
    
    for (size_t i = 0; i < m_versionItems.size(); i++) {
        VersionItem& item = m_versionItems[i];
        item.selected = ((int)i == m_selectedVersionIndex);
        item.rect = {100, y, m_screenWidth - 200, 60};
        
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
        renderText(fitText(m_fontSmall, item.deviceLabel.empty() ? LANG("history.unknown_device") : item.deviceLabel, 360),
                   item.rect.x + 320, item.rect.y + 10, m_fontSmall, Color::TextDim);
        renderText(fitText(m_fontSmall, item.userName.empty() ? LANG("history.unknown_user") : item.userName, 360),
                   item.rect.x + 320, item.rect.y + 30, m_fontSmall, Color::TextDim);
        renderText(fitText(m_fontSmall, item.sourceLabel.empty() ? LANG("history.unknown_source") : item.sourceLabel, 220),
                   item.rect.x + 760, item.rect.y + 18, m_fontSmall, Color::Accent);
        
        y += 70;
    }
    
    m_buttons.clear();
    m_buttons.emplace_back(100, m_screenHeight - 80, 200, 50, LANG("detail.back"));
    for (const auto& btn : m_buttons) {
        renderButton(btn);
    }
}

void MainUI::renderSyncProgress() {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(m_renderer, nullptr);
    
    SDL_Rect progressRect = {m_screenWidth / 2 - 300, m_screenHeight / 2 - 100, 600, 200};
    SDL_SetRenderDrawColor(m_renderer, Color::Card.r, Color::Card.g, Color::Card.b, 255);
    SDL_RenderFillRect(m_renderer, &progressRect);
    
    renderTextCentered(LANG("sync.syncing"), progressRect.x, progressRect.y + 20, progressRect.w, m_fontLarge, Color::Accent);
    renderTextCentered(m_syncStatus, progressRect.x, progressRect.y + 120, progressRect.w, m_fontMedium, Color::Text);
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

std::string MainUI::fitText(TTF_Font* font, const std::string& text, int maxWidth) const {
    if (!font || text.empty()) {
        return text;
    }

    int textW = 0;
    int textH = 0;
    TTF_SizeUTF8(font, text.c_str(), &textW, &textH);
    if (textW <= maxWidth) {
        return text;
    }

    std::string clipped = text;
    while (!clipped.empty()) {
        clipped.pop_back();
        std::string trial = clipped + "...";
        TTF_SizeUTF8(font, trial.c_str(), &textW, &textH);
        if (textW <= maxWidth) {
            return trial;
        }
    }

    return "...";
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
    std::snprintf(localMeta, sizeof(localMeta), "/switch/oc-save-keeper/temp/%016llX_latest.meta",
                  static_cast<unsigned long long>(m_selectedTitle->titleId));
    std::snprintf(localZip, sizeof(localZip), "/switch/oc-save-keeper/temp/%016llX_latest.zip",
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

void MainUI::openTokenKeyboard() {
#ifdef __SWITCH__
    SwkbdConfig keyboard{};
    if (R_FAILED(swkbdCreate(&keyboard, 0))) {
        m_syncStatus = "Keyboard open failed";
        return;
    }

    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetHeaderText(&keyboard, "Dropbox access token");
    swkbdConfigSetSubText(&keyboard, "Paste or type the generated token");
    swkbdConfigSetGuideText(&keyboard, "Token");
    swkbdConfigSetInitialText(&keyboard, m_authToken.c_str());
    swkbdConfigSetStringLenMin(&keyboard, 1);
    swkbdConfigSetStringLenMax(&keyboard, 512);
    swkbdConfigSetOkButtonText(&keyboard, "Save");

    char buffer[513] = {0};
    Result rc = swkbdShow(&keyboard, buffer, sizeof(buffer));
    swkbdClose(&keyboard);

    if (R_SUCCEEDED(rc) && buffer[0] != '\0') {
        m_authToken = buffer;
    }
#endif
}

void MainUI::toggleLanguage() {
    const std::string nextLang = utils::Language::instance().currentLang() == "ko" ? "en" : "ko";
    utils::Language::instance().load(nextLang);
    updateGameCards();
}

void MainUI::clampSelection() {
    if (m_gameCards.empty()) {
        m_selectedIndex = 0;
        m_currentPage = 0;
        return;
    }

    m_selectedIndex = std::clamp(m_selectedIndex, 0, (int)m_gameCards.size() - 1);
    const int itemsPerPage = getItemsPerPage();
    if (itemsPerPage > 0) {
        m_currentPage = std::clamp(m_selectedIndex / itemsPerPage, 0, std::max(0, getPageCount() - 1));
    }
}

int MainUI::getItemsPerPage() const {
    return std::max(1, m_gridCols * m_gridRows);
}

int MainUI::getPageCount() const {
    if (m_gameCards.empty()) {
        return 1;
    }
    const int itemsPerPage = getItemsPerPage();
    return std::max(1, (int(m_gameCards.size()) + itemsPerPage - 1) / itemsPerPage);
}

int MainUI::getVisibleStartIndex() const {
    return m_currentPage * getItemsPerPage();
}

} // namespace ui
