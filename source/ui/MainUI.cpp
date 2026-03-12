/**
 * oc-save-keeper - Main UI implementation
 */

#include "ui/MainUI.hpp"
#include "utils/Paths.hpp"
#include "utils/Language.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ui {

namespace Theme {
    ColorPalette Light() { // Catppuccin Latte
        return {
            {239, 241, 245, 255}, // Background: Base
            {230, 233, 239, 255}, // Header: Mantle
            {255, 255, 255, 255}, // Card: White (High contrast)
            {204, 208, 218, 255}, // CardHover: Surface0
            {30, 102, 245, 255},  // Accent: Blue
            {114, 135, 253, 255}, // AccentSoft: Lavender
            {223, 142, 29, 255},  // Warning: Yellow
            {210, 15, 57, 255},   // Error: Red
            {76, 79, 105, 255},   // Text: Latte Text (Dark Grey)
            {108, 111, 133, 255}, // TextDim: Subtext0
            {64, 160, 43, 255},   // Synced: Green
            {156, 160, 176, 255}, // NotSynced: Overlay0
            {172, 176, 190, 255}, // Border: Overlay1
            {30, 102, 245, 255},  // BorderStrong: Blue
            {220, 224, 232, 255}, // Poster: Crust
            {204, 208, 218, 255}, // TitleStrip: Surface0
            {0, 0, 0, 30},        // Shadow
            {30, 102, 245, 40}    // SelectionGlow
        };
    }

    ColorPalette Dark() { // Catppuccin Mocha
        return {
            {30, 30, 46, 255},    // Background: Base (Dark Navy)
            {24, 24, 37, 255},    // Header: Mantle
            {49, 50, 68, 255},    // Card: Surface0
            {69, 71, 90, 255},    // CardHover: Surface1
            {137, 220, 235, 255}, // Accent: Sky (Light Blue)
            {180, 190, 254, 255}, // AccentSoft: Lavender
            {249, 226, 175, 255}, // Warning: Yellow
            {243, 139, 168, 255}, // Error: Red
            {205, 214, 244, 255}, // Text: Mocha Text (Off White)
            {166, 173, 200, 255}, // TextDim: Subtext0
            {166, 227, 161, 255}, // Synced: Green
            {108, 112, 126, 255}, // NotSynced: Overlay0
            {88, 91, 112, 255},   // Border: Overlay1
            {137, 220, 235, 255}, // BorderStrong: Sky
            {17, 17, 27, 255},    // Poster: Crust
            {49, 50, 68, 255},    // TitleStrip: Surface0
            {0, 0, 0, 80},        // Shadow
            {137, 220, 235, 80}   // SelectionGlow
        };
    }
}

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

namespace {

bool readLatestLocalMetadata(core::SaveManager& saveManager, core::TitleInfo* title, core::BackupMetadata& outMeta) {
    auto versions = saveManager.getBackupVersions(title);
    for (const auto& version : versions) {
        if (saveManager.readBackupMetadata(version.path, outMeta)) {
            return true;
        }
    }
    return false;
}

std::string fileNameFromPath(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string directoryFromPath(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "" : path.substr(0, slash);
}

bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string formatStorageSize(int64_t bytes) {
    if (bytes <= 0) {
        return "0 B";
    }

    char buffer[32];
    if (bytes < 1024) {
        std::snprintf(buffer, sizeof(buffer), "%lld B", static_cast<long long>(bytes));
        return buffer;
    }

    const double kb = bytes / 1024.0;
    if (kb < 1024.0) {
        std::snprintf(buffer, sizeof(buffer), kb < 10.0 ? "%.1f KB" : "%.0f KB", kb);
        return buffer;
    }

    const double mb = kb / 1024.0;
    if (mb < 1024.0) {
        std::snprintf(buffer, sizeof(buffer), mb < 10.0 ? "%.1f MB" : "%.0f MB", mb);
        return buffer;
    }

    const double gb = mb / 1024.0;
    std::snprintf(buffer, sizeof(buffer), gb < 10.0 ? "%.1f GB" : "%.0f GB", gb);
    return buffer;
}

std::string closeLabel() {
    return utils::Language::instance().currentLang() == "ko" ? "닫기" : "Close";
}

} // namespace

MainUI::MainUI(SDL_Renderer* renderer, network::Dropbox& dropbox, core::SaveManager& saveMgr)
    : m_renderer(renderer)
    , m_dropbox(dropbox)
    , m_saveManager(saveMgr)
    , m_fontLarge(nullptr)
    , m_fontMedium(nullptr)
    , m_fontSmall(nullptr)
#ifdef __SWITCH__
    , m_plInitialized(false)
    , m_lastFocusState(AppletFocusState_InFocus)
#endif
    , m_state(State::Main)
    , m_shouldExit(false)
    , m_selectedIndex(0)
    , m_pressedIndex(-1)
    , m_pressedButtonIndex(-1)
    , m_pressedUserIndex(-1)
    , m_pressedVersionIndex(-1)
    , m_selectedVersionIndex(0)
    , m_versionScrollIndex(0)
    , m_selectedUserIndex(0)
    , m_userPickerScrollIndex(0)
    , m_selectedButtonIndex(0)
    , m_confirmDeleteVersion(false)
    , m_selectedTitle(nullptr)
    , m_authUrl("")
    , m_authSessionStarted(false)
    , m_syncProgress(0)
    , m_syncTotal(0)
    , m_syncStatus("")
    , m_touchX(0)
    , m_touchY(0)
    , m_touchPressed(false) {
    m_screenWidth = 1280;
    m_screenHeight = 720;
    m_gridCols = 1;
    m_gridRows = 1;
    m_scrollRow = 0;
    m_scrollOffset = 0.0f;
    m_authTokenBox = {0, 0, 0, 0};
    m_languageButton = {0, 0, 0, 0};
    m_refreshButton = {0, 0, 0, 0};
    m_statusButton = {0, 0, 0, 0};
    m_userButton = {0, 0, 0, 0};
    m_recentBackupRow = {0, 0, 0, 0};
    initParticles();
}

MainUI::~MainUI() {
    clearIconCache();
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

void MainUI::initParticles() {
    m_particles.clear();
    for (int i = 0; i < 40; i++) {
        m_particles.push_back({
            static_cast<float>(rand() % 1280),
            static_cast<float>(rand() % 720),
            static_cast<float>(2 + rand() % 4),
            0.15f + (rand() % 10) * 0.04f,
            static_cast<float>(15 + rand() % 45),
            static_cast<float>(rand() % 360)
        });
    }
}

void MainUI::clearIconCache() {
    for (auto& pair : m_iconCache) {
        if (pair.second) SDL_DestroyTexture(pair.second);
    }
    m_iconCache.clear();
}

void MainUI::renderParticles() {
    // Blocky particles removed to fix the "small squares" issue reported by user.
}

bool MainUI::initialize() {
    SDL_GetRendererOutputSize(m_renderer, &m_screenWidth, &m_screenHeight);

    // Detect system theme
    m_colors = Theme::Dark(); // Default
#ifdef __SWITCH__
    ColorSetId colorSetId = ColorSetId_Dark;
    if (R_SUCCEEDED(setsysInitialize())) {
        setsysGetColorSetId(&colorSetId);
        setsysExit();
    }
    if (colorSetId == ColorSetId_Light) {
        m_colors = Theme::Light();
    }
#endif

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
            m_fontLarge = openSharedFont(sharedFont, 48);
            m_fontMedium = openSharedFont(sharedFont, 32);
            m_fontSmall = openSharedFont(sharedFont, 26);
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

        m_fontLarge = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 48);
        m_fontMedium = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 32);
        m_fontSmall = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 26);
    }

    if (!m_fontLarge || !m_fontMedium || !m_fontSmall) {
        LOG_ERROR("Font initialization failed");
        return false;
    }
    
    // Scan titles
    m_saveManager.scanTitles();
    updateGameCards();
    refreshSyncStates();
    
    return true;
}

void MainUI::updateGameCards() {
    m_gameCards.clear();
    
    auto titles = m_saveManager.getTitlesWithSaves();
    
    const int outerMargin = 14;
    const int gridGap = 12;
    const int contentHeight = m_screenHeight - HEADER_HEIGHT - FOOTER_HEIGHT - outerMargin * 2;
    m_gridCols = 6;
    m_gridRows = 2;
    const int cardWidth = std::max(150, (m_screenWidth - outerMargin * 2 - gridGap * (m_gridCols - 1)) / m_gridCols);
    const int preferredCardHeight = cardWidth + 64;
    const int maxCardHeight = std::max(220, (contentHeight - gridGap * (m_gridRows - 1)) / m_gridRows);
    const int cardHeight = std::min(preferredCardHeight, maxCardHeight);
    const int firstVisibleIndex = getVisibleStartIndex();

    for (size_t i = 0; i < titles.size(); i++) {
        auto* title = titles[i];
        GameCard card;
        card.title = title;
        card.selected = false;
        card.synced = false;
        card.syncState = GameCard::SyncState::Unknown;
        card.syncLabel.clear();

        const int indexInView = static_cast<int>(i) - firstVisibleIndex;
        const int row = indexInView / m_gridCols;
        const int col = indexInView % m_gridCols;
        int x = outerMargin + col * (cardWidth + gridGap);
        int y = HEADER_HEIGHT + outerMargin + row * (cardHeight + gridGap);
        card.rect = {x, y, cardWidth, cardHeight};
        card.icon = nullptr;
        
        m_gameCards.push_back(card);
    }
}

void MainUI::refreshSyncStates(bool autoUpload) {
    if (m_dropbox.isAuthenticated()) {
        m_state = State::SyncAll;
        m_syncProgress = 0;
        m_syncTotal = std::max(1, static_cast<int>(m_gameCards.size()));
        m_syncStatus = "Refreshing cloud status";
    }

    for (auto& card : m_gameCards) {
        if (m_dropbox.isAuthenticated()) {
            m_syncStatus = card.title->name + " " + LANG("sync.downloading");
        }
        card.synced = false;
        card.syncState = GameCard::SyncState::Unknown;
        card.syncLabel.clear();

        core::BackupMetadata localMeta;
        const bool hasLocalMeta = readLatestLocalMetadata(m_saveManager, card.title, localMeta);
        if (!m_dropbox.isAuthenticated()) {
            card.syncState = hasLocalMeta ? GameCard::SyncState::LocalOnly : GameCard::SyncState::Disconnected;
            card.syncLabel = hasLocalMeta ? LANG("card.state.local_only") : LANG("card.state.disconnected");
            continue;
        }

        char localMetaPath[256];
        std::snprintf(localMetaPath, sizeof(localMetaPath), "%s/%016llX_card.meta", utils::paths::TEMP,
                      static_cast<unsigned long long>(card.title->titleId));
        std::string dropboxMetaPath = "/" + m_saveManager.getCloudMetadataPath(card.title);
        if (!m_dropbox.downloadFile(dropboxMetaPath, localMetaPath)) {
            card.syncState = hasLocalMeta ? GameCard::SyncState::NeedsUpload : GameCard::SyncState::LocalOnly;
            card.syncLabel = hasLocalMeta ? LANG("card.state.needs_upload") : LANG("card.state.local_only");
            continue;
        }

        core::BackupMetadata remoteMeta;
        const bool hasRemoteMeta = m_saveManager.readMetadataFile(localMetaPath, remoteMeta);
        remove(localMetaPath);
        if (!hasRemoteMeta) {
            card.syncState = hasLocalMeta ? GameCard::SyncState::NeedsUpload : GameCard::SyncState::LocalOnly;
            card.syncLabel = hasLocalMeta ? LANG("card.state.needs_upload") : LANG("card.state.local_only");
            continue;
        }

        if (!hasLocalMeta) {
            card.syncState = GameCard::SyncState::CloudNewer;
            card.syncLabel = LANG("card.state.cloud_newer");
            if (m_dropbox.isAuthenticated()) {
                m_syncProgress++;
            }
            continue;
        }

        core::SyncDecision incoming = m_saveManager.decideSync(&localMeta, remoteMeta);
        core::SyncDecision outgoing = m_saveManager.decideSync(&remoteMeta, localMeta);
        if (outgoing.useIncoming && !incoming.useIncoming) {
            card.syncState = GameCard::SyncState::NeedsUpload;
            card.syncLabel = LANG("card.state.needs_upload");
            if (autoUpload) {
                m_selectedTitle = card.title;
                syncToDropbox();
            }
        } else if (incoming.useIncoming && !outgoing.useIncoming) {
            card.syncState = GameCard::SyncState::CloudNewer;
            card.syncLabel = LANG("card.state.cloud_newer");
        } else {
            card.syncState = GameCard::SyncState::UpToDate;
            card.syncLabel = LANG("card.state.up_to_date");
            card.synced = true;
        }
        if (m_dropbox.isAuthenticated()) {
            m_syncProgress++;
        }
    }

    if (m_dropbox.isAuthenticated()) {
        m_syncStatus = LANG("sync.complete");
        m_state = State::Main;
    }
}

void MainUI::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            m_shouldExit = true;
            break;
            
        case SDL_JOYBUTTONDOWN: {
#ifdef __SWITCH__
            break;
#endif
            int button = event.jbutton.button;
            switch (button) {
                case 0: // A
                    if (m_state == State::Main && m_selectedIndex < (int)m_gameCards.size()) {
                        m_selectedTitle = m_gameCards[m_selectedIndex].title;
                        m_selectedButtonIndex = 0;
                        m_state = State::GameDetail;
                    } else if (m_state == State::Auth &&
                               m_selectedButtonIndex >= 0 &&
                               m_selectedButtonIndex < (int)m_buttons.size()) {
                        handleButtonPress(m_buttons[m_selectedButtonIndex]);
                    } else if (m_state == State::GameDetail &&
                               m_selectedButtonIndex >= 0 &&
                               m_selectedButtonIndex < (int)m_buttons.size()) {
                        handleButtonPress(m_buttons[m_selectedButtonIndex]);
                    } else if (m_state == State::CloudPicker &&
                               m_selectedButtonIndex >= 0 &&
                               m_selectedButtonIndex < (int)m_buttons.size()) {
                        handleButtonPress(m_buttons[m_selectedButtonIndex]);
                    } else if (m_state == State::CloudPicker &&
                               m_selectedVersionIndex < (int)m_versionItems.size()) {
                        downloadCloudItem(&m_versionItems[m_selectedVersionIndex]);
                    } else if (m_state == State::VersionHistory &&
                               m_selectedButtonIndex >= 0 &&
                               m_selectedButtonIndex < (int)m_buttons.size()) {
                        handleButtonPress(m_buttons[m_selectedButtonIndex]);
                    }
                    break;
                case 1: // B
                    if (m_state == State::Auth) {
                        m_dropbox.cancelPendingAuthorization();
                        m_authSessionStarted = false;
                        m_authUrl.clear();
                        m_authToken.clear();
                        m_state = State::Main;
                    } else if (m_state == State::VersionHistory && m_confirmDeleteVersion) {
                        m_confirmDeleteVersion = false;
                        m_selectedButtonIndex = 0;
                    } else if (m_state == State::CloudPicker) {
                        m_state = State::GameDetail;
                    } else if (m_state == State::GameDetail) {
                        m_state = State::Main;
                        m_selectedTitle = nullptr;
                    } else if (m_state == State::VersionHistory) {
                        m_state = State::GameDetail;
                    }
                    break;
                case 2: // X
                    if (m_state == State::Main) {
                        m_saveManager.scanTitles();
                        updateGameCards();
                        refreshSyncStates();
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
                    if (m_state == State::Main && m_scrollRow > 0) {
                        m_scrollRow = std::max(0, m_scrollRow - m_gridRows);
                        m_selectedIndex = std::max(0, m_scrollRow * m_gridCols);
                        clampSelection();
                    }
                    break;
                case 5: // R
                    if (m_state == State::Main) {
                        const int totalRows = std::max(1, ((int)m_gameCards.size() + m_gridCols - 1) / m_gridCols);
                        const int maxScrollRow = std::max(0, totalRows - m_gridRows);
                        if (m_scrollRow < maxScrollRow) {
                            m_scrollRow = std::min(maxScrollRow, m_scrollRow + m_gridRows);
                            m_selectedIndex = std::min((int)m_gameCards.size() - 1, m_scrollRow * m_gridCols);
                        }
                        clampSelection();
                    }
                    break;
                case 12: // D-pad up
                case 13: // D-pad down
                case 14: // D-pad left
                case 15: // D-pad right
                    if ((m_state == State::VersionHistory || m_state == State::CloudPicker)) {
                        if ((button == 14 || button == 15) && !m_buttons.empty()) {
                            if (button == 15) {
                                m_selectedButtonIndex = 0;
                            } else {
                                m_selectedButtonIndex = -1;
                            }
                            break;
                        }
                        if (m_selectedButtonIndex >= 0) {
                            break;
                        }
                        if (!m_versionItems.empty()) {
                            int nextIndex = m_selectedVersionIndex;
                            if (button == 12) nextIndex -= 1;
                            if (button == 13) nextIndex += 1;
                            nextIndex = std::clamp(nextIndex, 0, (int)m_versionItems.size() - 1);
                            m_selectedVersionIndex = nextIndex;
                        }
                    } else if (m_state == State::Auth && !m_buttons.empty()) {
                        int nextIndex = m_selectedButtonIndex < 0 ? 0 : m_selectedButtonIndex;
                        if (button == 14) nextIndex -= 1;
                        if (button == 15) nextIndex += 1;
                        nextIndex = std::clamp(nextIndex, 0, (int)m_buttons.size() - 1);
                        m_selectedButtonIndex = nextIndex;
                    } else if (m_state == State::GameDetail && !m_buttons.empty()) {
                        int nextIndex = m_selectedButtonIndex;
                        if (button == 12) nextIndex -= 1;
                        if (button == 13) nextIndex += 1;
                        nextIndex = std::clamp(nextIndex, 0, (int)m_buttons.size() - 1);
                        m_selectedButtonIndex = nextIndex;
                    } else if (m_state == State::Main && !m_gameCards.empty()) {
                        const int totalItems = (int)m_gameCards.size();
                        const int currentRow = m_selectedIndex / m_gridCols;
                        const int currentCol = m_selectedIndex % m_gridCols;
                        int nextIndex = m_selectedIndex;

                        if (button == 12) { // up
                            const int candidate = (currentRow - 1) * m_gridCols + currentCol;
                            if (currentRow > 0 && candidate >= 0) {
                                nextIndex = candidate;
                            }
                        } else if (button == 13) { // down
                            const int candidate = (currentRow + 1) * m_gridCols + currentCol;
                            if (candidate < totalItems) {
                                nextIndex = candidate;
                            }
                        } else if (button == 14) { // left
                            if (currentCol > 0) {
                                nextIndex = m_selectedIndex - 1;
                            }
                        } else if (button == 15) { // right
                            const int candidate = m_selectedIndex + 1;
                            if ((currentCol + 1) < m_gridCols && candidate < totalItems) {
                                nextIndex = candidate;
                            }
                        }

                        m_selectedIndex = nextIndex;
                        clampSelection();
                    }
                    break;
                case 10: // Plus
                case 11: // Minus
                    m_shouldExit = true;
                    break;
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

        case SDL_FINGERUP:
        case SDL_MOUSEBUTTONUP:
            m_touchPressed = false;
            if (event.type == SDL_FINGERUP) {
                m_touchX = static_cast<int>(event.tfinger.x * m_screenWidth);
                m_touchY = static_cast<int>(event.tfinger.y * m_screenHeight);
            } else {
                m_touchX = event.button.x;
                m_touchY = event.button.y;
            }
            handleTouch(m_touchX, m_touchY, false);
            break;
    }
}

#ifdef __SWITCH__
void MainUI::handlePadButtons(u64 keysDown) {
    if (keysDown & HidNpadButton_A) {
        if (m_state == State::Main && m_selectedIndex < (int)m_gameCards.size()) {
            m_selectedTitle = m_gameCards[m_selectedIndex].title;
            m_selectedButtonIndex = 0;
            m_state = State::GameDetail;
        } else if (m_state == State::Auth &&
                   m_selectedButtonIndex >= 0 &&
                   m_selectedButtonIndex < (int)m_buttons.size()) {
            handleButtonPress(m_buttons[m_selectedButtonIndex]);
        } else if (m_state == State::UserPicker &&
                   m_selectedUserIndex >= 0 &&
                   m_selectedUserIndex < (int)m_userChips.size()) {
            selectUser(m_selectedUserIndex);
        } else if (m_state == State::GameDetail &&
                   m_selectedButtonIndex >= 0 &&
                   m_selectedButtonIndex < (int)m_buttons.size()) {
            handleButtonPress(m_buttons[m_selectedButtonIndex]);
        } else if (m_state == State::CloudPicker &&
                   m_selectedButtonIndex >= 0 &&
                   m_selectedButtonIndex < (int)m_buttons.size()) {
            handleButtonPress(m_buttons[m_selectedButtonIndex]);
        } else if (m_state == State::CloudPicker &&
                   m_selectedVersionIndex < (int)m_versionItems.size()) {
            downloadCloudItem(&m_versionItems[m_selectedVersionIndex]);
        } else if (m_state == State::VersionHistory &&
                   m_selectedButtonIndex >= 0 &&
                   m_selectedButtonIndex < (int)m_buttons.size()) {
            handleButtonPress(m_buttons[m_selectedButtonIndex]);
        }
    }

    if (keysDown & HidNpadButton_B) {
        if (m_state == State::Auth) {
            m_dropbox.cancelPendingAuthorization();
            m_authSessionStarted = false;
            m_authUrl.clear();
            m_authToken.clear();
            m_state = State::Main;
        } else if (m_state == State::VersionHistory && m_confirmDeleteVersion) {
            m_confirmDeleteVersion = false;
            m_selectedButtonIndex = 0;
        } else if (m_state == State::UserPicker) {
            m_state = State::Main;
        } else if (m_state == State::CloudPicker) {
            m_state = State::GameDetail;
        } else if (m_state == State::GameDetail) {
            m_state = State::Main;
            m_selectedTitle = nullptr;
        } else if (m_state == State::VersionHistory) {
            m_state = State::GameDetail;
        }
    }

    if (keysDown & HidNpadButton_X) {
        if (m_state == State::Main) {
            m_saveManager.scanTitles();
            updateGameCards();
            refreshSyncStates();
        } else if (m_state == State::Auth) {
            connectDropbox();
        }
    }

    if (keysDown & HidNpadButton_Y) {
        if (m_state == State::Main) {
            toggleLanguage();
        } else if (m_state == State::Auth) {
            openTokenKeyboard();
        }
    }

    if (keysDown & HidNpadButton_L) {
        if (m_state == State::Main && m_scrollRow > 0) {
            m_scrollRow = std::max(0, m_scrollRow - m_gridRows);
            m_selectedIndex = std::max(0, m_scrollRow * m_gridCols);
            clampSelection();
        }
    }

    if (keysDown & HidNpadButton_R) {
        if (m_state == State::Main) {
            const int totalRows = std::max(1, ((int)m_gameCards.size() + m_gridCols - 1) / m_gridCols);
            const int maxScrollRow = std::max(0, totalRows - m_gridRows);
            if (m_scrollRow < maxScrollRow) {
                m_scrollRow = std::min(maxScrollRow, m_scrollRow + m_gridRows);
                m_selectedIndex = std::min((int)m_gameCards.size() - 1, m_scrollRow * m_gridCols);
            }
            clampSelection();
        }
    }

    const bool up = (keysDown & HidNpadButton_Up) != 0;
    const bool down = (keysDown & HidNpadButton_Down) != 0;
    const bool left = (keysDown & HidNpadButton_Left) != 0;
    const bool right = (keysDown & HidNpadButton_Right) != 0;

    if (m_state == State::Auth && !m_buttons.empty()) {
        int nextIndex = m_selectedButtonIndex < 0 ? 0 : m_selectedButtonIndex;
        if (left) nextIndex -= 1;
        if (right) nextIndex += 1;
        m_selectedButtonIndex = std::clamp(nextIndex, 0, (int)m_buttons.size() - 1);
        return;
    }

    if ((m_state == State::VersionHistory || m_state == State::CloudPicker)) {
        if (right && !m_buttons.empty()) {
            m_selectedButtonIndex = 0;
            return;
        }
        if (left) {
            m_selectedButtonIndex = -1;
            return;
        }
        if (m_selectedButtonIndex >= 0) {
            return;
        }
        if (!m_versionItems.empty()) {
            int nextIndex = m_selectedVersionIndex;
            if (up) nextIndex -= 1;
            if (down) nextIndex += 1;
            m_selectedVersionIndex = std::clamp(nextIndex, 0, (int)m_versionItems.size() - 1);
        }
        return;
    }

    if (m_state == State::UserPicker && !m_userChips.empty()) {
        int nextIndex = m_selectedUserIndex;
        if (up) nextIndex -= 1;
        if (down) nextIndex += 1;
        m_selectedUserIndex = std::clamp(nextIndex, 0, (int)m_userChips.size() - 1);
        m_userPickerScrollIndex = std::min(m_userPickerScrollIndex, m_selectedUserIndex);
        return;
    }

    if (m_state == State::GameDetail && !m_buttons.empty()) {
        int nextIndex = m_selectedButtonIndex;
        if (up) nextIndex -= 1;
        if (down) nextIndex += 1;
        m_selectedButtonIndex = std::clamp(nextIndex, 0, (int)m_buttons.size() - 1);
        return;
    }

    if (m_state != State::Main || m_gameCards.empty()) {
        return;
    }

    const int totalItems = (int)m_gameCards.size();
    const int currentRow = m_selectedIndex / m_gridCols;
    const int currentCol = m_selectedIndex % m_gridCols;
    int nextIndex = m_selectedIndex;

    if (up) {
        const int candidate = (currentRow - 1) * m_gridCols + currentCol;
        if (currentRow > 0 && candidate >= 0) {
            nextIndex = candidate;
        }
    } else if (down) {
        const int candidate = (currentRow + 1) * m_gridCols + currentCol;
        if (candidate < totalItems) {
            nextIndex = candidate;
        }
    } else if (left) {
        if (currentCol > 0) {
            nextIndex = m_selectedIndex - 1;
        }
    } else if (right) {
        const int candidate = m_selectedIndex + 1;
        if ((currentCol + 1) < m_gridCols && candidate < totalItems) {
            nextIndex = candidate;
        }
    }

    m_selectedIndex = nextIndex;
    clampSelection();
}
#endif

void MainUI::handleTouch(int x, int y, bool pressed) {
    if (pressed) { // ON DOWN
        if (m_state == State::Main) {
            if (x >= m_statusButton.x && x < m_statusButton.x + m_statusButton.w &&
                y >= m_statusButton.y && y < m_statusButton.y + m_statusButton.h) {
                m_pressedButtonIndex = 999; // Unique ID for status
                return;
            }
            if (x >= m_languageButton.x && x < m_languageButton.x + m_languageButton.w &&
                y >= m_languageButton.y && y < m_languageButton.y + m_languageButton.h) {
                m_pressedButtonIndex = 998; // Unique ID for lang
                return;
            }
            if (x >= m_userButton.x && x < m_userButton.x + m_userButton.w &&
                y >= m_userButton.y && y < m_userButton.y + m_userButton.h) {
                m_pressedButtonIndex = 997; // Unique ID for user
                return;
            }
            if (x >= m_refreshButton.x && x < m_refreshButton.x + m_refreshButton.w &&
                y >= m_refreshButton.y && y < m_refreshButton.y + m_refreshButton.h) {
                m_pressedButtonIndex = 996; // Unique ID for refresh
                return;
            }

            for (size_t i = 0; i < m_gameCards.size(); i++) {
                const int startIndex = getVisibleStartIndex();
                const int endIndex = std::min((int)m_gameCards.size(), startIndex + getItemsPerPage());
                if ((int)i < startIndex || (int)i >= endIndex) continue;
                SDL_Rect& r = m_gameCards[i].rect;
                if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
                    m_pressedIndex = i;
                    break;
                }
            }
        } else if (m_state == State::UserPicker) {
            for (size_t i = 0; i < m_userChips.size(); ++i) {
                if (x >= m_userChips[i].rect.x && x < m_userChips[i].rect.x + m_userChips[i].rect.w &&
                    y >= m_userChips[i].rect.y && y < m_userChips[i].rect.y + m_userChips[i].rect.h) {
                    m_pressedUserIndex = i;
                    break;
                }
            }
        } else if (m_state == State::VersionHistory || m_state == State::CloudPicker) {
            for (size_t i = 0; i < m_versionItems.size(); i++) {
                if (x >= m_versionItems[i].rect.x && x < m_versionItems[i].rect.x + m_versionItems[i].rect.w &&
                    y >= m_versionItems[i].rect.y && y < m_versionItems[i].rect.y + m_versionItems[i].rect.h) {
                    m_pressedVersionIndex = i;
                    break;
                }
            }
        }
        
        for (size_t i = 0; i < m_buttons.size(); ++i) {
            if (m_buttons[i].contains(x, y)) {
                m_pressedButtonIndex = i;
                break;
            }
        }
    } else { // ON UP (EXECUTE ACTION)
        if (m_state == State::Main) {
            if (m_pressedButtonIndex == 999 && x >= m_statusButton.x && x < m_statusButton.x + m_statusButton.w && y >= m_statusButton.y && y < m_statusButton.y + m_statusButton.h) {
                if (!m_dropbox.isAuthenticated()) m_state = State::Auth;
            } else if (m_pressedButtonIndex == 998 && x >= m_languageButton.x && x < m_languageButton.x + m_languageButton.w && y >= m_languageButton.y && y < m_languageButton.y + m_languageButton.h) {
                toggleLanguage();
            } else if (m_pressedButtonIndex == 997 && x >= m_userButton.x && x < m_userButton.x + m_userButton.w && y >= m_userButton.y && y < m_userButton.y + m_userButton.h) {
                showUserPicker();
            } else if (m_pressedIndex != -1) {
                SDL_Rect& r = m_gameCards[m_pressedIndex].rect;
                if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
                    m_selectedIndex = m_pressedIndex;
                    m_selectedTitle = m_gameCards[m_pressedIndex].title;
                    m_state = State::GameDetail;
                }
            }
        } else if (m_state == State::GameDetail) {
            if (m_recentBackupRow.w > 0 &&
                x >= m_recentBackupRow.x && x < m_recentBackupRow.x + m_recentBackupRow.w &&
                y >= m_recentBackupRow.y && y < m_recentBackupRow.y + m_recentBackupRow.h) {
                showVersionHistory();
            }
        } else if (m_state == State::Auth) {
            if (x >= m_authTokenBox.x && x < m_authTokenBox.x + m_authTokenBox.w &&
                y >= m_authTokenBox.y && y < m_authTokenBox.y + m_authTokenBox.h) {
                openTokenKeyboard();
            }
        } else if (m_state == State::UserPicker && m_pressedUserIndex != -1) {
            if (x >= m_userChips[m_pressedUserIndex].rect.x && x < m_userChips[m_pressedUserIndex].rect.x + m_userChips[m_pressedUserIndex].rect.w &&
                y >= m_userChips[m_pressedUserIndex].rect.y && y < m_userChips[m_pressedUserIndex].rect.y + m_userChips[m_pressedUserIndex].rect.h) {
                selectUser(m_pressedUserIndex);
            }
        } else if ((m_state == State::VersionHistory || m_state == State::CloudPicker) && m_pressedVersionIndex != -1) {
            if (x >= m_versionItems[m_pressedVersionIndex].rect.x && x < m_versionItems[m_pressedVersionIndex].rect.x + m_versionItems[m_pressedVersionIndex].rect.w &&
                y >= m_versionItems[m_pressedVersionIndex].rect.y && y < m_versionItems[m_pressedVersionIndex].rect.y + m_versionItems[m_pressedVersionIndex].rect.h) {
                m_selectedVersionIndex = m_pressedVersionIndex;
            }
        }

        if (m_pressedButtonIndex != -1 && m_pressedButtonIndex < (int)m_buttons.size()) {
            if (m_buttons[m_pressedButtonIndex].contains(x, y)) {
                handleButtonPress(m_buttons[m_pressedButtonIndex]);
            }
        }

        m_pressedIndex = -1;
        m_pressedButtonIndex = -1;
        m_pressedUserIndex = -1;
        m_pressedVersionIndex = -1;
    }
}

void MainUI::handleButtonPress(const Button& btn) {
    // Use button ID for comparison instead of text
    std::string uploadText = LANG("detail.upload");
    std::string downloadText = LANG("detail.download");
    std::string backupText = LANG("detail.backup");
    std::string historyText = LANG("detail.history");
    std::string backText = LANG("detail.back");
    std::string closeText = closeLabel();
    const bool isKo = utils::Language::instance().currentLang() == "ko";
    std::string createLinkText = isKo ? "링크 생성" : "Get Link";
    std::string openKeyboardText = LANG("auth.open_keyboard");
    std::string connectText = isKo ? "코드 연결" : "Connect Code";
    std::string cancelText = LANG("auth.cancel");
    std::string deleteText = isKo ? "삭제" : "Delete";
    std::string restoreText = isKo ? "복원" : "Restore";
    std::string confirmDeleteText = isKo ? "삭제 확인" : "Confirm Delete";
    std::string cancelDeleteText = isKo ? "취소" : "Cancel";
    
    if (btn.text == uploadText) {
        if (m_dropbox.isAuthenticated()) {
            syncToDropbox();
        } else {
            m_state = State::Auth;
        }
    } else if (btn.text == downloadText) {
        if (m_dropbox.isAuthenticated()) {
            showCloudPicker();
        } else {
            m_state = State::Auth;
        }
    } else if (btn.text == backupText) {
        backupLocal();
    } else if (btn.text == historyText) {
        showVersionHistory();
    } else if (btn.text == backText) {
        m_state = State::Main;
        m_selectedTitle = nullptr;
    } else if (btn.text == "X" || btn.text == closeText) {
        if (m_state == State::VersionHistory) {
            m_state = State::GameDetail;
        } else if (m_state == State::CloudPicker) {
            m_state = State::GameDetail;
        } else {
            m_state = State::Main;
            m_selectedTitle = nullptr;
        }
    } else if (btn.text == createLinkText || btn.text == connectText) {
        connectDropbox();
    } else if (btn.text == openKeyboardText) {
        openTokenKeyboard();
    } else if (btn.text == deleteText) {
        m_confirmDeleteVersion = true;
        m_selectedButtonIndex = 0;
    } else if (btn.text == confirmDeleteText) {
        if (m_selectedVersionIndex >= 0 && m_selectedVersionIndex < static_cast<int>(m_versionItems.size())) {
            deleteVersion(&m_versionItems[m_selectedVersionIndex]);
        }
    } else if (btn.text == cancelDeleteText && m_state == State::VersionHistory) {
        m_confirmDeleteVersion = false;
        m_selectedButtonIndex = 0;
    } else if (btn.text == restoreText) {
        if (m_selectedVersionIndex >= 0 && m_selectedVersionIndex < static_cast<int>(m_versionItems.size())) {
            VersionItem& item = m_versionItems[m_selectedVersionIndex];
            if (item.isLocal) {
                restoreVersion(&item);
            } else {
                downloadCloudItem(&item);
            }
        }
    } else if (btn.text == cancelText) {
        m_dropbox.cancelPendingAuthorization();
        m_authSessionStarted = false;
        m_authUrl.clear();
        m_authToken.clear();
        m_state = State::Main;
    }
}

void MainUI::update() {
    SDL_GetRendererOutputSize(m_renderer, &m_screenWidth, &m_screenHeight);

#ifdef __SWITCH__
    const AppletFocusState focusState = appletGetFocusState();
    if (focusState == AppletFocusState_InFocus && m_lastFocusState != AppletFocusState_InFocus) {
        m_saveManager.scanTitles();
        updateGameCards();
        refreshSyncStates();
    }
    m_lastFocusState = focusState;
#endif
    
    // Static selection state (Scaling removed for stability)
    m_selectionScale = 1.0f;
    m_selectionAlpha = 255.0f;
    
    // Background Drifting Animation
    m_bgTimer += 0.015f;
    
    // Fast Overlay Fade Logic
    const bool showOverlay = (m_state != State::Main && m_state != State::SyncAll);
    if (showOverlay) {
        if (m_overlayAlpha < 255.0f) m_overlayAlpha += 40.0f;
    } else {
        if (m_overlayAlpha > 0.0f) m_overlayAlpha -= 40.0f;
    }
    m_overlayAlpha = std::clamp(m_overlayAlpha, 0.0f, 255.0f);

    // Toast logic
    if (m_toast.active) {
        if (m_toast.timer > 0) {
            m_toast.timer--;
            if (m_toast.alpha < 255.0f) m_toast.alpha += 30.0f;
        } else {
            m_toast.alpha -= 20.0f;
            if (m_toast.alpha <= 0.0f) {
                m_toast.alpha = 0.0f;
                m_toast.active = false;
            }
        }
    }
    
    // Smooth scrolling (Faster)
    float targetOffset = static_cast<float>(m_scrollRow);
    m_scrollOffset += (targetOffset - m_scrollOffset) * 0.35f;
}

void MainUI::renderIcon(SDL_Texture* texture, const SDL_Rect& rect, bool selected) {
    if (!texture) return;
    
    if (selected) {
        SDL_Rect glow = {rect.x - 4, rect.y - 4, rect.w + 8, rect.h + 8};
        SDL_SetRenderDrawColor(m_renderer, m_colors.Accent.r, m_colors.Accent.g, m_colors.Accent.b, 80);
        SDL_RenderFillRect(m_renderer, &glow);
    }
    
    SDL_RenderCopy(m_renderer, texture, nullptr, &rect);
}

void MainUI::render() {
    renderAuraBackground();
    renderParticles();
    
    switch (m_state) {
        case State::Main:
            renderHeader();
            renderGameList();
            renderFooter();
            break;
            
        case State::GameDetail:
            renderHeader();
            renderGameList();
            renderFooter();
            renderGameDetail();
            break;
            
        case State::VersionHistory:
        case State::CloudPicker:
            renderHeader();
            renderGameList();
            renderFooter();
            renderVersionHistory();
            break;

        case State::UserPicker:
            renderHeader();
            renderGameList();
            renderFooter();
            renderUserPicker();
            break;
            
        case State::Auth:
            renderHeader();
            renderGameList();
            renderFooter();
            renderAuthScreen();
            break;
            
        case State::SyncAll:
            renderSyncProgress();
            break;
            
        default:
            break;
    }

    renderToast();
}

void MainUI::renderHeader() {
    // Background bar
    SDL_Rect headerBar = {0, 0, m_screenWidth, HEADER_HEIGHT};
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, m_colors.Header.r, m_colors.Header.g, m_colors.Header.b, 230);
    SDL_RenderFillRect(m_renderer, &headerBar);
    
    // Bottom divider
    SDL_SetRenderDrawColor(m_renderer, m_colors.Border.r, m_colors.Border.g, m_colors.Border.b, 255);
    SDL_RenderDrawLine(m_renderer, 0, HEADER_HEIGHT - 1, m_screenWidth, HEADER_HEIGHT - 1);

    // Standard Base Y for Header alignment (Centered)
    const int centerY = HEADER_HEIGHT / 2;
    const int logoH = 40;
    const int baseY = centerY - (logoH / 2);

    // Floating Logo and Title
    SDL_Rect logoBar = {32, baseY, 6, logoH};
    renderFilledRoundedRect(logoBar, 3, m_colors.Accent);
    
    int titleW, titleH;
    TTF_SizeUTF8(m_fontLarge, LANG("app.name"), &titleW, &titleH);
    renderText(LANG("app.name"), 54, centerY - (titleH / 2), m_fontLarge, m_colors.Text);

    bool connected = m_dropbox.isAuthenticated();
    std::string status = connected ? LANG("status.connected") : LANG("status.disconnected");
    SDL_Color statusColor = connected ? m_colors.Synced : m_colors.TextDim;
    
    // Buttons - Adapt for longer language names
    const int btnH = 40;
    int statusWidth = 0, statusHeight = 0;
    TTF_SizeUTF8(m_fontSmall, status.c_str(), &statusWidth, &statusHeight);

    const int langBtnW = 140;
    m_languageButton = {m_screenWidth - langBtnW - 32, centerY - (btnH / 2), langBtnW, btnH};
    m_statusButton = {m_languageButton.x - statusWidth - 52, centerY - (btnH / 2), statusWidth + 36, btnH};
    
    // Status Badge
    SDL_Rect statusRect = m_statusButton;
    if (m_pressedButtonIndex == 999) {
        statusRect.x += statusRect.w * 0.025f; statusRect.y += statusRect.h * 0.025f;
        statusRect.w *= 0.95f; statusRect.h *= 0.95f;
    }
    renderGlassPanel(statusRect, btnH / 2, m_colors.Card, true);
    renderTextCentered(status, statusRect.x, statusRect.y + (statusRect.h - statusHeight) / 2, statusRect.w, m_fontSmall, statusColor);

    // Language Toggle
    SDL_Rect langRect = m_languageButton;
    if (m_pressedButtonIndex == 998) {
        langRect.x += langRect.w * 0.025f; langRect.y += langRect.h * 0.025f;
        langRect.w *= 0.95f; langRect.h *= 0.95f;
    }
    renderGlassPanel(langRect, btnH / 2, m_colors.Card, true);
    renderTextCentered(utils::Language::instance().currentLang() == "ko" ? "한국어" : "English",
                       langRect.x, langRect.y + (langRect.h - statusHeight) / 2, langRect.w, m_fontSmall, m_colors.Accent);
}

void MainUI::renderFooter() {
    const int footerY = m_screenHeight - FOOTER_HEIGHT;

    // Background bar (More transparent for glass look)
    SDL_Rect footerBar = {0, footerY, m_screenWidth, FOOTER_HEIGHT};
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, m_colors.Header.r, m_colors.Header.g, m_colors.Header.b, 180);
    SDL_RenderFillRect(m_renderer, &footerBar);
    
    // Top divider (Subtle)
    SDL_SetRenderDrawColor(m_renderer, m_colors.Border.r, m_colors.Border.g, m_colors.Border.b, 80);
    SDL_RenderDrawLine(m_renderer, 0, footerY, m_screenWidth, footerY);

    const int centerY = footerY + (FOOTER_HEIGHT / 2);

    auto renderControl = [&](const std::string& icon, const std::string& label, SDL_Color iconColor, int& x) {
        int iw, ih, lw, lh;
        TTF_SizeUTF8(m_fontSmall, icon.c_str(), &iw, &ih);
        TTF_SizeUTF8(m_fontSmall, label.c_str(), &lw, &lh);
        
        // Button Icon Background (Circle)
        int circleSize = 32;
        SDL_Rect circle = {x, centerY - (circleSize / 2), circleSize, circleSize};
        renderFilledRoundedRect(circle, circleSize / 2, iconColor);
        
        // Icon character (A, B, X, Y) - White
        renderTextCentered(icon, circle.x, circle.y + (circle.h - ih) / 2 - 1, circle.w, m_fontSmall, SDL_Color{255, 255, 255, 255});
        
        // Label Text
        renderText(label, circle.x + circle.w + 10, centerY - (lh / 2), m_fontSmall, m_colors.Text);
        
        x += circle.w + lw + 40;
    };

    int currentX = 32;
    bool isKo = utils::Language::instance().currentLang() == "ko";
    
    // Switch Button Colors
    SDL_Color colorA = {0, 150, 255, 255}; // Blue
    SDL_Color colorB = {255, 60, 60, 255};  // Red
    SDL_Color colorX = {30, 220, 30, 255};  // Green
    SDL_Color colorY = {255, 220, 0, 255};  // Yellow

    renderControl("A", isKo ? "선택" : "Select", colorA, currentX);
    renderControl("B", isKo ? "뒤로" : "Back", colorB, currentX);
    renderControl("X", isKo ? "새로고침" : "Refresh", colorX, currentX);
    renderControl("Y", isKo ? "한국어" : "English", colorY, currentX);

    core::UserInfo* selectedUser = m_saveManager.getSelectedUser();
    const std::string userName = selectedUser ? selectedUser->name : std::string("User");
    int userTextW = 0, userTextH = 0;
    TTF_SizeUTF8(m_fontSmall, userName.c_str(), &userTextW, &userTextH);
    const int chipW = std::min(360, std::max(220, userTextW + 116));
    m_userButton = {m_screenWidth - chipW - 32, centerY - (52 / 2), chipW, 52};
    
    SDL_Rect userRect = m_userButton;
    if (m_pressedButtonIndex == 997) {
        userRect.x += userRect.w * 0.025f; userRect.y += userRect.h * 0.025f;
        userRect.w *= 0.95f; userRect.h *= 0.95f;
    }
    renderGlassPanel(userRect, 26, m_colors.Card, true);

    SDL_Rect avatarRect{userRect.x + 10, userRect.y + 8, 36, 36};
    bool renderedAvatar = false;
    if (selectedUser && !selectedUser->iconPath.empty()) {
        if (SDL_Texture* avatarTexture = loadIcon(selectedUser->iconPath)) {
            SDL_RenderCopy(m_renderer, avatarTexture, nullptr, &avatarRect);
            renderedAvatar = true;
        }
    }
    if (!renderedAvatar) {
        renderFilledRoundedRect(avatarRect, 18, m_colors.Border);
    }
    renderRoundedRect(avatarRect, 18, m_colors.BorderStrong);

    // User text with strict fitting to prevent overflow into boundaries
    renderText(fitText(m_fontSmall, userName, userRect.w - 88), userRect.x + 58, userRect.y + (userRect.h - userTextH)/2, m_fontSmall, m_colors.Text);
}

void MainUI::renderGameList() {
    // Smooth scrolling calculation: Offset cards by pixels instead of snapping to rows
    const int outerMargin = 32;
    const int gridGap = 24;
    const int cardWidth = (m_screenWidth - outerMargin * 2 - gridGap * (m_gridCols - 1)) / m_gridCols;
    const int cardHeight = cardWidth + 70; // Professional aspect ratio

    // We render a slightly larger range to ensure smooth entry/exit of cards during scroll
    const int totalItems = static_cast<int>(m_gameCards.size());
    
    // Pixel-perfect offset based on smooth scroll timer
    const int scrollPixelY = static_cast<int>(m_scrollOffset * (cardHeight + gridGap));

    for (int i = 0; i < totalItems; i++) {
        GameCard& card = m_gameCards[i];
        card.selected = ((int)i == m_selectedIndex);
        
        const int row = i / m_gridCols;
        const int col = i % m_gridCols;
        
        card.rect.x = outerMargin + col * (cardWidth + gridGap);
        card.rect.y = HEADER_HEIGHT + outerMargin + row * (cardHeight + gridGap) - scrollPixelY;
        card.rect.w = cardWidth;
        card.rect.h = cardHeight;

        // Culling: Only render if visible on screen to save performance
        if (card.rect.y + card.rect.h > HEADER_HEIGHT && card.rect.y < m_screenHeight - FOOTER_HEIGHT) {
            renderCard(card);
        }
    }

    renderScrollBar();
}

void MainUI::renderScrollBar() {
    const int totalRows = std::max(1, ((int)m_gameCards.size() + m_gridCols - 1) / m_gridCols);
    if (totalRows <= m_gridRows) return;

    const int barWidth = 4; // Slimmer
    const int trackHeight = m_screenHeight - 180; // Shorter track for airy feel
    const int handleHeight = std::max(40, (trackHeight * m_gridRows) / totalRows);
    
    const int maxScroll = totalRows - m_gridRows;
    const int scrollY = (m_scrollRow * (trackHeight - handleHeight)) / maxScroll;

    SDL_Rect trackRect = {m_screenWidth - 12, (m_screenHeight - trackHeight) / 2, barWidth, trackHeight};
    SDL_Rect handleRect = {trackRect.x, trackRect.y + scrollY, barWidth, handleHeight};

    renderFilledRoundedRect(trackRect, 2, SDL_Color{255, 255, 255, 20});
    renderFilledRoundedRect(handleRect, 2, m_colors.Accent);
}

void MainUI::renderSoftShadow(const SDL_Rect& rect, int radius, int spread, SDL_Color color, int offsetY) {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < spread; ++i) {
        Uint8 alpha = static_cast<Uint8>(color.a * (1.0f - static_cast<float>(i) / spread));
        // Exponential falloff for softer web-like shadow
        alpha = static_cast<Uint8>(alpha * alpha / 255.0f); 
        SDL_Rect shadowRect = {
            rect.x - i, 
            rect.y - i + offsetY, 
            rect.w + (i * 2), 
            rect.h + (i * 2)
        };
        renderRoundedRect(shadowRect, radius + i, SDL_Color{color.r, color.g, color.b, alpha});
    }
}

void MainUI::renderSelectionGlow(const SDL_Rect& rect) {
    Uint8 alphaBase = static_cast<Uint8>(m_selectionAlpha);
    renderSoftShadow(rect, 20, 24, SDL_Color{m_colors.Accent.r, m_colors.Accent.g, m_colors.Accent.b, static_cast<Uint8>(alphaBase / 2)}, 0);
}

void MainUI::renderRoundedRect(const SDL_Rect& rect, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // Draw lines
    SDL_RenderDrawLine(m_renderer, rect.x + radius, rect.y, rect.x + rect.w - radius, rect.y);
    SDL_RenderDrawLine(m_renderer, rect.x + radius, rect.y + rect.h - 1, rect.x + rect.w - radius, rect.y + rect.h - 1);
    SDL_RenderDrawLine(m_renderer, rect.x, rect.y + radius, rect.x, rect.y + rect.h - radius);
    SDL_RenderDrawLine(m_renderer, rect.x + rect.w - 1, rect.y + radius, rect.x + rect.w - 1, rect.y + rect.h - radius);

    // Draw corners
    auto drawCorner = [&](int cx, int cy, int startAngle, int endAngle) {
        for (int i = 0; i <= 90; i++) {
            const double angle = (startAngle + i) * std::acos(-1.0) / 180.0;
            int x = static_cast<int>(cx + radius * cos(angle));
            int y = static_cast<int>(cy + radius * sin(angle));
            SDL_RenderDrawPoint(m_renderer, x, y);
        }
    };

    drawCorner(rect.x + rect.w - radius - 1, rect.y + radius, 270, 360);
    drawCorner(rect.x + radius, rect.y + radius, 180, 270);
    drawCorner(rect.x + radius, rect.y + rect.h - radius - 1, 90, 180);
    drawCorner(rect.x + rect.w - radius - 1, rect.y + rect.h - radius - 1, 0, 90);
}

void MainUI::renderFilledRoundedRect(const SDL_Rect& rect, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // Body rectangles
    SDL_Rect midBody = {rect.x + radius, rect.y, rect.w - radius * 2, rect.h};
    SDL_Rect leftBody = {rect.x, rect.y + radius, radius, rect.h - radius * 2};
    SDL_Rect rightBody = {rect.x + rect.w - radius, rect.y + radius, radius, rect.h - radius * 2};
    
    SDL_RenderFillRect(m_renderer, &midBody);
    SDL_RenderFillRect(m_renderer, &leftBody);
    SDL_RenderFillRect(m_renderer, &rightBody);

    // Corners
    auto fillCorner = [&](int cx, int cy, int startAngle) {
        for (int x = 0; x <= radius; x++) {
            int y = static_cast<int>(sqrt(radius * radius - x * x));
            if (startAngle == 270) // Top right
                SDL_RenderDrawLine(m_renderer, cx + x, cy, cx + x, cy - y);
            else if (startAngle == 180) // Top left
                SDL_RenderDrawLine(m_renderer, cx - x, cy, cx - x, cy - y);
            else if (startAngle == 90) // Bottom left
                SDL_RenderDrawLine(m_renderer, cx - x, cy, cx - x, cy + y);
            else if (startAngle == 0) // Bottom right
                SDL_RenderDrawLine(m_renderer, cx + x, cy, cx + x, cy + y);
        }
    };

    fillCorner(rect.x + rect.w - radius - 1, rect.y + radius, 270);
    fillCorner(rect.x + radius, rect.y + radius, 180);
    fillCorner(rect.x + radius, rect.y + rect.h - radius - 1, 90);
    fillCorner(rect.x + rect.w - radius - 1, rect.y + rect.h - radius - 1, 0);
}

void MainUI::renderVerticalGradient(const SDL_Rect& rect, SDL_Color top, SDL_Color bottom) {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < rect.h; ++y) {
        float t = static_cast<float>(y) / (rect.h > 1 ? rect.h - 1 : 1);
        Uint8 r = static_cast<Uint8>(top.r + t * (bottom.r - top.r));
        Uint8 g = static_cast<Uint8>(top.g + t * (bottom.g - top.g));
        Uint8 b = static_cast<Uint8>(top.b + t * (bottom.b - top.b));
        Uint8 a = static_cast<Uint8>(top.a + t * (bottom.a - top.a));
        SDL_SetRenderDrawColor(m_renderer, r, g, b, a);
        // Fill 1px tall rectangle to ensure zero gaps on high-res displays
        SDL_Rect line = {rect.x, rect.y + y, rect.w, 1};
        SDL_RenderFillRect(m_renderer, &line);
    }
}

void MainUI::renderAuraBackground() {
    // Force refresh renderer dimensions to catch Docked/Handheld changes
    SDL_GetRendererOutputSize(m_renderer, &m_screenWidth, &m_screenHeight);

    // Solid Base with 2px overscan for safety
    SDL_Rect screenRect = {-2, -2, m_screenWidth + 4, m_screenHeight + 4};
    SDL_SetRenderDrawColor(m_renderer, m_colors.Background.r, m_colors.Background.g, m_colors.Background.b, 255);
    SDL_RenderFillRect(m_renderer, &screenRect);

    // Full area gradient
    renderVerticalGradient(screenRect, m_colors.Background, m_colors.Header);

    // Dynamic accent light
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    int lightSize = std::max(m_screenWidth, m_screenHeight) / 2;
    SDL_Rect accentLight = {-lightSize / 2, -lightSize / 2, lightSize, lightSize};
    SDL_SetRenderDrawColor(m_renderer, m_colors.Accent.r, m_colors.Accent.g, m_colors.Accent.b, 12);
    SDL_RenderFillRect(m_renderer, &accentLight);
}

void MainUI::renderGlassPanel(const SDL_Rect& rect, int radius, SDL_Color baseColor, bool hasRimLight) {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    
    // Respect alpha for glass effect
    SDL_Color solidColor = baseColor;

    // Soft Shadow
    renderSoftShadow(rect, radius, 12, SDL_Color{0, 0, 0, 80}, 6);

    // Solid Base
    renderFilledRoundedRect(rect, radius, solidColor);

    // Border (Mocha Overlay1)
    SDL_Color borderColor = m_colors.Border;
    if (hasRimLight) borderColor.a = 200;
    renderRoundedRect(rect, radius, borderColor);
}

void MainUI::renderCard(const GameCard& card, float unused_scale) {
    // Determine if this specific card is being pressed
    bool isPressed = false;
    for (size_t i = 0; i < m_gameCards.size(); i++) {
        if (&m_gameCards[i] == &card && (int)i == m_pressedIndex) {
            isPressed = true;
            break;
        }
    }

    SDL_Rect rect = card.rect;
    if (isPressed) {
        // Visual compression on press
        rect.x += rect.w * 0.025f;
        rect.y += rect.h * 0.025f;
        rect.w *= 0.95f;
        rect.h *= 0.95f;
    }
    
    const int borderRadius = 16;
    
    // Opaque Surface (Mocha/Latte)
    SDL_Color cardColor = card.selected ? m_colors.CardHover : m_colors.Card;
    if (isPressed) cardColor.a = 220; // Slight darkening
    
    renderGlassPanel(rect, borderRadius, cardColor, false);

    if (card.selected && !isPressed) {
        // High-Visibility Accent Border (4px)
        SDL_SetRenderDrawColor(m_renderer, m_colors.Accent.r, m_colors.Accent.g, m_colors.Accent.b, 255);
        for(int i=1; i<=4; i++) {
            SDL_Rect b = {rect.x - i, rect.y - i, rect.w + i*2, rect.h + i*2};
            renderRoundedRect(b, borderRadius + i, m_colors.Accent);
        }
    }

    const int padding = 12;
    const int iconSize = rect.w - padding * 2;
    SDL_Rect iconRect = {rect.x + padding, rect.y + padding, iconSize, iconSize};
    
    // Guaranteed Icon Rendering
    SDL_Texture* iconTexture = loadIcon(card.title->iconPath);
    if (iconTexture) {
        // Dim the icon if it's local only to distinguish from cloud games
        if (card.syncState == GameCard::SyncState::LocalOnly || card.syncState == GameCard::SyncState::Disconnected) {
            SDL_SetTextureColorMod(iconTexture, 100, 100, 100); // Darker / Grayscale-ish
        } else {
            SDL_SetTextureColorMod(iconTexture, 255, 255, 255); // Reset to normal
        }
        SDL_RenderCopy(m_renderer, iconTexture, nullptr, &iconRect);
    } else {
        renderFilledRoundedRect(iconRect, 12, m_colors.Poster);
        renderTextCentered("?", iconRect.x, iconRect.y + (iconRect.h / 2) - 20, iconRect.w, m_fontLarge, m_colors.TextDim);
    }
    renderRoundedRect(iconRect, 12, m_colors.Border);

    int textY = iconRect.y + iconRect.h + 12;
    // Strict use of Theme Text color
    renderText(fitText(m_fontSmall, card.title->name, rect.w - padding * 2), rect.x + padding, textY, m_fontSmall, m_colors.Text);

    if (!card.syncLabel.empty()) {
        SDL_Color statusColor = card.selected ? m_colors.Accent : m_colors.TextDim;
        renderText(fitText(m_fontSmall, card.syncLabel, rect.w - padding * 2), rect.x + padding, textY + 26, m_fontSmall, statusColor);
    }

    if (card.synced) {
        renderSyncBadge(rect.x + rect.w - 28, rect.y + 12, true);
    }
}

void MainUI::renderSyncBadge(int x, int y, bool synced) {
    SDL_Rect badgeRect = {x, y, 20, 20};
    SDL_Color color = synced ? m_colors.Synced : m_colors.NotSynced;
    
    renderFilledRoundedRect(badgeRect, 10, color);
    
    // Draw a small white checkmark for synced
    if (synced) {
        SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(m_renderer, x + 5, y + 10, x + 9, y + 14);
        SDL_RenderDrawLine(m_renderer, x + 9, y + 14, x + 15, y + 6);
    }
}

void MainUI::renderGameDetail() {
    if (!m_selectedTitle || m_overlayAlpha <= 0.0f) return;
    
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, static_cast<Uint8>(m_overlayAlpha * 0.55f)); // Dynamic overlay
    SDL_RenderFillRect(m_renderer, nullptr);

    const int sideWidth = std::min(800, std::max(720, (m_screenWidth * 13) / 20));
    SDL_Rect sideRect{m_screenWidth - sideWidth - 20, 40, sideWidth, m_screenHeight - 80};
    
    // Opaque Surface
    renderGlassPanel(sideRect, 32, m_colors.Card, false);

    SDL_Rect posterRect{sideRect.x + 40, sideRect.y + 40, 160, 160};
    if (SDL_Texture* posterTexture = loadIcon(m_selectedTitle->iconPath)) {
        SDL_RenderCopy(m_renderer, posterTexture, nullptr, &posterRect);
    }
    renderRoundedRect(posterRect, 20, m_colors.Border);

    const int infoX = posterRect.x + posterRect.w + 32;
    // CRITICAL: Max width restricted to ensure title never hits the right side
    const int infoW = sideRect.x + sideRect.w - infoX - 60; 
    renderText(fitText(m_fontLarge, m_selectedTitle->name, infoW), infoX, sideRect.y + 60, m_fontLarge, m_colors.Text);
    renderText(fitText(m_fontMedium, m_selectedTitle->publisher, infoW), infoX, sideRect.y + 110, m_fontMedium, m_colors.TextDim);

    // Tags
    auto drawTag = [&](const std::string& text, int x, int y, SDL_Color color) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(m_fontSmall, text.c_str(), &tw, &th);
        SDL_Rect tag{x, y, tw + 32, 40};
        renderFilledRoundedRect(tag, 20, SDL_Color{color.r, color.g, color.b, 40});
        renderRoundedRect(tag, 20, color);
        renderText(text, tag.x + 16, tag.y + (tag.h - th)/2, m_fontSmall, color);
        return tag.w;
    };

    int tagX = infoX;
    tagX += drawTag(formatStorageSize(m_selectedTitle->saveSize), tagX, sideRect.y + 160, m_colors.Accent) + 16;
    drawTag("STABLE", tagX, sideRect.y + 160, m_colors.Synced);

    core::UserInfo* selectedUser = m_saveManager.getSelectedUser();
    const auto versions = m_saveManager.getBackupVersions(m_selectedTitle);
    const core::BackupVersion* latestVersion = versions.empty() ? nullptr : &versions.front();
    const std::time_t now = std::time(nullptr);
    const int recentBackupCount = static_cast<int>(std::count_if(versions.begin(), versions.end(), [&](const core::BackupVersion& version) {
        return version.timestamp != 0 && (now - version.timestamp) <= (7 * 24 * 60 * 60);
    }));
    const std::string backupCount = recentBackupCount <= 0 ? LANG("detail.no_backup") : std::to_string(recentBackupCount) + " " + LANG("detail.versions");
    const std::string latestDeviceLabel = latestVersion
        ? (!latestVersion->deviceLabel.empty()
            ? latestVersion->deviceLabel
            : (latestVersion->source == "cloud"
                ? std::string(LANG("detail.cloud_inside"))
                : m_saveManager.getDeviceLabel()))
        : m_saveManager.getDeviceLabel();
    const std::string latestUserLabel = latestVersion
        ? (!latestVersion->userName.empty()
            ? latestVersion->userName
            : (selectedUser ? selectedUser->name : std::string(LANG("detail.current_user_backup"))))
        : (selectedUser ? selectedUser->name : std::string(LANG("detail.current_user_backup")));

    // Standard Row rendering with guaranteed contrast
    auto drawRow = [&](const std::string& label, const std::string& value, int y, bool clickable = false) {
        SDL_Rect row{sideRect.x + 40, y, sideRect.w - 80, 90};
        // Opaque background for the row to ensure text visibility
        renderFilledRoundedRect(row, 16, m_colors.CardHover);
        renderRoundedRect(row, 16, clickable ? m_colors.Accent : m_colors.Border);
        
        renderText(label, row.x + 24, row.y + 16, m_fontSmall, m_colors.TextDim);
        renderText(fitText(m_fontMedium, value, row.w - 48), row.x + 24, row.y + 46, m_fontMedium, m_colors.Text);
        return row;
    };

    int rowY = sideRect.y + 230;
    m_recentBackupRow = drawRow(LANG("detail.recent_backup"), backupCount, rowY, !versions.empty()); rowY += 105;
    drawRow(LANG("detail.latest_device"), latestDeviceLabel, rowY); rowY += 105;
    drawRow(LANG("detail.latest_user"), latestUserLabel, rowY);

    m_buttons.clear();
    const int btnGap = 24;
    const int btnW = (sideRect.w - 80 - btnGap) / 2;
    const int btnH = 62;
    const int startY = sideRect.y + sideRect.h - (m_selectedTitle->hasSave ? 226 : 154);

    if (m_selectedTitle->hasSave) {
        m_buttons.emplace_back(sideRect.x + 40, startY, btnW, btnH, LANG("detail.upload"));
        m_buttons.emplace_back(sideRect.x + 40 + btnW + btnGap, startY, btnW, btnH, LANG("detail.download"));
        m_buttons.emplace_back(sideRect.x + 40, startY + btnH + 16, btnW, btnH, LANG("detail.backup"));
        m_buttons.emplace_back(sideRect.x + 40 + btnW + btnGap, startY + btnH + 16, btnW, btnH, LANG("detail.history"));
    } else {
        m_buttons.emplace_back(sideRect.x + 40, startY, btnW, btnH, LANG("detail.download"));
        m_buttons.emplace_back(sideRect.x + 40 + btnW + btnGap, startY, btnW, btnH, LANG("detail.history"));
    }

    for (size_t i = 0; i < m_buttons.size(); ++i) {
        auto& btn = m_buttons[i];
        btn.hover = static_cast<int>(i) == m_selectedButtonIndex;
        renderButton(btn);
    }
}

void MainUI::renderButton(const Button& btn) {
    const bool isKo = utils::Language::instance().currentLang() == "ko";
    const bool isClose = btn.text == "X" || btn.text == closeLabel();
    const bool isPrimary = btn.text == LANG("detail.upload") ||
                           btn.text == LANG("detail.download") ||
                           btn.text == LANG("auth.connect");
    const bool isDanger = btn.text == (isKo ? "삭제" : "Delete") ||
                          btn.text == (isKo ? "삭제 확인" : "Confirm Delete");
    
    // Check if this specific button is being pressed
    bool isPressed = false;
    for (size_t i = 0; i < m_buttons.size(); ++i) {
        if (&m_buttons[i] == &btn && (int)i == m_pressedButtonIndex) {
            isPressed = true;
            break;
        }
    }

    SDL_Rect renderRect = btn.rect;
    if (isPressed) {
        // Shrink effect
        renderRect.x += renderRect.w * 0.025f;
        renderRect.y += renderRect.h * 0.025f;
        renderRect.w *= 0.95f;
        renderRect.h *= 0.95f;
    }

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    
    SDL_Color bgColor;
    SDL_Color textColor = m_colors.Text;

    if (isClose) {
        bgColor = btn.hover ? m_colors.Accent : m_colors.Card;
        if (isPressed) bgColor.a = 200; // Darken
        textColor = btn.hover ? m_colors.Background : m_colors.Text;
    } else if (isDanger) {
        bgColor = btn.hover ? SDL_Color{104, 48, 56, 255} : SDL_Color{72, 43, 49, 255};
        if (isPressed) {
            bgColor = SDL_Color{58, 36, 41, 255};
        }
        textColor = SDL_Color{250, 246, 247, 255};
    } else if (isPrimary) {
        bgColor = btn.hover ? SDL_Color{68, 111, 171, 255} : SDL_Color{49, 92, 151, 255};
        if (isPressed) {
            bgColor = SDL_Color{41, 77, 127, 255};
        }
        textColor = SDL_Color{248, 250, 255, 255};
    } else {
        bgColor = btn.hover ? SDL_Color{58, 67, 81, 255} : SDL_Color{39, 46, 58, 255};
        if (isPressed) {
            bgColor = SDL_Color{31, 38, 48, 255};
        }
        textColor = SDL_Color{245, 248, 252, 255};
    }

    const int radius = renderRect.h / 2;
    renderGlassPanel(renderRect, radius, bgColor, false);
    
    if (btn.hover && !isPressed) {
        renderRoundedRect(renderRect, radius, m_colors.BorderStrong);
    }

    TTF_Font* font = m_fontMedium;
    int textW, textH;
    TTF_SizeUTF8(font, btn.text.c_str(), &textW, &textH);
    if (textW > renderRect.w - 24) {
        font = m_fontSmall;
        TTF_SizeUTF8(font, btn.text.c_str(), &textW, &textH);
    }
    
    renderTextCentered(fitText(font, btn.text, renderRect.w - 16),
                       renderRect.x, renderRect.y + (renderRect.h - textH) / 2,
                       renderRect.w, font, textColor);
}

void MainUI::renderAuthScreen() {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, m_colors.Background.r, m_colors.Background.g, m_colors.Background.b, 82);
    SDL_RenderFillRect(m_renderer, nullptr);

    const bool isKo = utils::Language::instance().currentLang() == "ko";
    const std::string createLinkText = isKo ? "링크 생성" : "Get Link";
    const std::string connectCodeText = isKo ? "코드 연결" : "Connect Code";
    const std::string nextAction = m_authSessionStarted
        ? (isKo ? "지금 할 일: 휴대폰에서 링크를 연 뒤, 완료 화면의 코드나 URL을 아래 칸에 붙여넣고 [코드 연결]을 누르세요."
                : "Next: Open the link on your phone, paste the returned code or URL below, then press [Connect Code].")
        : (isKo ? "지금 할 일: 먼저 아래의 [링크 생성]을 눌러 Dropbox 인증 링크를 만드세요."
                : "Next: Press [Get Link] below to generate your Dropbox authorization link.");
    const std::string authStep1 = isKo ? "1. [링크 생성]을 눌러 인증 링크를 만듭니다." : "1. Press [Get Link] to create the authorization link.";
    const std::string authStep2 = isKo ? "2. 휴대폰이나 PC에서 링크를 열고 Dropbox 로그인을 끝냅니다." : "2. Open the link on your phone or PC and finish Dropbox login.";
    const std::string authStep3 = isKo ? "3. 완료 화면에 보이는 코드, 또는 이동된 전체 URL을 복사합니다." : "3. Copy the returned code or the full redirected URL.";
    const std::string authStep4 = isKo ? "4. [키보드 열기]로 붙여넣고 [코드 연결]을 누르면 끝납니다." : "4. Press [Open Keyboard], paste it, then press [Connect Code].";
    const std::string emptyLinkHint = isKo ? "아직 링크가 없습니다. 먼저 [링크 생성]을 누르세요." : "No link yet. Press [Get Link] first.";
    
    int panelWidth = std::max(1180, m_screenWidth - 48);
    panelWidth = std::min(panelWidth, m_screenWidth - 24);
    int panelHeight = m_screenHeight - 110;
    SDL_Rect authRect = {(m_screenWidth - panelWidth) / 2, HEADER_HEIGHT + 10, panelWidth, panelHeight};
    
    renderSoftShadow(authRect, 32, 28, m_colors.Shadow, 8);
    renderGlassPanel(authRect, 32, m_colors.Card, true);
    renderRoundedRect(authRect, 32, m_colors.BorderStrong);
    
    renderTextCentered(LANG("auth.title"), authRect.x, authRect.y + 40, authRect.w, m_fontLarge, m_colors.Accent);

    const int left = authRect.x + 42;
    const int contentWidth = authRect.w - 84;
    int y = authRect.y + 110;

    SDL_Rect guideRect{left, y, contentWidth, 84};
    renderFilledRoundedRect(guideRect, 18, m_colors.CardHover);
    renderRoundedRect(guideRect, 18, m_colors.Border);
    renderWrappedText(nextAction, guideRect.x + 18, guideRect.y + 16, guideRect.w - 36, m_fontSmall, m_authSessionStarted ? m_colors.Accent : m_colors.Text, 4);
    y += 104;

    renderText(authStep1, left, y, m_fontMedium, m_colors.Text); y += 34;
    renderText(authStep2, left, y, m_fontMedium, m_colors.Text); y += 34;
    renderText(authStep3, left, y, m_fontMedium, m_colors.Text); y += 34;
    renderText(authStep4, left, y, m_fontMedium, m_colors.Accent); y += 42;

    if (!m_authUrl.empty()) {
        SDL_Rect urlRect{left, y, contentWidth, 118};
        renderFilledRoundedRect(urlRect, 14, m_colors.Poster);
        renderRoundedRect(urlRect, 14, m_colors.Border);
        renderText(isKo ? "Dropbox 인증 링크" : "Dropbox authorization link", left + 16, y + 12, m_fontSmall, m_colors.TextDim);
        renderWrappedText(m_authUrl, left + 16, y + 38, urlRect.w - 32, m_fontSmall, m_colors.Warning, 2);
        y += 136;
    } else {
        renderText(emptyLinkHint, left, y, m_fontSmall, m_colors.TextDim);
        y += 34;
    }

    m_authTokenBox = {left, y, contentWidth, 108};
    renderFilledRoundedRect(m_authTokenBox, 16, m_colors.Poster);
    renderRoundedRect(m_authTokenBox, 16, m_colors.Border);
    renderText(isKo ? "인증 코드 또는 돌아온 URL" : "Authorization code or redirected URL",
               m_authTokenBox.x + 20, m_authTokenBox.y + 12, m_fontSmall, m_colors.TextDim);
    
    if (m_authToken.empty()) {
        renderWrappedText(LANG("auth.token_placeholder"), m_authTokenBox.x + 20, m_authTokenBox.y + 42, m_authTokenBox.w - 40, m_fontMedium, m_colors.TextDim, 2);
    } else {
        renderWrappedText(m_authToken, m_authTokenBox.x + 20, m_authTokenBox.y + 42, m_authTokenBox.w - 40, m_fontMedium, m_colors.Text, 2);
    }
    y += 130;
    
    m_buttons.clear();
    int btnW = (contentWidth - 48) / 3;
    int btnH = 70;
    int gap = 24;
    int startBtnX = left;
    m_buttons.emplace_back(startBtnX, y, btnW, btnH, m_authSessionStarted ? connectCodeText : createLinkText);
    m_buttons.emplace_back(startBtnX + btnW + gap, y, btnW, btnH, LANG("auth.open_keyboard"));
    m_buttons.emplace_back(startBtnX + (btnW + gap) * 2, y, btnW, btnH, LANG("auth.cancel"));
    
    for (size_t i = 0; i < m_buttons.size(); ++i) {
        auto& btn = m_buttons[i];
        btn.hover = static_cast<int>(i) == m_selectedButtonIndex;
        renderButton(btn);
    }
}

void MainUI::renderUserPicker() {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 160);
    SDL_RenderFillRect(m_renderer, nullptr);

    const int panelWidth = 500;
    const int rowHeight = 92;
    const int panelHeight = std::min(140 + static_cast<int>(m_saveManager.getUsers().size()) * rowHeight, m_screenHeight - 120);
    SDL_Rect panel{m_screenWidth - panelWidth - 16, m_screenHeight - FOOTER_HEIGHT - panelHeight - 10, panelWidth, panelHeight};
    SDL_Rect listViewport{panel.x + 24, panel.y + 76, panel.w - 48, panel.h - 96};
    
    renderFilledRoundedRect(SDL_Rect{panel.x+6, panel.y+8, panel.w, panel.h}, 24, m_colors.Shadow);
    renderFilledRoundedRect(panel, 24, m_colors.Card);
    renderRoundedRect(panel, 24, m_colors.Border);

    renderText(utils::Language::instance().currentLang() == "ko" ? "사용자 전환" : "Switch User",
               panel.x + 24, panel.y + 24, m_fontMedium, m_colors.Text);

    m_userChips.clear();
    const auto& users = m_saveManager.getUsers();
    const int visibleRows = std::max(1, (listViewport.h - 8) / rowHeight);
    const int maxScrollIndex = std::max(0, static_cast<int>(users.size()) - visibleRows);
    m_userPickerScrollIndex = std::clamp(m_userPickerScrollIndex, 0, maxScrollIndex);
    if (m_selectedUserIndex < m_userPickerScrollIndex) {
        m_userPickerScrollIndex = m_selectedUserIndex;
    } else if (m_selectedUserIndex >= m_userPickerScrollIndex + visibleRows) {
        m_userPickerScrollIndex = m_selectedUserIndex - visibleRows + 1;
    }

    for (size_t i = 0; i < users.size(); ++i) {
        UserChip chip;
        chip.user = const_cast<core::UserInfo*>(&users[i]);
        chip.selected = static_cast<int>(i) == m_selectedUserIndex;
        chip.rect = {0, 0, 0, 0};
        m_userChips.push_back(chip);
    }

    for (int row = 0; row < visibleRows; ++row) {
        const int userIndex = m_userPickerScrollIndex + row;
        if (userIndex >= static_cast<int>(users.size())) {
            break;
        }

        UserChip& chip = m_userChips[userIndex];
        chip.rect = {listViewport.x, listViewport.y + row * rowHeight, listViewport.w, 80};

        SDL_Color chipBg = chip.selected ? SDL_Color{28, 55, 86, 255} : m_colors.Poster;
        renderFilledRoundedRect(chip.rect, 18, chipBg);
        renderRoundedRect(chip.rect, 18, chip.selected ? m_colors.Accent : m_colors.Border);

        SDL_Rect avatarRect{chip.rect.x + 14, chip.rect.y + 12, 56, 56};
        bool renderedAvatar = false;
        if (!chip.user->iconPath.empty()) {
            if (SDL_Texture* avatarTexture = loadIcon(chip.user->iconPath)) {
                SDL_RenderCopy(m_renderer, avatarTexture, nullptr, &avatarRect);
                // CRITICAL FIX: DO NOT destroy cached texture!
                renderedAvatar = true;
            }
        }
        if (!renderedAvatar) {
            renderFilledRoundedRect(avatarRect, 28, m_colors.Border);
        }
        renderRoundedRect(avatarRect, 28, chip.selected ? m_colors.Accent : m_colors.BorderStrong);

        const SDL_Color nameColor = chip.selected ? SDL_Color{245, 250, 255, 255} : m_colors.Text;
        const SDL_Color idColor = chip.selected ? SDL_Color{196, 221, 242, 255} : m_colors.TextDim;
        renderText(fitText(m_fontMedium, chip.user->name, chip.rect.w - 120), chip.rect.x + 86, chip.rect.y + 14, m_fontMedium, nameColor);
        renderText(fitText(m_fontSmall, chip.user->id, chip.rect.w - 120), chip.rect.x + 86, chip.rect.y + 46, m_fontSmall, idColor);
    }

    if (maxScrollIndex > 0) {
        const std::string scrollHint = utils::Language::instance().currentLang() == "ko"
            ? "위/아래로 더 많은 사용자 보기"
            : "Use up/down to view more users";
        renderText(scrollHint, panel.x + 24, panel.y + panel.h - 24, m_fontSmall, m_colors.TextDim);
    }
}

void MainUI::renderVersionHistory() {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180); // Slightly darker overlay
    SDL_RenderFillRect(m_renderer, nullptr);

    const int panelWidth = std::min(840, std::max(760, (m_screenWidth * 14) / 20));
    SDL_Rect listRect{m_screenWidth - panelWidth - 20, 40, panelWidth, m_screenHeight - 80};

    // Main Panel with Glass Effect
    renderGlassPanel(listRect, 32, m_colors.Card, false);
    renderRoundedRect(listRect, 32, m_colors.Border);

    SDL_Rect headerCard{listRect.x + 24, listRect.y + 24, listRect.w - 48, 132};
    renderFilledRoundedRect(headerCard, 24, m_colors.CardHover);
    renderRoundedRect(headerCard, 24, m_colors.Border);

    SDL_Rect posterRect{headerCard.x + 20, headerCard.y + 16, 96, 96};
    if (m_selectedTitle && !m_selectedTitle->iconPath.empty()) {
        if (SDL_Texture* posterTexture = loadIcon(m_selectedTitle->iconPath)) {
            SDL_RenderCopy(m_renderer, posterTexture, nullptr, &posterRect);
        }
    } else {
        renderFilledRoundedRect(posterRect, 18, m_colors.Poster);
    }
    renderRoundedRect(posterRect, 18, m_colors.Border);

    const int headerTextX = posterRect.x + posterRect.w + 20;
    const int headerTextW = headerCard.w - (headerTextX - headerCard.x) - 24;
    const std::string historyTitle = utils::Language::instance().currentLang() == "ko"
        ? "세이브 이력"
        : "Save History";
    const std::string historySubtitle = utils::Language::instance().currentLang() == "ko"
        ? "로컬 및 클라우드 세이브를 최신순으로 정렬"
        : "Local and cloud saves, newest first";
    renderText(historyTitle, headerTextX, headerCard.y + 16, m_fontSmall, m_colors.Accent);
    if (m_selectedTitle) {
        renderText(fitText(m_fontLarge, m_selectedTitle->name, headerTextW), headerTextX, headerCard.y + 40, m_fontLarge, m_colors.Text);
        renderText(fitText(m_fontMedium, m_selectedTitle->publisher, headerTextW), headerTextX, headerCard.y + 78, m_fontMedium, m_colors.TextDim);
    }
    const std::string countLabel = std::to_string(m_versionItems.size()) + " " + LANG("detail.versions");
    renderText(countLabel, headerCard.x + headerCard.w - 150, headerCard.y + 18, m_fontSmall, m_colors.TextDim);
    renderText(historySubtitle, headerTextX, headerCard.y + 104, m_fontSmall, m_colors.TextDim);

    const int footerHeight = 110;
    const int rowHeight = 96; // Slightly taller rows
    const int listTop = headerCard.y + headerCard.h + 20;
    const int visibleRows = std::max(1, (listRect.h - footerHeight - 140) / rowHeight);
    const int maxScrollIndex = std::max(0, static_cast<int>(m_versionItems.size()) - visibleRows);
    m_versionScrollIndex = std::clamp(m_versionScrollIndex, 0, maxScrollIndex);
    if (m_selectedVersionIndex < m_versionScrollIndex) {
        m_versionScrollIndex = m_selectedVersionIndex;
    } else if (m_selectedVersionIndex >= m_versionScrollIndex + visibleRows) {
        m_versionScrollIndex = m_selectedVersionIndex - visibleRows + 1;
    }

    auto formatTimestamp = [&](std::time_t timestamp) {
        if (timestamp == 0) return std::string("--");
        char buffer[64];
        std::tm* local = std::localtime(&timestamp);
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local); // Added seconds for precision
        return std::string(buffer);
    };

    // Draw List Items
    int y = listTop;
    for (int row = 0; row < visibleRows; ++row) {
        const int index = m_versionScrollIndex + row;
        if (index >= static_cast<int>(m_versionItems.size())) break;

        VersionItem& item = m_versionItems[index];
        item.selected = index == m_selectedVersionIndex;
        item.rect = {listRect.x + 24, y, listRect.w - 48, 86};

        // Selection / Hover Highlight
        SDL_Color itemBg = item.selected ? SDL_Color{m_colors.Accent.r, m_colors.Accent.g, m_colors.Accent.b, 45} : m_colors.Poster;
        renderFilledRoundedRect(item.rect, 16, itemBg);
        renderRoundedRect(item.rect, 16, item.selected ? m_colors.Accent : m_colors.Border);

        const std::string sizeLabel = formatStorageSize(static_cast<int64_t>(item.size));
        const std::string deviceLabel = item.deviceLabel.empty()
            ? ((item.sourceLabel == "cloud" || item.sourceLabel == "CLOUD")
                ? std::string(LANG("detail.cloud_inside"))
                : (item.isLocal ? m_saveManager.getDeviceLabel() : std::string(LANG("history.unknown_device"))))
            : item.deviceLabel;
        const std::string sourceLabel = item.sourceLabel.empty()
            ? (item.isLocal ? std::string("LOCAL") : std::string("CLOUD"))
            : item.sourceLabel;
        const std::string timestampLabel = formatTimestamp(item.timestamp);

        // Version ID (Meaningful Numbering)
        std::string versionNum = "#" + std::to_string(m_versionItems.size() - index);
        int vnW, vnH;
        TTF_SizeUTF8(m_fontMedium, versionNum.c_str(), &vnW, &vnH);
        renderText(versionNum, item.rect.x + 16, item.rect.y + 12, m_fontMedium, item.selected ? m_colors.Accent : m_colors.TextDim);

        // Source Badge
        int badgeW = sourceLabel == "LOCAL" ? 70 : 80;
        SDL_Rect sourceBadge{item.rect.x + 16, item.rect.y + 48, badgeW, 24};
        SDL_Color badgeColor = item.isLocal ? m_colors.Synced : m_colors.Accent;
        renderFilledRoundedRect(sourceBadge, 12, SDL_Color{badgeColor.r, badgeColor.g, badgeColor.b, 30});
        renderRoundedRect(sourceBadge, 12, badgeColor);
        renderTextCentered(sourceLabel, sourceBadge.x, sourceBadge.y + 2, sourceBadge.w, m_fontSmall, badgeColor);

        const SDL_Color titleColor = item.selected ? SDL_Color{248, 250, 255, 255} : m_colors.Text;
        const SDL_Color metaColor = item.selected ? SDL_Color{206, 221, 236, 255} : m_colors.TextDim;
        
        // Detailed info
        const std::string userDeviceLabel = item.userName.empty() ? deviceLabel : item.userName + " (" + deviceLabel + ")";
        renderText(fitText(m_fontMedium, item.name, item.rect.w - 320), item.rect.x + 80, item.rect.y + 12, m_fontMedium, titleColor);
        renderText(fitText(m_fontSmall, userDeviceLabel, item.rect.w - 320), item.rect.x + 80 + badgeW + 10, item.rect.y + 48, m_fontSmall, metaColor);
        
        // Right side: Time and Size
        int tw, th, sw, sh;
        TTF_SizeUTF8(m_fontSmall, timestampLabel.c_str(), &tw, &th);
        TTF_SizeUTF8(m_fontSmall, sizeLabel.c_str(), &sw, &sh);
        renderText(timestampLabel, item.rect.x + item.rect.w - 20 - tw, item.rect.y + 12, m_fontSmall, metaColor);
        renderText(sizeLabel, item.rect.x + item.rect.w - 20 - sw, item.rect.y + 48, m_fontSmall, m_colors.Accent);

        y += rowHeight;
    }

    if (maxScrollIndex > 0) {
        SDL_Rect scrollTrack{listRect.x + listRect.w - 18, listTop, 6, visibleRows * rowHeight - 10};
        renderFilledRoundedRect(scrollTrack, 3, SDL_Color{m_colors.Border.r, m_colors.Border.g, m_colors.Border.b, 90});
        const int thumbH = std::max(28, (scrollTrack.h * visibleRows) / std::max(visibleRows, static_cast<int>(m_versionItems.size())));
        const int thumbY = scrollTrack.y + ((scrollTrack.h - thumbH) * m_versionScrollIndex) / std::max(1, maxScrollIndex);
        SDL_Rect thumb{scrollTrack.x, thumbY, scrollTrack.w, thumbH};
        renderFilledRoundedRect(thumb, 3, m_colors.Accent);
    }

    // Integrated Footer Area (No separate box)
    const int footerY = listRect.y + listRect.h - footerHeight;
    
    // Divider line
    SDL_SetRenderDrawColor(m_renderer, m_colors.Border.r, m_colors.Border.g, m_colors.Border.b, 100);
    SDL_RenderDrawLine(m_renderer, listRect.x + 32, footerY, listRect.x + listRect.w - 32, footerY);

    m_buttons.clear();
    const int buttonY = footerY + 25;
    const int buttonGap = 24;
    const int buttonW = (listRect.w - 64 - buttonGap) / 2;
    const int buttonH = 60;
    const bool hasSelection = m_selectedVersionIndex >= 0 && m_selectedVersionIndex < static_cast<int>(m_versionItems.size());
    
    if (m_confirmDeleteVersion && hasSelection) {
        const bool isKo = utils::Language::instance().currentLang() == "ko";
        SDL_Rect confirmCard{listRect.x + 72, listRect.y + (listRect.h / 2) - 92, listRect.w - 144, 184};
        renderFilledRoundedRect(confirmCard, 24, SDL_Color{28, 33, 42, 250});
        renderRoundedRect(confirmCard, 24, m_colors.Error);
        renderTextCentered(isKo ? "세이브 삭제 확인" : "Confirm Save Deletion",
                           confirmCard.x, confirmCard.y + 24, confirmCard.w, m_fontMedium, m_colors.Error);
        renderTextCentered(isKo ? "선택하신 세이브를 삭제하시겠습니까?" : "Do you want to delete the selected save?",
                           confirmCard.x + 24, confirmCard.y + 74, confirmCard.w - 48, m_fontSmall, m_colors.Text);
        m_buttons.emplace_back(confirmCard.x + 24, confirmCard.y + 112, buttonW, buttonH, isKo ? "삭제 확인" : "Confirm Delete");
        m_buttons.emplace_back(confirmCard.x + confirmCard.w - 24 - buttonW, confirmCard.y + 112, buttonW, buttonH, isKo ? "취소" : "Cancel");
    } else {
        const bool isKo = utils::Language::instance().currentLang() == "ko";
        m_buttons.emplace_back(listRect.x + 32, buttonY, buttonW, buttonH, isKo ? "삭제" : "Delete");
        m_buttons.emplace_back(listRect.x + 32 + buttonW + buttonGap, buttonY, buttonW, buttonH, isKo ? "복원" : "Restore");
        if (!hasSelection) {
            m_buttons[0].hover = false;
            m_buttons[1].hover = false;
        }
    }

    for (size_t i = 0; i < m_buttons.size(); ++i) {
        auto& btn = m_buttons[i];
        btn.hover = static_cast<int>(i) == m_selectedButtonIndex;
        renderButton(btn);
    }
}

void MainUI::renderSyncProgress() {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(m_renderer, nullptr);
    
    SDL_Rect progressRect = {m_screenWidth / 2 - 300, m_screenHeight / 2 - 100, 600, 200};
    renderFilledRoundedRect(SDL_Rect{progressRect.x+6, progressRect.y+8, progressRect.w, progressRect.h}, 24, m_colors.Shadow);
    renderFilledRoundedRect(progressRect, 24, m_colors.Card);
    renderRoundedRect(progressRect, 24, m_colors.Border);
    
    renderTextCentered(LANG("sync.syncing"), progressRect.x, progressRect.y + 30, progressRect.w, m_fontLarge, m_colors.Accent);

    // Progress bar
    SDL_Rect track = {progressRect.x + 40, progressRect.y + 90, progressRect.w - 80, 12};
    renderFilledRoundedRect(track, 6, m_colors.AccentSoft);
    
    float progress = static_cast<float>(m_syncProgress) / std::max(1, m_syncTotal);
    SDL_Rect fill = {track.x, track.y, static_cast<int>(track.w * progress), track.h};
    if (fill.w > 0) {
        renderFilledRoundedRect(fill, 6, m_colors.Accent);
    }
    
    renderTextCentered(m_syncStatus, progressRect.x, progressRect.y + 130, progressRect.w, m_fontSmall, m_colors.Text);
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

void MainUI::renderTextWithShadow(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color) {
    // Drop shadow for text
    renderText(text, x, y + 2, font, SDL_Color{0, 0, 0, 180});
    renderText(text, x, y, font, color);
}

void MainUI::renderTextCentered(const std::string& text, int x, int y, int w, TTF_Font* font, SDL_Color color) {
    if (!font) return;
    
    int textW, textH;
    TTF_SizeUTF8(font, text.c_str(), &textW, &textH);
    renderText(text, x + (w - textW) / 2, y, font, color);
}

int MainUI::renderWrappedText(const std::string& text, int x, int y, int maxWidth, TTF_Font* font, SDL_Color color, int lineGap) {
    if (!font || text.empty() || maxWidth <= 0) {
        return y;
    }

    int sampleW = 0;
    int lineHeight = 0;
    TTF_SizeUTF8(font, "Ag", &sampleW, &lineHeight);

    auto flushLine = [&](const std::string& line, int drawY) {
        if (!line.empty()) {
            renderText(line, x, drawY, font, color);
        }
    };

    int drawY = y;
    std::string currentLine;
    std::string currentWord;

    auto appendWord = [&](const std::string& word) {
        if (word.empty()) {
            return;
        }

        std::string trial = currentLine.empty() ? word : currentLine + " " + word;
        int trialW = 0;
        int trialH = 0;
        TTF_SizeUTF8(font, trial.c_str(), &trialW, &trialH);
        if (trialW <= maxWidth) {
            currentLine = trial;
            return;
        }

        if (!currentLine.empty()) {
            flushLine(currentLine, drawY);
            drawY += lineHeight + lineGap;
            currentLine.clear();
        }

        std::string chunk;
        for (char ch : word) {
            std::string nextChunk = chunk + ch;
            TTF_SizeUTF8(font, nextChunk.c_str(), &trialW, &trialH);
            if (!chunk.empty() && trialW > maxWidth) {
                flushLine(chunk, drawY);
                drawY += lineHeight + lineGap;
                chunk.assign(1, ch);
            } else {
                chunk = nextChunk;
            }
        }
        currentLine = chunk;
    };

    for (char ch : text) {
        if (ch == '\n') {
            appendWord(currentWord);
            currentWord.clear();
            flushLine(currentLine, drawY);
            drawY += lineHeight + lineGap;
            currentLine.clear();
        } else if (ch == ' ') {
            appendWord(currentWord);
            currentWord.clear();
        } else {
            currentWord.push_back(ch);
        }
    }

    appendWord(currentWord);
    if (!currentLine.empty()) {
        flushLine(currentLine, drawY);
        drawY += lineHeight;
    }

    return drawY;
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

    // Check Cache
    auto it = m_iconCache.find(path);
    if (it != m_iconCache.end()) return it->second;

    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) return nullptr;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);

    if (texture) {
        m_iconCache[path] = texture;
    }

    return texture;
}
// Actions
void MainUI::syncToDropbox() {
    if (!m_selectedTitle || !m_selectedTitle->hasSave || !m_dropbox.isAuthenticated()) return;
    
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
    const std::string cloudDir = directoryFromPath(dropboxPath);
    const std::string archiveName = fileNameFromPath(archivePath);
    const std::string metaName = fileNameFromPath(localMetaPath);
    const std::string revisionDropboxPath = cloudDir.empty() ? ("/" + archiveName) : ("/" + cloudDir + "/" + archiveName);
    const std::string revisionMetaDropboxPath = cloudDir.empty() ? ("/" + metaName) : ("/" + cloudDir + "/" + metaName);

    if (m_dropbox.uploadFile(localMetaPath, revisionMetaDropboxPath) &&
        m_dropbox.uploadFile(archivePath, revisionDropboxPath) &&
        m_dropbox.uploadFile(localMetaPath, dropboxMetaPath) &&
        m_dropbox.uploadFile(archivePath, dropboxPath)) {
        m_syncStatus = "Uploaded with metadata-aware sync";
        showToast(LANG("sync.complete"));
    } else {
        m_syncStatus = "Dropbox upload failed";
        showToast("Upload failed", true);
    }
    
    updateGameCards();
}

void MainUI::backupLocal() {
    if (!m_selectedTitle || !m_selectedTitle->hasSave) return;
    m_saveManager.createVersionedBackup(m_selectedTitle);
    updateGameCards();
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
        item.sourceLabel = "LOCAL";
        item.timestamp = v.timestamp;
        item.size = v.size;
        item.isLocal = true;
        item.selected = false;
        m_versionItems.push_back(item);
    }

    if (m_dropbox.isAuthenticated()) {
        const std::string cloudFolder = directoryFromPath("/" + m_saveManager.getCloudPath(m_selectedTitle));
        auto remoteFiles = m_dropbox.listFolder(cloudFolder);
        for (const auto& remote : remoteFiles) {
            if (remote.isFolder || !endsWith(remote.name, ".meta") || remote.name == "latest.meta") {
                continue;
            }

            char tempMeta[256];
            std::snprintf(tempMeta, sizeof(tempMeta), "%s/%016llX_%s", utils::paths::TEMP,
                          static_cast<unsigned long long>(m_selectedTitle->titleId),
                          remote.name.c_str());
            if (!m_dropbox.downloadFile(remote.path, tempMeta)) {
                continue;
            }

            core::BackupMetadata incomingMeta;
            const bool validMeta = m_saveManager.readMetadataFile(tempMeta, incomingMeta);
            remove(tempMeta);
            if (!validMeta) {
                continue;
            }

            VersionItem item;
            item.name = incomingMeta.backupName.empty() ? remote.name : incomingMeta.backupName;
            item.path = remote.path.substr(0, remote.path.size() - 5) + ".zip";
            item.deviceLabel = incomingMeta.deviceLabel.empty() ? LANG("history.unknown_device") : incomingMeta.deviceLabel;
            item.userName = incomingMeta.userName.empty() ? LANG("history.unknown_user") : incomingMeta.userName;
            item.sourceLabel = "CLOUD";
            item.timestamp = incomingMeta.createdAt != 0 ? incomingMeta.createdAt : remote.modifiedTime;
            item.size = incomingMeta.size > 0 ? incomingMeta.size : static_cast<int64_t>(remote.size);
            item.isLocal = false;
            item.selected = false;
            m_versionItems.push_back(item);
        }
    }

    std::sort(m_versionItems.begin(), m_versionItems.end(), [](const VersionItem& a, const VersionItem& b) {
        return a.timestamp > b.timestamp;
    });
    
    m_selectedVersionIndex = 0;
    m_versionScrollIndex = 0;
    m_selectedButtonIndex = -1;
    m_confirmDeleteVersion = false;
    m_state = State::VersionHistory;
}

void MainUI::showCloudPicker() {
    if (!m_selectedTitle || !m_selectedTitle->hasSave || !m_dropbox.isAuthenticated()) return;

    m_syncStatus = LANG("sync.downloading");
    m_state = State::SyncAll;
    m_syncProgress = 0;
    m_syncTotal = 1;

    m_versionItems.clear();
    const std::string cloudFolder = directoryFromPath("/" + m_saveManager.getCloudPath(m_selectedTitle));
    auto remoteFiles = m_dropbox.listFolder(cloudFolder);
    for (const auto& remote : remoteFiles) {
        if (remote.isFolder || !endsWith(remote.name, ".meta") || remote.name == "latest.meta") {
            continue;
        }

        char tempMeta[256];
        std::snprintf(tempMeta, sizeof(tempMeta), "%s/%016llX_%s", utils::paths::TEMP,
                      static_cast<unsigned long long>(m_selectedTitle->titleId),
                      remote.name.c_str());
        if (!m_dropbox.downloadFile(remote.path, tempMeta)) {
            continue;
        }

        core::BackupMetadata incomingMeta;
        const bool validMeta = m_saveManager.readMetadataFile(tempMeta, incomingMeta);
        remove(tempMeta);
        if (!validMeta) {
            continue;
        }

        VersionItem item;
        item.name = incomingMeta.backupName.empty() ? remote.name : incomingMeta.backupName;
        item.path = remote.path.substr(0, remote.path.size() - 5) + ".zip";
        item.deviceLabel = incomingMeta.deviceLabel.empty() ? LANG("history.unknown_device") : incomingMeta.deviceLabel;
        item.userName = incomingMeta.userName.empty() ? LANG("history.unknown_user") : incomingMeta.userName;
        item.sourceLabel = incomingMeta.source.empty() ? "cloud" : incomingMeta.source;
        item.timestamp = incomingMeta.createdAt != 0 ? incomingMeta.createdAt : remote.modifiedTime;
        item.size = incomingMeta.size > 0 ? incomingMeta.size : static_cast<int64_t>(remote.size);
        item.isLocal = false;
        item.selected = false;
        m_versionItems.push_back(item);
    }

    std::sort(m_versionItems.begin(), m_versionItems.end(), [](const VersionItem& a, const VersionItem& b) {
        return a.timestamp > b.timestamp;
    });
    if (!m_versionItems.empty()) {
        m_versionItems[0].selected = true;
    }

    m_selectedVersionIndex = 0;
    m_selectedButtonIndex = -1;
    m_state = State::CloudPicker;
}

void MainUI::showUserPicker() {
    m_selectedUserIndex = 0;
    m_userPickerScrollIndex = 0;
    const auto& users = m_saveManager.getUsers();
    core::UserInfo* selectedUser = m_saveManager.getSelectedUser();
    for (size_t i = 0; i < users.size(); ++i) {
        if (selectedUser == &users[i]) {
            m_selectedUserIndex = static_cast<int>(i);
            m_userPickerScrollIndex = m_selectedUserIndex;
            break;
        }
    }
    m_state = State::UserPicker;
}

void MainUI::selectUser(size_t index) {
    if (!m_saveManager.selectUser(index)) {
        m_state = State::Main;
        return;
    }

    m_selectedTitle = nullptr;
    m_selectedIndex = 0;
    m_scrollRow = 0;
    m_selectedUserIndex = static_cast<int>(index);
    updateGameCards();
    refreshSyncStates();
    m_state = State::Main;
}

void MainUI::downloadCloudItem(VersionItem* item) {
    if (!item || !m_selectedTitle || !m_dropbox.isAuthenticated()) {
        m_state = State::GameDetail;
        return;
    }

    m_syncStatus = LANG("sync.downloading");
    char localZip[256];
    std::snprintf(localZip, sizeof(localZip), "%s/%016llX_selected.zip", utils::paths::TEMP,
                  static_cast<unsigned long long>(m_selectedTitle->titleId));

    if (!m_dropbox.downloadFile(item->path, localZip)) {
        m_syncStatus = "Dropbox download failed";
        m_state = State::GameDetail;
        return;
    }

    std::string reason;
    if (m_saveManager.importBackupArchive(m_selectedTitle, localZip, &reason, false)) {
        m_syncStatus = reason.empty() ? "Download complete" : reason;
        showToast(LANG("sync.complete"));
    } else {
        m_syncStatus = reason.empty() ? "Import failed" : reason;
        showToast("Import failed", true);
    }

    remove(localZip);
    updateGameCards();
    refreshSyncStates();
    m_state = State::GameDetail;
}

void MainUI::restoreVersion(VersionItem* item) {
    if (!item || !m_selectedTitle) return;
    m_saveManager.restoreSave(m_selectedTitle, item->path);
    updateGameCards();
    refreshSyncStates();
    m_state = State::GameDetail;
}

void MainUI::deleteVersion(VersionItem* item) {
    if (!item || !m_selectedTitle) {
        return;
    }

    bool ok = false;
    if (item->isLocal) {
        ok = m_saveManager.deleteBackup(item->path);
    } else if (m_dropbox.isAuthenticated()) {
        const std::string metaPath = item->path.substr(0, item->path.size() - 4) + ".meta";
        ok = m_dropbox.deleteFile(item->path);
        m_dropbox.deleteFile(metaPath);
    }

    if (ok) {
        showToast(utils::Language::instance().currentLang() == "ko" ? "세이브 이력을 삭제했습니다." : "Save history entry deleted.");
    } else {
        showToast(utils::Language::instance().currentLang() == "ko" ? "세이브 이력 삭제에 실패했습니다." : "Failed to delete save history entry.", true);
    }

    m_confirmDeleteVersion = false;
    showVersionHistory();
    updateGameCards();
    refreshSyncStates();
}

void MainUI::syncAllGames() {
    if (!m_dropbox.isAuthenticated()) {
        m_state = State::Auth;
        return;
    }

    refreshSyncStates();
    m_state = State::SyncAll;
    m_syncProgress = 0;
    m_syncTotal = m_gameCards.size();
    int uploadedCount = 0;

    for (auto& card : m_gameCards) {
        if (card.syncState != GameCard::SyncState::NeedsUpload) {
            m_syncProgress++;
            continue;
        }
        m_syncStatus = card.title->name + " " + LANG("sync.syncing_game");
        m_selectedTitle = card.title;
        syncToDropbox();
        uploadedCount++;
        m_syncProgress++;
    }

    refreshSyncStates();
    m_syncStatus = uploadedCount > 0 ? LANG("sync.complete") : LANG("sync.no_uploads");
    m_state = State::Main;
}

void MainUI::connectDropbox() {
    const bool isKo = utils::Language::instance().currentLang() == "ko";
    if (!m_authSessionStarted) {
        m_authUrl = m_dropbox.getAuthorizeUrl();
        if (m_authUrl.empty()) {
            m_syncStatus = isKo ? "Dropbox 인증 링크 생성에 실패했습니다." : "Failed to create Dropbox authorization link.";
            return;
        }
        m_authSessionStarted = true;
        m_syncStatus = isKo ? "링크를 휴대폰에서 연 뒤, 완료 화면의 코드나 URL을 붙여넣으세요." : "Open the link on your phone, then paste the returned code or URL.";
        return;
    }

    if (m_authToken.empty()) {
        m_syncStatus = isKo ? "먼저 키보드를 열고 코드나 URL을 붙여넣으세요." : "Open the keyboard and paste the code or URL first.";
        return;
    }

    if (m_dropbox.exchangeAuthorizationCode(m_authToken)) {
        m_syncStatus = isKo ? "Dropbox 연결이 완료되었습니다." : "Dropbox connected.";
        showToast(isKo ? "Dropbox 연결 완료" : "Dropbox Connected!");
        m_authSessionStarted = false;
        m_authUrl.clear();
        m_authToken.clear();
        updateGameCards();
        refreshSyncStates(true);
        m_state = State::Main;
    } else {
        m_syncStatus = isKo ? "코드 연결에 실패했습니다. 코드나 전체 URL을 다시 확인하세요." : "Code exchange failed. Check the code or pasted URL.";
        showToast(isKo ? "연결 실패" : "Connection failed", true);
    }
}

void MainUI::openTokenKeyboard() {
#ifdef __SWITCH__
    const bool isKo = utils::Language::instance().currentLang() == "ko";
    SwkbdConfig keyboard{};
    if (R_FAILED(swkbdCreate(&keyboard, 0))) {
        m_syncStatus = isKo ? "키보드를 열지 못했습니다." : "Keyboard open failed.";
        return;
    }

    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetHeaderText(&keyboard, isKo ? "Dropbox 인증 코드 입력" : "Dropbox authorization code");
    swkbdConfigSetSubText(&keyboard, isKo ? "완료 화면의 코드 또는 전체 URL을 붙여넣으세요" : "Paste the returned code or the full redirected URL");
    swkbdConfigSetGuideText(&keyboard, isKo ? "코드 또는 URL" : "Code or URL");
    swkbdConfigSetInitialText(&keyboard, m_authToken.c_str());
    swkbdConfigSetStringLenMin(&keyboard, 1);
    swkbdConfigSetStringLenMax(&keyboard, 512);
    swkbdConfigSetOkButtonText(&keyboard, isKo ? "입력" : "Save");

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
        m_scrollRow = 0;
        return;
    }

    m_selectedIndex = std::clamp(m_selectedIndex, 0, (int)m_gameCards.size() - 1);
    const int selectedRow = m_selectedIndex / std::max(1, m_gridCols);
    const int totalRows = std::max(1, ((int)m_gameCards.size() + m_gridCols - 1) / m_gridCols);
    const int maxScrollRow = std::max(0, totalRows - m_gridRows);
    if (selectedRow < m_scrollRow) {
        m_scrollRow = selectedRow;
    } else if (selectedRow >= m_scrollRow + m_gridRows) {
        m_scrollRow = selectedRow - m_gridRows + 1;
    }
    m_scrollRow = std::clamp(m_scrollRow, 0, maxScrollRow);
}

void MainUI::showToast(const std::string& message, bool isError) {
    m_toast.message = message;
    m_toast.isError = isError;
    m_toast.timer = 180; // ~3 seconds at 60fps
    m_toast.alpha = 0.0f;
    m_toast.active = true;
}

void MainUI::renderToast() {
    if (!m_toast.active) return;

    int textW, textH;
    TTF_SizeUTF8(m_fontSmall, m_toast.message.c_str(), &textW, &textH);
    
    int panelW = textW + 60;
    int panelH = 44;
    SDL_Rect toastRect = {(m_screenWidth - panelW) / 2, m_screenHeight - 120, panelW, panelH};
    
    Uint8 alpha = static_cast<Uint8>(m_toast.alpha);
    SDL_Color bgColor = m_toast.isError ? SDL_Color{239, 68, 68, static_cast<Uint8>(alpha * 0.8f)} : SDL_Color{30, 41, 59, static_cast<Uint8>(alpha * 0.9f)};
    
    renderGlassPanel(toastRect, 22, bgColor, true);
    
    SDL_Color textColor = {255, 255, 255, alpha};
    renderTextCentered(m_toast.message, toastRect.x, toastRect.y + (toastRect.h - textH)/2, toastRect.w, m_fontSmall, textColor);
}

int MainUI::getItemsPerPage() const {
    return 12;
}

int MainUI::getVisibleStartIndex() const {
    return m_scrollRow * m_gridCols;
}

} // namespace ui
