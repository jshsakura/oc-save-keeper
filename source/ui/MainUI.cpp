/**
 * oc-save-keeper - Main UI implementation
 */

#include "ui/MainUI.hpp"
#include "utils/Language.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace ui {

namespace Theme {
    ColorPalette Light() {
        return {
            {248, 250, 252, 255}, // Background: Slate 50
            {255, 255, 255, 255}, // Header: White
            {255, 255, 255, 255}, // Card: White
            {241, 245, 249, 255}, // CardHover: Slate 100
            {59, 130, 246, 255},  // Accent: Blue 500
            {219, 234, 254, 255}, // AccentSoft: Blue 100
            {245, 158, 11, 255},  // Warning: Amber 500
            {239, 68, 68, 255},   // Error: Red 500
            {15, 23, 42, 255},    // Text: Slate 900
            {100, 116, 139, 255}, // TextDim: Slate 500
            {34, 197, 94, 255},   // Synced: Green 500
            {148, 163, 184, 255}, // NotSynced: Slate 400
            {226, 232, 240, 255}, // Border: Slate 200
            {59, 130, 246, 255},  // BorderStrong: Blue 500
            {248, 250, 252, 255}, // Poster: Slate 50
            {255, 255, 255, 255}, // TitleStrip: White
            {0, 0, 0, 20},        // Shadow
            {59, 130, 246, 40}    // SelectionGlow
        };
    }

    ColorPalette Dark() {
        return {
            {15, 23, 42, 255},    // Background: Slate 900
            {30, 41, 59, 255},    // Header: Slate 800
            {30, 41, 59, 255},    // Card: Slate 800
            {51, 65, 85, 255},    // CardHover: Slate 700
            {56, 189, 248, 255},  // Accent: Sky 400
            {12, 74, 110, 255},   // AccentSoft: Sky 900
            {251, 191, 36, 255},  // Warning: Amber 400
            {248, 113, 113, 255}, // Error: Red 400
            {248, 250, 252, 255}, // Text: Slate 50
            {148, 163, 184, 255}, // TextDim: Slate 400
            {34, 197, 94, 255},   // Synced: Green 500
            {71, 85, 105, 255},   // NotSynced: Slate 600
            {51, 65, 85, 255},    // Border: Slate 700
            {56, 189, 248, 255},  // BorderStrong: Sky 400
            {15, 23, 42, 255},    // Poster: Slate 900
            {30, 41, 59, 255},    // TitleStrip: Slate 800
            {0, 0, 0, 150},       // Shadow
            {56, 189, 248, 60}    // SelectionGlow
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
    , m_selectedVersionIndex(0)
    , m_selectedUserIndex(0)
    , m_selectedButtonIndex(0)
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
        std::snprintf(localMetaPath, sizeof(localMetaPath), "/switch/oc-save-keeper/temp/%016llX_card.meta",
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
                    } else if (m_state == State::VersionHistory &&
                               m_selectedVersionIndex < (int)m_versionItems.size()) {
                        restoreVersion(&m_versionItems[m_selectedVersionIndex]);
                    }
                    break;
                case 1: // B
                    if (m_state == State::Auth) {
                        m_dropbox.cancelPendingAuthorization();
                        m_authSessionStarted = false;
                        m_authUrl.clear();
                        m_authToken.clear();
                        m_state = State::Main;
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
        } else if (m_state == State::VersionHistory &&
                   m_selectedVersionIndex < (int)m_versionItems.size()) {
            restoreVersion(&m_versionItems[m_selectedVersionIndex]);
        }
    }

    if (keysDown & HidNpadButton_B) {
        if (m_state == State::Auth) {
            m_dropbox.cancelPendingAuthorization();
            m_authSessionStarted = false;
            m_authUrl.clear();
            m_authToken.clear();
            m_state = State::Main;
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
    if (!pressed) return;
    
    if (m_state == State::Main) {
        if (x >= m_statusButton.x && x < m_statusButton.x + m_statusButton.w &&
            y >= m_statusButton.y && y < m_statusButton.y + m_statusButton.h) {
            if (!m_dropbox.isAuthenticated()) {
                m_state = State::Auth;
            }
            return;
        }

        if (x >= m_languageButton.x && x < m_languageButton.x + m_languageButton.w &&
            y >= m_languageButton.y && y < m_languageButton.y + m_languageButton.h) {
            toggleLanguage();
            return;
        }

        if (x >= m_userButton.x && x < m_userButton.x + m_userButton.w &&
            y >= m_userButton.y && y < m_userButton.y + m_userButton.h) {
            showUserPicker();
            return;
        }

        if (x >= m_refreshButton.x && x < m_refreshButton.x + m_refreshButton.w &&
            y >= m_refreshButton.y && y < m_refreshButton.y + m_refreshButton.h) {
            refreshSyncStates();
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
    } else if (m_state == State::GameDetail || m_state == State::Auth || m_state == State::VersionHistory || m_state == State::CloudPicker) {
        if (m_state == State::VersionHistory || m_state == State::CloudPicker) {
            for (size_t i = 0; i < m_versionItems.size(); i++) {
                SDL_Rect& r = m_versionItems[i].rect;
                if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
                    m_selectedVersionIndex = i;
                    if (m_state == State::VersionHistory) {
                        restoreVersion(&m_versionItems[i]);
                    }
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
    } else if (m_state == State::UserPicker) {
        for (size_t i = 0; i < m_userChips.size(); ++i) {
            const SDL_Rect& r = m_userChips[i].rect;
            if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
                selectUser(i);
                return;
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
    std::string closeText = closeLabel();
    std::string connectText = LANG("auth.connect");
    std::string cancelText = LANG("auth.cancel");
    
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
    } else if (btn.text == connectText) {
        connectDropbox();
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
    
    // Smooth selection pulse animation
    static float timer = 0;
    timer += 0.06f;
    m_selectionScale = 1.0f + 0.025f * sin(timer);
    m_selectionAlpha = 180.0f + 75.0f * sin(timer);
    
    // Smooth scrolling
    float targetOffset = static_cast<float>(m_scrollRow);
    m_scrollOffset += (targetOffset - m_scrollOffset) * 0.25f;
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
    // Standard Base Y for Header alignment (Centered around 50px mark)
    const int baseY = 32;
    const int centerY = baseY + (42 / 2); // logoBar height is 42

    // Floating Logo and Title
    SDL_Rect logoBar = {32, baseY, 6, 42};
    renderFilledRoundedRect(logoBar, 3, m_colors.Accent);
    renderText(LANG("app.name"), 54, baseY + 4, m_fontLarge, m_colors.Text);

    bool connected = m_dropbox.isAuthenticated();
    std::string status = connected ? LANG("status.connected") : LANG("status.disconnected");
    SDL_Color statusColor = connected ? m_colors.Synced : m_colors.TextDim;
    
    // Exact vertical centering for buttons
    m_languageButton = {m_screenWidth - 110, centerY - (38 / 2), 76, 38};

    int statusWidth = 0, statusHeight = 0;
    TTF_SizeUTF8(m_fontSmall, status.c_str(), &statusWidth, &statusHeight);
    m_statusButton = {m_languageButton.x - statusWidth - 48, centerY - (38 / 2), statusWidth + 32, 38};
    
    // Status Badge (Glassy)
    renderGlassPanel(m_statusButton, 19, SDL_Color{255, 255, 255, 15}, true);
    renderTextCentered(status, m_statusButton.x, m_statusButton.y + (m_statusButton.h - statusHeight) / 2, m_statusButton.w, m_fontSmall, statusColor);

    // Language Toggle (Glassy)
    renderGlassPanel(m_languageButton, 19, SDL_Color{255, 255, 255, 15}, true);
    renderTextCentered(utils::Language::instance().currentLang() == "ko" ? "KO" : "EN",
                       m_languageButton.x, m_languageButton.y + (m_languageButton.h - statusHeight) / 2, m_languageButton.w, m_fontSmall, m_colors.Accent);
}

void MainUI::renderFooter() {
    const int footerY = m_screenHeight - 68;
    const int footerH = 40;
    const int footerCenterY = footerY + (footerH / 2);

    auto renderPill = [&](const std::string& key, const std::string& label, int& x) {
        int kw, kh, lw, lh;
        TTF_SizeUTF8(m_fontSmall, key.c_str(), &kw, &kh);
        TTF_SizeUTF8(m_fontSmall, label.c_str(), &lw, &lh);
        
        int pillW = kw + lw + 48;
        SDL_Rect pill = {x, footerY, pillW, footerH};
        renderGlassPanel(pill, 20, SDL_Color{255, 255, 255, 10}, true);
        
        renderText(key, pill.x + 16, pill.y + (pill.h - kh) / 2, m_fontSmall, m_colors.Accent);
        renderText(label, pill.x + kw + 28, pill.y + (pill.h - lh) / 2, m_fontSmall, m_colors.TextDim);
        
        x += pillW + 16;
    };

    int currentX = 32;
    renderPill("A", utils::Language::instance().currentLang() == "ko" ? "선택" : "Select", currentX);
    renderPill("B", utils::Language::instance().currentLang() == "ko" ? "뒤로" : "Back", currentX);
    renderPill("X", utils::Language::instance().currentLang() == "ko" ? "새로고침" : "Refresh", currentX);

    core::UserInfo* selectedUser = m_saveManager.getSelectedUser();
    const std::string userName = selectedUser ? selectedUser->name : std::string("User");
    int userTextW = 0, userTextH = 0;
    TTF_SizeUTF8(m_fontSmall, userName.c_str(), &userTextW, &userTextH);
    const int chipW = std::min(300, std::max(160, userTextW + 80));
    m_userButton = {m_screenWidth - chipW - 32, footerY, chipW, footerH};
    
    renderGlassPanel(m_userButton, 20, SDL_Color{255, 255, 255, 15}, true);
    // User text with strict fitting to prevent overflow into boundaries
    renderText(fitText(m_fontSmall, userName, m_userButton.w - 64), m_userButton.x + 52, m_userButton.y + (m_userButton.h - userTextH)/2, m_fontSmall, m_colors.Text);
}

void MainUI::renderGameList() {
    const int startIndex = getVisibleStartIndex();
    const int endIndex = std::min((int)m_gameCards.size(), startIndex + getItemsPerPage());
    
    // Calculate layout parameters
    const int outerMargin = 14;
    const int gridGap = 12;
    const int contentHeight = m_screenHeight - HEADER_HEIGHT - FOOTER_HEIGHT - outerMargin * 2;
    const int cardWidth = std::max(150, (m_screenWidth - outerMargin * 2 - gridGap * (m_gridCols - 1)) / m_gridCols);
    const int preferredCardHeight = cardWidth + 64;
    const int maxCardHeight = std::max(220, (contentHeight - gridGap * (m_gridRows - 1)) / m_gridRows);
    const int cardHeight = std::min(preferredCardHeight, maxCardHeight);

    for (int i = startIndex; i < endIndex; i++) {
        GameCard& card = m_gameCards[i];
        card.selected = ((int)i == m_selectedIndex);
        
        // Recalculate card rect based on current scroll position
        const int indexInView = i - startIndex;
        const int row = indexInView / m_gridCols;
        const int col = indexInView % m_gridCols;
        card.rect.x = outerMargin + col * (cardWidth + gridGap);
        card.rect.y = HEADER_HEIGHT + outerMargin + row * (cardHeight + gridGap);
        card.rect.w = cardWidth;
        card.rect.h = cardHeight;

        renderCard(card);
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
            double angle = (startAngle + i) * M_PI / 180.0;
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
    for (int y = 0; y < rect.h; ++y) {
        float t = static_cast<float>(y) / rect.h;
        Uint8 r = static_cast<Uint8>(top.r + t * (bottom.r - top.r));
        Uint8 g = static_cast<Uint8>(top.g + t * (bottom.g - top.g));
        Uint8 b = static_cast<Uint8>(top.b + t * (bottom.b - top.b));
        Uint8 a = static_cast<Uint8>(top.a + t * (bottom.a - top.a));
        SDL_SetRenderDrawColor(m_renderer, r, g, b, a);
        SDL_RenderDrawLine(m_renderer, rect.x, rect.y + y, rect.x + rect.w - 1, rect.y + y);
    }
}

void MainUI::renderAuraBackground() {
    // Deep Base
    SDL_Rect screenRect = {0, 0, m_screenWidth, m_screenHeight};
    SDL_SetRenderDrawColor(m_renderer, 10, 15, 28, 255);
    SDL_RenderFillRect(m_renderer, &screenRect);

    // Simulated Web Mesh Gradient / Blurred Orbs
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    
    // Top-Left Orb (Primary Accent)
    for (int i = 0; i < 500; i += 5) {
        Uint8 alpha = static_cast<Uint8>(35.0f * (1.0f - std::pow(static_cast<float>(i) / 500.0f, 2.0f)));
        SDL_Rect lightRect = {-100 + i/2, -100 + i/2, 800 - i, 800 - i};
        renderRoundedRect(lightRect, (800 - i)/2, SDL_Color{m_colors.Accent.r, m_colors.Accent.g, m_colors.Accent.b, alpha});
    }

    // Bottom-Right Orb (Secondary Tone - Purple/Indigo feel)
    for (int i = 0; i < 600; i += 6) {
        Uint8 alpha = static_cast<Uint8>(25.0f * (1.0f - std::pow(static_cast<float>(i) / 600.0f, 2.0f)));
        SDL_Rect lightRect = {m_screenWidth - 500 + i/2, m_screenHeight - 400 + i/2, 900 - i, 900 - i};
        renderRoundedRect(lightRect, (900 - i)/2, SDL_Color{124, 58, 237, alpha}); // Indigo 600
    }
}

void MainUI::renderGlassPanel(const SDL_Rect& rect, int radius, SDL_Color baseColor, bool hasRimLight) {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    
    // Deep web-like soft shadow
    renderSoftShadow(rect, radius, 24, SDL_Color{0, 0, 0, 140}, 12);

    // Glass Base
    renderFilledRoundedRect(rect, radius, baseColor);

    // Rim Light (Top Edge Highlight)
    if (hasRimLight) {
        SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 60);
        SDL_RenderDrawLine(m_renderer, rect.x + radius, rect.y, rect.x + rect.w - radius, rect.y);
        // Soft corner highlights
        SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 30);
        SDL_RenderDrawPoint(m_renderer, rect.x + radius - 1, rect.y + 1);
        SDL_RenderDrawPoint(m_renderer, rect.x + rect.w - radius, rect.y + 1);
    }
    
    // Subtle Inner Border (Glass reflection)
    renderRoundedRect(rect, radius, SDL_Color{255, 255, 255, 18});
    
    // Inner dark shadow for 3D depth (Inset shadow)
    SDL_Rect inset = {rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2};
    renderRoundedRect(inset, radius, SDL_Color{0, 0, 0, 20});
}

void MainUI::renderCard(const GameCard& card, float unused_scale) {
    SDL_Rect rect = card.rect;
    float scale = 1.0f;
    int yOffset = 0;

    if (card.selected) {
        // Strict scale limit to prevent overflowing into header or sides
        scale = 1.0f + (m_selectionScale - 1.0f) * 0.8f; 
        yOffset = -10; // Subtle lift
        
        rect.w = static_cast<int>(rect.w * scale);
        rect.h = static_cast<int>(rect.h * scale);
        rect.x -= (rect.w - card.rect.w) / 2;
        rect.y -= (rect.h - card.rect.h) / 2;
        rect.y += yOffset;

        // Screen boundary safety check
        if (rect.x < 16) rect.x = 16;
        if (rect.x + rect.w > m_screenWidth - 16) rect.x = m_screenWidth - 16 - rect.w;
    }

    const int borderRadius = 20; // More organic rounding
    
    // Premium Glass Material with Web-like contrast
    SDL_Color glassColor = card.selected ? SDL_Color{255, 255, 255, 20} : SDL_Color{30, 41, 59, 140};
    if (card.selected) {
        // Render large soft glow behind the selected card
        renderSelectionGlow(rect);
    }
    
    renderGlassPanel(rect, borderRadius, glassColor, true);

    const int padding = 12;
    const int iconSize = rect.w - padding * 2;
    SDL_Rect iconRect = {rect.x + padding, rect.y + padding, iconSize, iconSize};
    
    // Icon Area
    SDL_Texture* iconTexture = loadIcon(card.title->iconPath);
    if (iconTexture) {
        SDL_RenderCopy(m_renderer, iconTexture, nullptr, &iconRect);
        SDL_DestroyTexture(iconTexture);
    } else {
        renderFilledRoundedRect(iconRect, 18, SDL_Color{15, 23, 42, 255});
        renderTextCentered("?", iconRect.x, iconRect.y + (iconRect.h / 2) - 20, iconRect.w, m_fontLarge, m_colors.TextDim);
    }
    // Deep inset shadow on icon to make it pop
    renderRoundedRect(iconRect, 18, SDL_Color{0, 0, 0, 60});
    renderRoundedRect(iconRect, 18, SDL_Color{255, 255, 255, 20}); // Subtle rim on icon

    // Content Area with shadow for high contrast
    int textY = iconRect.y + iconRect.h + 16;
    renderTextWithShadow(fitText(m_fontMedium, card.title->name, rect.w - padding * 2), rect.x + padding + 4, textY, m_fontMedium, m_colors.Text);

    if (!card.syncLabel.empty()) {
        SDL_Color statusColor = card.selected ? m_colors.Accent : m_colors.TextDim;
        renderTextWithShadow(fitText(m_fontSmall, card.syncLabel, rect.w - padding * 2), rect.x + padding + 4, textY + 32, m_fontSmall, statusColor);
    }

    if (card.synced) {
        renderSyncBadge(rect.x + rect.w - 32, rect.y + 14, true);
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
    if (!m_selectedTitle) return;
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 140); // Deep overlay
    SDL_RenderFillRect(m_renderer, nullptr);

    const int sideWidth = std::min(760, std::max(680, (m_screenWidth * 12) / 20));
    SDL_Rect sideRect{m_screenWidth - sideWidth - 32, 48, sideWidth, m_screenHeight - 96};
    
    // Premium Detail Glass
    renderGlassPanel(sideRect, 32, SDL_Color{30, 41, 59, 230}, true);

    SDL_Rect posterRect{sideRect.x + 40, sideRect.y + 40, 144, 144};
    if (SDL_Texture* posterTexture = loadIcon(m_selectedTitle->iconPath)) {
        SDL_RenderCopy(m_renderer, posterTexture, nullptr, &posterRect);
        SDL_DestroyTexture(posterTexture);
    }
    renderRoundedRect(posterRect, 20, SDL_Color{255, 255, 255, 30});

    const int infoX = posterRect.x + posterRect.w + 32;
    const int infoW = sideRect.x + sideRect.w - infoX - 40;
    renderText(fitText(m_fontLarge, m_selectedTitle->name, infoW), infoX, sideRect.y + 54, m_fontLarge, m_colors.Text);
    renderText(fitText(m_fontMedium, m_selectedTitle->publisher, infoW), infoX, sideRect.y + 108, m_fontMedium, m_colors.TextDim);

    // Tags
    auto drawTag = [&](const std::string& text, int x, int y, SDL_Color color) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(m_fontSmall, text.c_str(), &tw, &th);
        SDL_Rect tag{x, y, tw + 28, 34};
        renderGlassPanel(tag, 17, SDL_Color{color.r, color.g, color.b, 40}, false);
        renderText(text, tag.x + 14, tag.y + (tag.h - th)/2, m_fontSmall, color);
        return tag.w;
    };

    int tagX = infoX;
    tagX += drawTag(formatStorageSize(m_selectedTitle->saveSize), tagX, sideRect.y + 154, m_colors.Accent) + 12;
    drawTag("STABLE", tagX, sideRect.y + 154, m_colors.Synced);

    const auto versions = m_saveManager.getBackupVersions(m_selectedTitle);
    const std::string backupCount = versions.empty() ? LANG("detail.no_backup") : std::to_string(versions.size()) + " " + LANG("detail.versions");

    auto drawRow = [&](const std::string& label, const std::string& value, int y) {
        SDL_Rect row{sideRect.x + 40, y, sideRect.w - 80, 84};
        renderGlassPanel(row, 16, SDL_Color{255, 255, 255, 8}, false);
        renderText(label, row.x + 24, row.y + 16, m_fontSmall, m_colors.TextDim);
        renderText(fitText(m_fontMedium, value, row.w - 48), row.x + 24, row.y + 44, m_fontMedium, m_colors.Text);
    };

    int rowY = sideRect.y + 220;
    drawRow(LANG("detail.recent_backup"), backupCount, rowY); rowY += 100;
    drawRow(LANG("detail.latest_device"), versions.empty() ? m_saveManager.getDeviceLabel() : versions.front().deviceLabel, rowY); rowY += 100;
    drawRow(LANG("detail.latest_user"), versions.empty() ? LANG("history.unknown_user") : versions.front().userName, rowY);

    m_buttons.clear();
    const int btnW = (sideRect.w - 80 - 24) / 2;
    const int btnH = 56;
    const int startY = sideRect.y + sideRect.h - 160;
    
    m_buttons.emplace_back(sideRect.x + 40, startY, btnW, btnH, LANG("detail.upload"));
    m_buttons.emplace_back(sideRect.x + 40 + btnW + 24, startY, btnW, btnH, LANG("detail.download"));
    m_buttons.emplace_back(sideRect.x + 40, startY + 72, btnW, btnH, LANG("detail.backup"));
    m_buttons.emplace_back(sideRect.x + 40 + btnW + 24, startY + 72, btnW, btnH, LANG("detail.history"));
    
    // Close button (Extreme Minimalism)
    m_buttons.emplace_back(sideRect.x + sideRect.w - 120, sideRect.y + 24, 80, 40, "X");

    for (size_t i = 0; i < m_buttons.size(); ++i) {
        auto& btn = m_buttons[i];
        btn.hover = static_cast<int>(i) == m_selectedButtonIndex;
        renderButton(btn);
    }
}

void MainUI::renderButton(const Button& btn) {
    const bool isClose = btn.text == "X" || btn.text == closeLabel();
    const bool isPrimary = btn.text == LANG("detail.upload") ||
                           btn.text == LANG("detail.download") ||
                           btn.text == LANG("auth.connect");
    
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    
    SDL_Color bgColor;
    SDL_Color textColor;

    if (isClose) {
        bgColor = btn.hover ? m_colors.AccentSoft : m_colors.Card;
        textColor = m_colors.TextDim;
    } else if (isPrimary) {
        bgColor = btn.hover ? SDL_Color{56, 189, 248, 255} : m_colors.Accent;
        textColor = SDL_Color{255, 255, 255, 255};
    } else {
        bgColor = btn.hover ? m_colors.AccentSoft : m_colors.Card;
        textColor = m_colors.Text;
    }

    const int radius = btn.rect.h / 2;
    
    if (btn.hover) {
        // CSS Box-shadow hover effect
        renderSoftShadow(btn.rect, radius, 16, SDL_Color{bgColor.r, bgColor.g, bgColor.b, 100}, 4);
        
        // Slight scale up on hover simulation (just by drawing it 1px larger or shifting)
        SDL_Rect hoverRect = {btn.rect.x, btn.rect.y - 1, btn.rect.w, btn.rect.h};
        renderGlassPanel(hoverRect, radius, SDL_Color{bgColor.r, bgColor.g, bgColor.b, 200}, true);
        
        TTF_Font* font = m_fontMedium;
        int textW, textH;
        TTF_SizeUTF8(font, btn.text.c_str(), &textW, &textH);
        if (textW > btn.rect.w - 24) {
            font = m_fontSmall;
            TTF_SizeUTF8(font, btn.text.c_str(), &textW, &textH);
        }
        renderTextWithShadow(fitText(font, btn.text, btn.rect.w - 16),
                           hoverRect.x + (hoverRect.w - textW) / 2, hoverRect.y + (hoverRect.h - textH) / 2,
                           font, textColor);
    } else {
        // Default state
        renderGlassPanel(btn.rect, radius, SDL_Color{bgColor.r, bgColor.g, bgColor.b, 160}, true);
        
        TTF_Font* font = m_fontMedium;
        int textW, textH;
        TTF_SizeUTF8(font, btn.text.c_str(), &textW, &textH);
        if (textW > btn.rect.w - 24) {
            font = m_fontSmall;
            TTF_SizeUTF8(font, btn.text.c_str(), &textW, &textH);
        }
        renderTextWithShadow(fitText(font, btn.text, btn.rect.w - 16),
                           btn.rect.x + (btn.rect.w - textW) / 2, btn.rect.y + (btn.rect.h - textH) / 2,
                           font, textColor);
    }
}

void MainUI::renderAuthScreen() {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(m_renderer, nullptr);
    
    int panelWidth = std::min(m_screenWidth - 120, 1000);
    int panelHeight = std::min(m_screenHeight - 120, 580);
    SDL_Rect authRect = {(m_screenWidth - panelWidth) / 2, (m_screenHeight - panelHeight) / 2, panelWidth, panelHeight};
    
    renderFilledRoundedRect(SDL_Rect{authRect.x+6, authRect.y+8, authRect.w, authRect.h}, 24, m_colors.Shadow);
    renderFilledRoundedRect(authRect, 24, m_colors.Card);
    renderRoundedRect(authRect, 24, m_colors.Border);
    
    renderTextCentered(LANG("auth.title"), authRect.x, authRect.y + 30, authRect.w, m_fontLarge, m_colors.Accent);

    int left = authRect.x + 60;
    int y = authRect.y + 100;

    renderText("PKCE setup (one-time)", left, y, m_fontMedium, m_colors.Warning);
    y += 44;
    renderText("1. Press Connect to generate an authorization URL", left, y, m_fontMedium, m_colors.Text);
    y += 38;
    renderText("2. Open the URL on your phone and approve access", left, y, m_fontMedium, m_colors.Text);
    y += 38;
    renderText("3. Copy the code from redirected URL (?code=...)", left, y, m_fontMedium, m_colors.Text);
    y += 38;
    renderText("4. Press A or tap the box and paste code (or full URL)", left, y, m_fontMedium, m_colors.Accent);
    y += 50;

    if (!m_authUrl.empty()) {
        const std::string authUrlText = fitText(m_fontSmall, m_authUrl, authRect.w - 120);
        renderText(authUrlText, left, y, m_fontSmall, m_colors.Warning);
        y += 34;
    }

    m_authTokenBox = {left, y, authRect.w - 120, 64};
    renderFilledRoundedRect(m_authTokenBox, 12, m_colors.AccentSoft);
    renderRoundedRect(m_authTokenBox, 12, m_colors.Border);
    
    if (m_authToken.empty()) {
        renderText("Paste authorization code or redirected URL", m_authTokenBox.x + 20, y + 18, m_fontSmall, m_colors.TextDim);
    } else {
        std::string preview = m_authToken;
        if (preview.size() > 60) preview = preview.substr(0, 57) + "...";
        renderText(preview, m_authTokenBox.x + 20, y + 18, m_fontSmall, m_colors.Text);
    }
    y += 90;
    
    m_buttons.clear();
    int btnW = 220;
    int btnH = 48;
    int gap = 24;
    int totalWidth = btnW * 2 + gap;
    int startBtnX = authRect.x + (authRect.w - totalWidth) / 2;
    m_buttons.emplace_back(startBtnX, y, btnW, btnH, LANG("auth.connect"));
    m_buttons.emplace_back(startBtnX + btnW + gap, y, btnW, btnH, LANG("auth.cancel"));
    
    if (m_selectedButtonIndex < 0 || m_selectedButtonIndex >= (int)m_buttons.size()) {
        m_selectedButtonIndex = 0;
    }
    
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

    const int panelWidth = 380;
    const int panelHeight = std::min(120 + static_cast<int>(m_saveManager.getUsers().size()) * 80, m_screenHeight - 160);
    SDL_Rect panel{m_screenWidth - panelWidth - 30, m_screenHeight - FOOTER_HEIGHT - panelHeight - 10, panelWidth, panelHeight};
    
    renderFilledRoundedRect(SDL_Rect{panel.x+6, panel.y+8, panel.w, panel.h}, 24, m_colors.Shadow);
    renderFilledRoundedRect(panel, 24, m_colors.Card);
    renderRoundedRect(panel, 24, m_colors.Border);

    renderText(utils::Language::instance().currentLang() == "ko" ? "사용자 전환" : "Switch User",
               panel.x + 24, panel.y + 24, m_fontMedium, m_colors.Text);

    m_userChips.clear();
    int y = panel.y + 70;
    const auto& users = m_saveManager.getUsers();
    for (size_t i = 0; i < users.size(); ++i) {
        UserChip chip;
        chip.user = const_cast<core::UserInfo*>(&users[i]);
        chip.selected = static_cast<int>(i) == m_selectedUserIndex;
        chip.rect = {panel.x + 20, y, panel.w - 40, 68};
        m_userChips.push_back(chip);

        SDL_Color chipBg = chip.selected ? m_colors.AccentSoft : m_colors.Poster;
        renderFilledRoundedRect(chip.rect, 16, chipBg);
        renderRoundedRect(chip.rect, 16, chip.selected ? m_colors.Accent : m_colors.Border);

        SDL_Rect avatarRect{chip.rect.x + 12, chip.rect.y + 10, 48, 48};
        bool renderedAvatar = false;
        if (!chip.user->iconPath.empty()) {
            if (SDL_Texture* avatarTexture = loadIcon(chip.user->iconPath)) {
                SDL_RenderCopy(m_renderer, avatarTexture, nullptr, &avatarRect);
                SDL_DestroyTexture(avatarTexture);
                renderedAvatar = true;
            }
        }
        if (!renderedAvatar) {
            renderFilledRoundedRect(avatarRect, 24, m_colors.Border);
        }

        renderText(fitText(m_fontMedium, chip.user->name, chip.rect.w - 100), chip.rect.x + 72, chip.rect.y + 12, m_fontMedium, m_colors.Text);
        renderText(fitText(m_fontSmall, chip.user->id, chip.rect.w - 100), chip.rect.x + 72, chip.rect.y + 38, m_fontSmall, m_colors.TextDim);
        y += 76;
    }
}

void MainUI::renderVersionHistory() {
    const bool isCloudPicker = m_state == State::CloudPicker;
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 160);
    SDL_RenderFillRect(m_renderer, nullptr);

    const int listWidth = std::min(720, std::max(600, (m_screenWidth * 11) / 20));
    SDL_Rect listRect{m_screenWidth - listWidth - 20, HEADER_HEIGHT + 20, listWidth, m_screenHeight - HEADER_HEIGHT - FOOTER_HEIGHT - 40};
    
    // Shadow and Background
    renderFilledRoundedRect(SDL_Rect{listRect.x+6, listRect.y+8, listRect.w, listRect.h}, 24, m_colors.Shadow);
    renderFilledRoundedRect(listRect, 24, m_colors.Card);
    renderRoundedRect(listRect, 24, m_colors.Border);

    renderText(isCloudPicker ? LANG("detail.download") : LANG("history.title"), listRect.x + 32, listRect.y + 24, m_fontLarge, m_colors.Text);
    if (m_selectedTitle) {
        renderText(fitText(m_fontSmall, m_selectedTitle->name, listRect.w - 64), listRect.x + 32, listRect.y + 68, m_fontSmall, m_colors.TextDim);
    }

    int y = listRect.y + 110;
    for (size_t i = 0; i < m_versionItems.size() && i < 5; i++) {
        VersionItem& item = m_versionItems[i];
        item.selected = ((int)i == m_selectedVersionIndex);
        item.rect = {listRect.x + 24, y, listRect.w - 48, 88};

        SDL_Color itemBg = item.selected ? m_colors.AccentSoft : m_colors.Poster;
        renderFilledRoundedRect(item.rect, 16, itemBg);
        renderRoundedRect(item.rect, 16, item.selected ? m_colors.Accent : m_colors.Border);

        const std::string sizeLabel = formatStorageSize(static_cast<int64_t>(item.size));
        const std::string deviceLabel = item.deviceLabel.empty()
            ? (item.isLocal ? m_saveManager.getDeviceLabel() : std::string(LANG("history.unknown_device")))
            : item.deviceLabel;
        const std::string sourceLabel = item.sourceLabel.empty()
            ? (item.isLocal ? std::string("LOCAL") : std::string("CLOUD"))
            : item.sourceLabel;

        // Source Badge
        int badgeW = sourceLabel == "LOCAL" ? 70 : 80;
        SDL_Rect sourceBadge{item.rect.x + 20, item.rect.y + 50, badgeW, 24};
        SDL_Color badgeColor = item.isLocal ? m_colors.Synced : m_colors.Accent;
        renderFilledRoundedRect(sourceBadge, 12, SDL_Color{badgeColor.r, badgeColor.g, badgeColor.b, 40});
        renderRoundedRect(sourceBadge, 12, badgeColor);
        renderTextCentered(sourceLabel, sourceBadge.x, sourceBadge.y + 2, sourceBadge.w, m_fontSmall, badgeColor);

        renderText(fitText(m_fontMedium, item.name, item.rect.w - 40), item.rect.x + 20, item.rect.y + 12, m_fontMedium, m_colors.Text);
        renderText(fitText(m_fontSmall, deviceLabel, item.rect.w - 150), item.rect.x + 20 + badgeW + 15, item.rect.y + 52, m_fontSmall, m_colors.TextDim);
        
        int sizeWidth = 0, sizeHeight = 0;
        TTF_SizeUTF8(m_fontSmall, sizeLabel.c_str(), &sizeWidth, &sizeHeight);
        renderText(sizeLabel, item.rect.x + item.rect.w - 20 - sizeWidth, item.rect.y + 52, m_fontSmall, m_colors.TextDim);

        y += 100;
    }

    m_buttons.clear();
    SDL_Rect closeBtnRect = {listRect.x + listRect.w - 180, listRect.y + 24, 150, 40};
    m_buttons.emplace_back(closeBtnRect.x, closeBtnRect.y, closeBtnRect.w, closeBtnRect.h, closeLabel());
    
    Button closeBtn = m_buttons.front();
    closeBtn.hover = m_selectedButtonIndex == 0;
    renderButton(closeBtn);
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
    } else {
        m_syncStatus = "Dropbox upload failed";
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
        item.sourceLabel = v.source;
        item.timestamp = v.timestamp;
        item.size = v.size;
        item.isLocal = true;
        item.selected = false;
        m_versionItems.push_back(item);
    }
    
    m_selectedVersionIndex = 0;
    m_selectedButtonIndex = -1;
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
        std::snprintf(tempMeta, sizeof(tempMeta), "/switch/oc-save-keeper/temp/%016llX_%s",
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
    const auto& users = m_saveManager.getUsers();
    core::UserInfo* selectedUser = m_saveManager.getSelectedUser();
    for (size_t i = 0; i < users.size(); ++i) {
        if (selectedUser == &users[i]) {
            m_selectedUserIndex = static_cast<int>(i);
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
    std::snprintf(localZip, sizeof(localZip), "/switch/oc-save-keeper/temp/%016llX_selected.zip",
                  static_cast<unsigned long long>(m_selectedTitle->titleId));

    if (!m_dropbox.downloadFile(item->path, localZip)) {
        m_syncStatus = "Dropbox download failed";
        m_state = State::GameDetail;
        return;
    }

    std::string reason;
    if (m_saveManager.importBackupArchive(m_selectedTitle, localZip, &reason, false)) {
        m_syncStatus = reason.empty() ? "Download complete" : reason;
    } else {
        m_syncStatus = reason.empty() ? "Import failed" : reason;
    }

    remove(localZip);
    updateGameCards();
    refreshSyncStates();
    m_state = State::GameDetail;
}

void MainUI::restoreVersion(VersionItem* item) {
    if (!item || !m_selectedTitle) return;
    m_saveManager.restoreSave(m_selectedTitle, item->path);
    m_state = State::GameDetail;
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
    if (!m_authSessionStarted) {
        m_authUrl = m_dropbox.getAuthorizeUrl();
        if (m_authUrl.empty()) {
            m_syncStatus = "Failed to create authorize URL";
            return;
        }
        m_authSessionStarted = true;
        m_syncStatus = "Open URL on phone, then paste code";
        return;
    }

    if (m_authToken.empty()) {
        m_syncStatus = "Paste authorization code first";
        return;
    }

    if (m_dropbox.exchangeAuthorizationCode(m_authToken)) {
        m_syncStatus = "Dropbox connected";
        m_authSessionStarted = false;
        m_authUrl.clear();
        m_authToken.clear();
        updateGameCards();
        refreshSyncStates(true);
        m_state = State::Main;
    } else {
        m_syncStatus = "Auth code exchange failed";
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
    swkbdConfigSetHeaderText(&keyboard, "Dropbox authorization code");
    swkbdConfigSetSubText(&keyboard, "Paste code or redirected URL");
    swkbdConfigSetGuideText(&keyboard, "Code");
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

int MainUI::getItemsPerPage() const {
    return 12;
}

int MainUI::getVisibleStartIndex() const {
    return m_scrollRow * m_gridCols;
}

} // namespace ui
