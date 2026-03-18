#include "ui/saves/SaveShell.hpp"

#include "ui/saves/RevisionMenuScreen.hpp"
#include "ui/saves/Runtime.hpp"
#include "ui/saves/SaveBackendAdapter.hpp"
#include "ui/saves/SaveMenuScreen.hpp"
#include "ui/saves/Sidebar.hpp"
#include "ui/saves/Widget.hpp"

#include "core/SaveManager.hpp"
#include "network/Dropbox.hpp"
#include "utils/Language.hpp"
#include "utils/Logger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ui::saves {

namespace {

// Catppuccin Mocha Colors
static const SDL_Color CAT_BASE = {30, 30, 46, 255};
static const SDL_Color CAT_MANTLE = {24, 24, 37, 255};
static const SDL_Color CAT_CRUST = {17, 17, 27, 255};
static const SDL_Color CAT_TEXT = {205, 214, 244, 255};
static const SDL_Color CAT_SUBTEXT0 = {166, 173, 200, 255};
static const SDL_Color CAT_SURFACE0 = {49, 50, 68, 255};
static const SDL_Color CAT_SURFACE1 = {69, 71, 90, 255};
static const SDL_Color CAT_SURFACE2 = {88, 91, 112, 255};
static const SDL_Color CAT_OVERLAY0 = {108, 112, 134, 255};
static const SDL_Color CAT_BLUE = {137, 180, 250, 255};
static const SDL_Color CAT_SAPPHIRE = {116, 199, 236, 255};
static const SDL_Color CAT_SKY = {137, 220, 235, 255};
static const SDL_Color CAT_TEAL = {148, 226, 213, 255};
static const SDL_Color CAT_GREEN = {166, 227, 161, 255};
static const SDL_Color CAT_YELLOW = {249, 226, 175, 255};
static const SDL_Color CAT_PEACH = {250, 179, 135, 255};
static const SDL_Color CAT_MAROON = {235, 160, 172, 255};
static const SDL_Color CAT_RED = {243, 139, 168, 255};
static const SDL_Color CAT_MAUVE = {203, 166, 247, 255};
static const SDL_Color CAT_PINK = {245, 194, 231, 255};
static const SDL_Color CAT_FLAMINGO = {242, 205, 205, 255};
static const SDL_Color CAT_ROSEWATER = {245, 224, 220, 255};

SDL_Color color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    return SDL_Color{r, g, b, a};
}

void fillRect(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &rect);
}

void strokeRect(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(renderer, &rect);
}

void drawPanel(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color fill, SDL_Color border) {
    SDL_Rect shadow{rect.x + 4, rect.y + 5, rect.w, rect.h};
    fillRect(renderer, shadow, color(0, 0, 0, 70));
    fillRect(renderer, rect, fill);
    strokeRect(renderer, rect, border);
}

void drawFocus(SDL_Renderer* renderer, const SDL_Rect& rect) {
    SDL_Rect outer{rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4};
    SDL_Rect inner{rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4};
    strokeRect(renderer, outer, color(96, 165, 250));
    strokeRect(renderer, rect, color(125, 211, 252));
    strokeRect(renderer, inner, color(37, 99, 235));
}

std::string currentSelectionSubtitle(core::SaveManager& saveManager, network::Dropbox& dropbox, const utils::Language& lang) {
    const auto* user = saveManager.getSelectedUser();
    const std::string selected = user ? user->name : lang.get("ui.no_user");
    const std::string mode = user && user->id == "device"
        ? lang.get("ui.mode_device")
        : lang.get("ui.mode_user");
    
    // Display localized device label + ID instead of cloud status - removed colon
    const std::string device = lang.get("ui.device") + " " + saveManager.getDeviceLabel();
    
    return mode + "  |  " + selected + "  |  " + device;
}

#ifndef __SWITCH__
bool fileExists(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (!file) {
        return false;
    }
    std::fclose(file);
    return true;
}

const char* hostFontPath() {
    static const char* const candidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };

    for (const char* path : candidates) {
        if (fileExists(path)) {
            return path;
        }
    }
    return candidates[3];
}
#endif

#ifdef __SWITCH__
TTF_Font* openSharedFont(const PlFontData& font, int size) {
    SDL_RWops* rw = SDL_RWFromConstMem(font.address, static_cast<int>(font.size));
    if (!rw) {
        LOG_ERROR("SDL_RWFromConstMem failed for SaveShell font: %s", SDL_GetError());
        return nullptr;
    }

    TTF_Font* ttfFont = TTF_OpenFontRW(rw, 1, size);
    if (!ttfFont) {
        LOG_ERROR("TTF_OpenFontRW failed for SaveShell font: %s", TTF_GetError());
    }
    return ttfFont;
}
#endif

} // namespace

SaveShell::SaveShell(SDL_Renderer* renderer, network::Dropbox& dropbox, core::SaveManager& saveManager)
    : m_renderer(renderer)
    , m_dropbox(dropbox)
    , m_saveManager(saveManager) {}

SaveShell::~SaveShell() {
    for (auto& pair : m_iconCache) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    if (m_fontLarge) TTF_CloseFont(m_fontLarge);
    if (m_fontMedium) TTF_CloseFont(m_fontMedium);
    if (m_fontSmall) TTF_CloseFont(m_fontSmall);
    if (m_fontLargeFallback) TTF_CloseFont(m_fontLargeFallback);
    if (m_fontMediumFallback) TTF_CloseFont(m_fontMediumFallback);
    if (m_fontSmallFallback) TTF_CloseFont(m_fontSmallFallback);
#ifdef __SWITCH__
    if (m_plInitialized) {
        plExit();
    }
#endif
}

bool SaveShell::initialize() {
    if (TTF_WasInit() == 0 && TTF_Init() < 0) {
        LOG_ERROR("TTF_Init failed: %s", TTF_GetError());
        return false;
    }

#ifdef __SWITCH__
    if (R_FAILED(plInitialize(PlServiceType_User))) {
        LOG_ERROR("SaveShell plInitialize failed");
        return false;
    }
    m_plInitialized = true;

    PlFontData standardFont;
    if (R_FAILED(plGetSharedFontByType(&standardFont, PlSharedFontType_Standard))) {
        LOG_ERROR("SaveShell plGetSharedFontByType failed");
        plExit();
        m_plInitialized = false;
        return false;
    }

    PlFontData koreanFont{};
    const bool hasKoreanFont = R_SUCCEEDED(plGetSharedFontByType(&koreanFont, PlSharedFontType_KO));

    m_fontLarge = openSharedFont(standardFont, 34);
    m_fontMedium = openSharedFont(standardFont, 24);
    m_fontSmall = openSharedFont(standardFont, 18);
    if (hasKoreanFont) {
        m_fontLargeFallback = openSharedFont(koreanFont, 34);
        m_fontMediumFallback = openSharedFont(koreanFont, 24);
        m_fontSmallFallback = openSharedFont(koreanFont, 18);
    }
#else
    const char* fontPath = hostFontPath();
    m_fontLarge = TTF_OpenFont(fontPath, 34);
    m_fontMedium = TTF_OpenFont(fontPath, 24);
    m_fontSmall = TTF_OpenFont(fontPath, 18);
    m_fontLargeFallback = TTF_OpenFont(fontPath, 34);
    m_fontMediumFallback = TTF_OpenFont(fontPath, 24);
    m_fontSmallFallback = TTF_OpenFont(fontPath, 18);
#endif
    if (!m_fontLarge || !m_fontMedium || !m_fontSmall) {
        LOG_ERROR("SaveShell font initialization failed");
#ifdef __SWITCH__
        if (m_plInitialized) {
            plExit();
            m_plInitialized = false;
        }
#endif
        return false;
    }

#ifdef __SWITCH__
    m_isAppletMode = (appletGetAppletType() != AppletType_Application);
#endif

    utils::Language::instance().init();
    Runtime::instance().setFont(m_fontMedium);
    Runtime::instance().setShell(this);
    m_saveManager.scanTitles();
    m_backend = std::make_shared<SaveBackendAdapter>(m_saveManager, m_dropbox);
    pushRootScreen();
    return true;
}

void SaveShell::pushRootScreen() {
    m_rootScreen = std::make_shared<SaveMenuScreen>(m_backend);
    Runtime::instance().push(m_rootScreen);
}

void SaveShell::rebuildRootScreen() {
    Runtime::instance().popToMenu();
    Runtime::instance().pop();
    pushRootScreen();
}

void SaveShell::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            m_shouldExit = true;
            break;
        case SDL_JOYBUTTONDOWN:
            switch (event.jbutton.button) {
                case 0: setButtonDown(Button::A); break;
                case 1: setButtonDown(Button::B); break;
                case 2: setButtonDown(Button::X); break;
                case 3: setButtonDown(Button::Y); break;
                case 4: setButtonDown(Button::L); break;
                case 5: setButtonDown(Button::R); break;
                case 10: setButtonDown(Button::Plus); break;
                case 11: setButtonDown(Button::Minus); break;
                default: break;
            }
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
                case SDLK_RETURN: setButtonDown(Button::A); break;
                case SDLK_BACKSPACE:
                case SDLK_ESCAPE: setButtonDown(Button::B); break;
                case SDLK_x: setButtonDown(Button::X); break;
                case SDLK_y: setButtonDown(Button::Y); break;
                case SDLK_l: setButtonDown(Button::L); break;
                case SDLK_r: setButtonDown(Button::R); break;
                case SDLK_PLUS:
                case SDLK_EQUALS: setButtonDown(Button::Plus); break;
                case SDLK_MINUS: setButtonDown(Button::Minus); break;
                case SDLK_LEFT: setButtonDown(Button::Left); break;
                case SDLK_RIGHT: setButtonDown(Button::Right); break;
                case SDLK_UP: setButtonDown(Button::Up); break;
                case SDLK_DOWN: setButtonDown(Button::Down); break;
                default: break;
            }
            break;
        case SDL_TEXTINPUT:
            break;
        case SDL_MOUSEBUTTONDOWN:
            m_touch.x = event.button.x;
            m_touch.y = event.button.y;
            m_touch.isClicked = true;
            m_touch.isTap = true;
            m_touch.isTouching = true;
            break;
        case SDL_FINGERDOWN:
            m_touch.x = static_cast<int>(event.tfinger.x * 1280.0f);
            m_touch.y = static_cast<int>(event.tfinger.y * 720.0f);
            m_touch.isClicked = true;
            m_touch.isTap = true;
            m_touch.isTouching = true;
            break;
        default:
            break;
    }
}

#ifdef __SWITCH__
void SaveShell::handlePadButtons(u64 keysDown) {
    if (keysDown & HidNpadButton_A) setButtonDown(Button::A);
    if (keysDown & HidNpadButton_B) setButtonDown(Button::B);
    if (keysDown & HidNpadButton_X) setButtonDown(Button::X);
    if (keysDown & HidNpadButton_Y) setButtonDown(Button::Y);
    if (keysDown & HidNpadButton_L) setButtonDown(Button::L);
    if (keysDown & HidNpadButton_R) setButtonDown(Button::R);
    if (keysDown & HidNpadButton_Plus) setButtonDown(Button::Plus);
    if (keysDown & HidNpadButton_Minus) setButtonDown(Button::Minus);
    if (keysDown & (HidNpadButton_Up | HidNpadButton_StickLUp | HidNpadButton_StickRUp)) setButtonDown(Button::Up);
    if (keysDown & (HidNpadButton_Down | HidNpadButton_StickLDown | HidNpadButton_StickRDown)) setButtonDown(Button::Down);
    if (keysDown & (HidNpadButton_Left | HidNpadButton_StickLLeft | HidNpadButton_StickRLeft)) setButtonDown(Button::Left);
    if (keysDown & (HidNpadButton_Right | HidNpadButton_StickLRight | HidNpadButton_StickRRight)) setButtonDown(Button::Right);
}
#endif

void SaveShell::setButtonDown(Button button) {
    m_controller.down |= static_cast<u64>(button);
}

void SaveShell::setStatus(const std::string& message) {
    m_statusMessage = message;
    m_statusTime = static_cast<u64>(std::time(nullptr));
}

void SaveShell::update() {
    u64 now = static_cast<u64>(std::time(nullptr));
    // Pull notifications from runtime
    if (Runtime::instance().hasNotification()) {
        setStatus(Runtime::instance().consumeNotification());
    }

    // Auto-clear status after 3 seconds (only for non-essential info in main view)
    if (!m_statusMessage.empty() && now - m_statusTime >= 3 && m_overlay == Overlay::None) {
        m_statusMessage.clear();
    }

    if (m_overlay != Overlay::None) {
        updateOverlay();
        resetInput();
        return;
    }

    if (m_controller.gotDown(Button::Y)) {
        auto& lang = utils::Language::instance();
        const std::string newLang = (lang.currentLang() == "ko") ? "en" : "ko";
        lang.load(newLang);
        setStatus(tr("ui.language_changed", "Language changed"));
        resetInput();
        return;
    }
    if (m_controller.gotDown(Button::L)) {
        openUserPicker();
        resetInput();
        return;
    }
    if (m_controller.gotDown(Button::R)) {
        openDropboxOverlay();
        resetInput();
        return;
    }

    dispatchCurrent();
    resetInput();
}

void SaveShell::dispatchCurrent() {
    auto current = Runtime::instance().current();
    if (!current) {
        m_shouldExit = true;
        return;
    }

    if (current->kind() != ObjectKind::Widget &&
        current->kind() != ObjectKind::SaveMenuScreen &&
        current->kind() != ObjectKind::RevisionMenuScreen) {
        return;
    }

    auto* widget = static_cast<Widget*>(current.get());
    if (!widget) {
        return;
    }

    widget->update(m_controller, m_touch);
    if (widget->shouldPop()) {
        Runtime::instance().pop();
        if (!Runtime::instance().current()) {
            m_shouldExit = true;
        }
    }
}

void SaveShell::resetInput() {
    m_controller = {};
    m_touch = {};
}

void SaveShell::showProcessingOverlay(const std::string& message) {
    // 1. Draw current screen as background
    render();

    // 2. Semi-transparent dark layer
    SDL_Rect fullScreen = {0, 0, 1280, 720};
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 150); 
    SDL_RenderFillRect(m_renderer, &fullScreen);

    // 3. Status box in center
    SDL_Rect box = {1280/2 - 300, 720/2 - 60, 600, 120};
    drawPanel(m_renderer, box, color(15, 23, 42, 255), color(56, 189, 248, 255));
    
    // 4. Progress text
    renderTextCentered(message, box, m_fontMedium, color(241, 245, 249, 255));

    // 5. Present immediately so user sees it while the CPU is busy
    SDL_RenderPresent(m_renderer);
}

void SaveShell::render() {
    auto current = Runtime::instance().current();
    if (!current) {
        return;
    }

    SDL_SetRenderDrawColor(m_renderer, 10, 16, 28, 255);
    SDL_RenderClear(m_renderer);

    switch (current->kind()) {
        case ObjectKind::SaveMenuScreen:
            renderSaveMenu(*static_cast<SaveMenuScreen*>(current.get()));
            break;
        case ObjectKind::RevisionMenuScreen:
            renderRevisionMenu(*static_cast<RevisionMenuScreen*>(current.get()));
            break;
        default:
            break;
    }

    if (m_overlay == Overlay::UserPicker) {
        renderUserPickerOverlay();
    } else if (m_overlay == Overlay::DropboxAuth) {
        renderDropboxOverlay();
    }

    if (Runtime::instance().isLoading()) {
        const std::string& msg = Runtime::instance().loadingMessage();
        
        // Darken screen
        SDL_Rect shade{0, 0, 1280, 720};
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 2, 6, 14, 170);
        SDL_RenderFillRect(m_renderer, &shade);

        // Status box
        SDL_Rect box = {1280/2 - 300, 720/2 - 60, 600, 120};
        drawPanel(m_renderer, box, color(15, 23, 42, 255), color(56, 189, 248, 255));
        renderTextCentered(msg, box, m_fontMedium, color(241, 245, 249, 255));
    }
}

void SaveShell::renderHeader(const std::string& title, const std::string& subtitle) {
    SDL_Rect header{0, 0, 1280, 96};
    fillRect(m_renderer, header, color(15, 23, 42));
    SDL_SetRenderDrawColor(m_renderer, 56, 189, 248, 255);
    SDL_Rect accent{24, 24, 8, 40};
    SDL_RenderFillRect(m_renderer, &accent);
    
    // Main App Name - Moved down slightly (18 -> 24)
    renderText(tr("app.name", "OC Save Keeper"), 46, 24, m_fontLarge, color(241, 245, 249));
    
    // Screen Title next to it - smaller font and moved right/down
    if (!title.empty()) {
        renderText(title, 350, 36, m_fontSmall, color(148, 163, 184));
    }

    if (!subtitle.empty()) {
        renderText(subtitle, 46, 64, m_fontSmall, color(148, 163, 184));
    }

    const int chipW = 180;
    const int chipX = 1280 - 24 - chipW; // 1076
    const int chipY = 33; // Centered vertically (96/2 - 30/2 = 33)

    if (m_isAppletMode) {
        renderStatusChip(tr("app.applet_warning_title", "Applet Mode"),
                         chipX - (chipW + 12), // 884
                         chipY,
                         chipW,
                         color(185, 28, 28), // Brighter red background (Red-700)
                         color(248, 113, 113));
    }

    renderStatusChip(tr(m_dropbox.isAuthenticated() ? "status.connected" : "status.disconnected",
                        m_dropbox.isAuthenticated() ? "Connected" : "Not connected"),
                     chipX,
                     chipY,
                     chipW,
                     m_dropbox.isAuthenticated() ? color(20, 83, 45) : color(69, 26, 26),
                     m_dropbox.isAuthenticated() ? color(74, 222, 128) : color(248, 113, 113));
    SDL_SetRenderDrawColor(m_renderer, 51, 65, 85, 255);
    SDL_RenderDrawLine(m_renderer, 0, header.h - 1, 1280, header.h - 1);
}

void SaveShell::renderFooter(const std::string& leftHint, const std::string& rightHint) {
    SDL_Rect footer{0, 676, 1280, 44};
    fillRect(m_renderer, footer, color(15, 23, 42));
    SDL_SetRenderDrawColor(m_renderer, 51, 65, 85, 255);
    SDL_RenderDrawLine(m_renderer, 0, footer.y, 1280, footer.y);
    renderText(leftHint, 24, footer.y + 10, m_fontSmall, color(148, 163, 184));
    
    if (!rightHint.empty()) {
        const int maxW = 800;
        const std::string fitted = fitText(m_fontSmall, rightHint, maxW);
        int textW = 0;
        TTF_SizeText(m_fontSmall, fitted.c_str(), &textW, nullptr);
        const int x = std::max(24, 1280 - 24 - textW);
        renderText(fitted, x, footer.y + 10, m_fontSmall, color(100, 116, 139));
    }
}

void SaveShell::renderStatusChip(const std::string& text, int x, int y, int w, SDL_Color fill, SDL_Color border) {
    SDL_Rect chip{x, y, w, 30};
    fillRect(m_renderer, chip, fill);
    strokeRect(m_renderer, chip, border);
    renderTextCentered(text, chip, m_fontSmall, color(241, 245, 249));
}

void SaveShell::renderSaveMenu(const SaveMenuScreen& screen) {
    const auto& lang = utils::Language::instance();
    renderHeader(tr("ui.save_browser", "Save Browser"), currentSelectionSubtitle(m_saveManager, m_dropbox, lang));

    const auto& entries = screen.entries();
    const int selected = screen.index();
    const int firstVisible = screen.firstVisibleIndex();
    const int lastVisible = std::min(static_cast<int>(entries.size()), firstVisible + std::max(1, screen.visibleCount()));
    const int cols = 3;
    const int frameX = 0;
    const int frameY = 96;
    const int frameH = 580;
    const int frameW = 1280;
    const int gap = 16;
    const int innerPad = 20;
    const int cardW = (frameW - innerPad * 2 - gap * (cols - 1)) / cols;
    const int cardH = 150; // Reduced from 160 to 150
    const int startX = frameX + innerPad;
    const int startY = frameY + innerPad;

    SDL_Rect contentFrame{frameX, frameY, frameW, frameH};
    fillRect(m_renderer, contentFrame, color(9, 15, 26, 245));

    renderText(lang.get("ui.section_titles"), frameX + 24, frameY + 10, m_fontSmall, color(100, 116, 139));
    if (!m_statusMessage.empty() && m_overlay == Overlay::None && !screen.sidebar()) {
        renderText(fitText(m_fontSmall, m_statusMessage, 1000), frameX + 240, frameY + 10, m_fontSmall, color(125, 211, 252));
    }

    for (int i = firstVisible; i < lastVisible; ++i) {
        const auto& entry = entries[i];
        const int visibleIndex = i - firstVisible;
        const int row = visibleIndex / cols;
        const int col = visibleIndex % cols;
        SDL_Rect card{startX + col * (cardW + gap), startY + row * (cardH + gap) + 30, cardW, cardH};
        const bool isSelected = i == selected;

        drawPanel(m_renderer, card, isSelected ? color(19, 42, 79) : color(17, 24, 39), color(51, 65, 85));
        if (isSelected) {
            drawFocus(m_renderer, card);
        }

        const int iconSize = 90; // Reduced from 96 to match 150 height
        SDL_Rect iconRect{card.x + 16, card.y + (card.h - iconSize) / 2, iconSize, iconSize};
        SDL_Rect iconFrame{iconRect.x - 1, iconRect.y - 1, iconRect.w + 2, iconRect.h + 2};
        fillRect(m_renderer, iconFrame, color(8, 12, 22));
        if (SDL_Texture* icon = loadIcon(entry.iconPath)) {
            SDL_RenderCopy(m_renderer, icon, nullptr, &iconRect);
        } else {
            fillRect(m_renderer, iconRect, color(15, 23, 42));
            renderText("?", iconRect.x + 38, iconRect.y + 28, m_fontMedium, color(148, 163, 184));
        }
        strokeRect(m_renderer, iconFrame, color(51, 65, 85));

        const int textX = iconRect.x + iconRect.w + 20;
        const int textW = card.x + card.w - textX - 16;
        renderText(fitText(m_fontMedium, entry.name, textW), textX, card.y + 12, m_fontMedium, color(241, 245, 249));
        renderText(fitText(m_fontSmall, entry.author, textW), textX, card.y + 46, m_fontSmall, color(148, 163, 184));
        renderText(fitText(m_fontSmall, entry.subtitle, textW), textX, card.y + 70, m_fontSmall, color(148, 163, 184));

        const int chipGap = 10;
        const int chipW = std::max(80, (textW - chipGap) / 2);
        SDL_Rect localChip{textX, card.y + 104, chipW, 28};
        SDL_Rect cloudChip{textX + chipW + chipGap, card.y + 104, chipW, 28};
        fillRect(m_renderer, localChip, entry.hasLocalBackup ? color(20, 83, 45) : color(39, 39, 42));
        fillRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(8, 47, 73) : color(39, 39, 42));
        strokeRect(m_renderer, localChip, entry.hasLocalBackup ? color(74, 222, 128) : color(82, 82, 91));
        strokeRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(56, 189, 248) : color(82, 82, 91));
        renderTextCentered(fitText(m_fontSmall, entry.hasLocalBackup ? tr("history.local", "Local") : tr("ui.empty", "Empty"), chipW - 8),
                           localChip,
                           m_fontSmall,
                           color(241, 245, 249));
        renderTextCentered(fitText(m_fontSmall, entry.hasCloudBackup ? tr("ui.cloud_ready", "Cloud") : tr("ui.cloud_need_connect", "Connect"), chipW - 8),
                           cloudChip,
                           m_fontSmall,
                           color(241, 245, 249));
    }

    if (screen.sidebar()) {
        renderSidebar(*screen.sidebar());
    }

    renderFooter(tr("footer.hint.main", "A: Open  B: Exit  X: Refresh  Y: Language  L: Users"));
}

void SaveShell::renderRevisionMenu(const RevisionMenuScreen& screen) {
    renderHeader(screen.shortTitle(), screen.titleLabel());
    const auto& entries = screen.entries();
    const int selected = screen.index();
    const int firstVisible = screen.firstVisibleIndex();
    const int lastVisible = std::min(static_cast<int>(entries.size()), firstVisible + std::max(1, screen.visibleCount()));

    SDL_Rect listRect{0, 96, 1280, 580};
    fillRect(m_renderer, listRect, color(9, 15, 26, 245));

    renderText(tr("ui.revision", "Revision"), 32, 110, m_fontSmall, color(100, 116, 139));
    renderText(tr("ui.user", "User"), 380, 110, m_fontSmall, color(100, 116, 139));
    renderText(tr("ui.device", "Device"), 620, 110, m_fontSmall, color(100, 116, 139));
    renderText(tr("ui.source", "Source"), 900, 110, m_fontSmall, color(100, 116, 139));
    renderText(tr("ui.size", "Size"), 1080, 110, m_fontSmall, color(100, 116, 139));

    for (int i = firstVisible; i < lastVisible; ++i) {
        const auto& entry = entries[i];
        const int visibleIndex = i - firstVisible;
        SDL_Rect row{20, 140 + visibleIndex * 62, 1240, 50};
        const bool isSelected = i == selected;
        fillRect(m_renderer, row, isSelected ? color(19, 42, 79) : color(17, 24, 39));
        strokeRect(m_renderer, row, color(51, 65, 85));
        if (isSelected) {
            drawFocus(m_renderer, row);
        }

        char sizeBuf[32];
        std::snprintf(sizeBuf, sizeof(sizeBuf), "%.1f MB", entry.size / (1024.0 * 1024.0));

        renderText(fitText(m_fontMedium, entry.label, 340), row.x + 16, row.y + 12, m_fontMedium, color(241, 245, 249));
        renderText(fitText(m_fontSmall, entry.userLabel.empty() ? tr("history.unknown_user", "Unknown user") : entry.userLabel, 200), row.x + 360, row.y + 16, m_fontSmall, color(148, 163, 184));
        renderText(fitText(m_fontSmall, entry.deviceLabel.empty() ? tr("history.unknown_device", "Unknown device") : entry.deviceLabel, 240), row.x + 600, row.y + 16, m_fontSmall, color(148, 163, 184));

        // Source Badge
        SDL_Rect sourceBadge{row.x + 880, row.y + 12, 140, 26};
        fillRect(m_renderer, sourceBadge, entry.source == SaveSource::Cloud ? color(8, 47, 73) : color(20, 83, 45));
        strokeRect(m_renderer, sourceBadge, entry.source == SaveSource::Cloud ? color(56, 189, 248) : color(74, 222, 128));
        renderTextCentered(fitText(m_fontSmall, entry.sourceLabel.empty() ? tr("history.unknown_source", "Unknown source") : entry.sourceLabel, 130), sourceBadge, m_fontSmall, color(241, 245, 249));
        renderText(sizeBuf, row.x + 1060, row.y + 16, m_fontSmall, color(148, 163, 184));
        SDL_SetRenderDrawColor(m_renderer, 51, 65, 85, 255);
        SDL_RenderDrawLine(m_renderer, row.x, row.y + row.h + 11, row.x + row.w, row.y + row.h + 11);
    }

    if (screen.sidebar()) {
        renderSidebar(*screen.sidebar());
    }

    renderFooter(tr("footer.hint.revision", "A: Restore/Download  B: Back  X: Refresh  Y: Language  L: Users  -: Delete"),
                 m_statusMessage.empty() ? screen.titleLabel() : m_statusMessage);
}

void SaveShell::renderSidebar(const Sidebar& sidebar) {
    const bool isLoading = Runtime::instance().isLoading();
    const int sidebarW = 576; // 45% of 1280
    SDL_Rect panel{1280 - sidebarW, 96, sidebarW, 580};
    fillRect(m_renderer, panel, color(17, 24, 39, 250));
    SDL_SetRenderDrawColor(m_renderer, 51, 65, 85, 255);
    SDL_RenderDrawLine(m_renderer, panel.x, panel.y, panel.x, panel.y + panel.h);
    
    SDL_Rect topLine{panel.x + 32, panel.y + 76, panel.w - 64, 1};
    fillRect(m_renderer, topLine, color(51, 65, 85));
    renderText(tr("ui.save_actions", "Save Actions"), panel.x + 32, panel.y + 18, m_fontSmall, color(56, 189, 248));
    renderText(fitText(m_fontMedium, sidebar.title(), panel.w - 64), panel.x + 32, panel.y + 38, m_fontMedium, color(241, 245, 249));
    renderText(tr("ui.actions_hint", "A Confirm   B Close"), panel.x + 32, panel.y + 90, m_fontSmall, color(100, 116, 139));

    const auto& items = sidebar.items();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const auto& item = items[i];
        SDL_Rect row{panel.x + 32, panel.y + 126 + i * 82, panel.w - 64, 70};
        const bool isSelected = i == sidebar.index();
        const bool isEnabled = item->isEnabled() && !isLoading;

        SDL_Color rowColor = isSelected ? color(19, 42, 79) : color(17, 24, 39);
        SDL_Color borderColor = isSelected ? color(56, 189, 248) : color(51, 65, 85);
        SDL_Color textColor = isEnabled ? color(241, 245, 249) : color(75, 85, 99);
        SDL_Color infoColor = isEnabled ? color(100, 116, 139) : color(55, 65, 81);

        if (!isEnabled) {
            rowColor = color(12, 18, 28);
            borderColor = color(30, 41, 59);
        }

        fillRect(m_renderer, row, rowColor);
        strokeRect(m_renderer, row, borderColor);
        if (isSelected && isEnabled) {
            drawFocus(m_renderer, row);
        }

        renderText(item->title(), row.x + 20, row.y + 18, m_fontMedium, textColor);
        if (!item->info().empty()) {
            renderText(fitText(m_fontSmall, item->info(), panel.w - 100), row.x + 20, row.y + 44, m_fontSmall, infoColor);
        }
    }
    
    if (!m_statusMessage.empty()) {
        SDL_Rect statusArea{panel.x + 32, panel.y + panel.h - 60, panel.w - 64, 44};
        fillRect(m_renderer, statusArea, color(15, 23, 42));
        strokeRect(m_renderer, statusArea, color(51, 65, 85));
        renderTextCentered(fitText(m_fontSmall, m_statusMessage, statusArea.w - 16),
                          statusArea, m_fontSmall, color(125, 211, 252));
    }
}

void SaveShell::renderText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color colorValue) {
    if (text.empty()) {
        return;
    }

    TTF_Font* actualFont = selectFont(font, text);
    if (!actualFont) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(actualFont, text.c_str(), colorValue);
    if (!surface) {
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    if (texture) {
        SDL_Rect dest{x, y, surface->w, surface->h};
        SDL_RenderCopy(m_renderer, texture, nullptr, &dest);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void SaveShell::renderTextCentered(const std::string& text, const SDL_Rect& rect, TTF_Font* font, SDL_Color colorValue) {
    TTF_Font* actualFont = selectFont(font, text);
    if (!actualFont) {
        return;
    }

    int width = 0;
    int height = 0;
    TTF_SizeUTF8(actualFont, text.c_str(), &width, &height);
    renderText(text,
               rect.x + std::max(0, (rect.w - width) / 2),
               rect.y + std::max(0, (rect.h - height) / 2),
               actualFont,
               colorValue);
}

std::string SaveShell::fitText(TTF_Font* font, const std::string& text, int maxWidth) const {
    if (text.empty()) return "";
    
    TTF_Font* actualFont = selectFont(font, text);
    if (!actualFont) {
        return text;
    }
    
    int width = 0;
    if (TTF_SizeUTF8(actualFont, text.c_str(), &width, nullptr) != 0) {
        return text;
    }
    
    if (width <= maxWidth) {
        return text;
    }

    std::string clipped = text;
    while (!clipped.empty()) {
        do {
            clipped.pop_back();
        } while (!clipped.empty() && (static_cast<unsigned char>(clipped.back()) & 0xC0) == 0x80);
        
        std::string trial = clipped + "...";
        if (TTF_SizeUTF8(actualFont, trial.c_str(), &width, nullptr) == 0 && width <= maxWidth) {
            return trial;
        }
    }
    return "...";
}

SDL_Texture* SaveShell::loadIcon(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }
    auto it = m_iconCache.find(path);
    if (it != m_iconCache.end()) {
        rememberCachedIcon(path);
        return it->second;
    }
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        return nullptr;
    }

    SDL_Surface* scaledSurface = SDL_CreateRGBSurfaceWithFormat(0,
                                                                ICON_TEXTURE_SIZE,
                                                                ICON_TEXTURE_SIZE,
                                                                32,
                                                                SDL_PIXELFORMAT_RGBA32);
    if (!scaledSurface) {
        SDL_FreeSurface(surface);
        return nullptr;
    }

    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    SDL_FillRect(scaledSurface, nullptr, SDL_MapRGBA(scaledSurface->format, 0, 0, 0, 0));

    SDL_Rect destRect{};
    if (surface->w > 0 && surface->h > 0) {
        if (surface->w >= surface->h) {
            destRect.w = ICON_TEXTURE_SIZE;
            destRect.h = std::max(1, (surface->h * ICON_TEXTURE_SIZE) / surface->w);
            destRect.x = 0;
            destRect.y = (ICON_TEXTURE_SIZE - destRect.h) / 2;
        } else {
            destRect.h = ICON_TEXTURE_SIZE;
            destRect.w = std::max(1, (surface->w * ICON_TEXTURE_SIZE) / surface->h);
            destRect.y = 0;
            destRect.x = (ICON_TEXTURE_SIZE - destRect.w) / 2;
        }
    }

    SDL_BlitScaled(surface, nullptr, scaledSurface, &destRect);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, scaledSurface);
    SDL_FreeSurface(scaledSurface);
    SDL_FreeSurface(surface);
    if (texture) {
        m_iconCache[path] = texture;
        rememberCachedIcon(path);
        trimIconCache();
    }
    return texture;
}

void SaveShell::rememberCachedIcon(const std::string& path) {
    auto existing = std::find(m_iconCacheOrder.begin(), m_iconCacheOrder.end(), path);
    if (existing != m_iconCacheOrder.end()) {
        m_iconCacheOrder.erase(existing);
    }
    m_iconCacheOrder.push_back(path);
}

void SaveShell::trimIconCache() {
    while (m_iconCacheOrder.size() > MAX_ICON_CACHE_ITEMS) {
        const std::string oldest = m_iconCacheOrder.front();
        m_iconCacheOrder.pop_front();

        auto it = m_iconCache.find(oldest);
        if (it == m_iconCache.end()) {
            continue;
        }
        if (it->second) {
            SDL_DestroyTexture(it->second);
        }
        m_iconCache.erase(it);
    }
}

void SaveShell::openUserPicker() {
    m_overlay = Overlay::UserPicker;
    m_overlayIndex = 0;
    const auto* selectedUser = m_saveManager.getSelectedUser();
    const auto& users = m_saveManager.getUsers();
    if (!selectedUser) {
        return;
    }
    for (int i = 0; i < static_cast<int>(users.size()); ++i) {
        if (users[i].id == selectedUser->id) {
            m_overlayIndex = i;
            break;
        }
    }
}

void SaveShell::openDropboxOverlay() {
    if (!network::Dropbox::isInternetConnected()) {
        setStatus(tr("error.no_internet", "Internet connection required"));
        return;
    }
    
    m_overlay = Overlay::DropboxAuth;
    m_overlayIndex = 0;
    m_authUrl.clear();
    m_authQrCode.size = 0;
    m_authQrCode.modules.clear();
    m_bridgeSession = {};

    if (m_dropbox.isAuthenticated()) {
        m_dropboxState = DropboxAuthState::Idle;
        setStatus(tr("auth.status_authenticated", "You are already connected to Dropbox."));
    } else {
        m_dropboxState = DropboxAuthState::Idle;
        setStatus(tr("auth.status_ready", "Ready to start Dropbox connection."));
    }
}

void SaveShell::closeOverlay() {
    if (m_hostTextInput) {
        SDL_StopTextInput();
        m_hostTextInput = false;
    }
    m_overlay = Overlay::None;
    m_overlayIndex = 0;
}

void SaveShell::updateAuthQrCode(const std::string& value) {
    m_authQrCode.size = 0;
    m_authQrCode.modules.clear();
    if (value.empty()) {
        return;
    }
    utils::generateQRCode(value, m_authQrCode);
}

void SaveShell::refreshCurrentScreen() {
    auto current = Runtime::instance().current();
    if (!current) {
        rebuildRootScreen();
        return;
    }
    if (current->kind() == ObjectKind::SaveMenuScreen) {
        rebuildRootScreen();
        return;
    }
    if (current->kind() == ObjectKind::RevisionMenuScreen) {
        auto* screen = static_cast<RevisionMenuScreen*>(current.get());
        Runtime::instance().pop();
        Runtime::instance().push(std::make_shared<RevisionMenuScreen>(m_backend,
                                                                      screen->entries().empty() ? 0 : screen->entries().front().source == SaveSource::Cloud ? 0 : 0,
                                                                      SaveSource::Local,
                                                                      screen->titleLabel(),
                                                                      false));
    }
}

void SaveShell::updateOverlay() {
    int itemCount = 0;
    if (m_overlay == Overlay::UserPicker) {
        itemCount = static_cast<int>(m_saveManager.getUsers().size());
    } else if (m_overlay == Overlay::DropboxAuth) {
        if (m_dropboxState == DropboxAuthState::Success) {
            itemCount = 1; // Close and Finish
        } else if (m_dropboxState == DropboxAuthState::ConfirmLogout || m_dropboxState == DropboxAuthState::ConfirmCancel) {
            itemCount = 2; // Yes, No
        } else if (m_dropbox.isAuthenticated()) {
            itemCount = 2; // Close, Logout
        } else if (m_dropboxState == DropboxAuthState::Failed || 
                   m_dropboxState == DropboxAuthState::Idle || 
                   m_dropboxState == DropboxAuthState::Expired) {
            itemCount = 2; // Start/Retry + Cancel
        } else {
            itemCount = 0;
        }

        // Handle polling
        if (m_dropboxState == DropboxAuthState::WaitingForScan) {
            const u64 now = static_cast<u64>(std::time(nullptr));
            if (now - m_lastPollTime >= 8) { // Increased from 2 to 8 to avoid 429
                m_lastPollTime = now;
                const std::string status = m_dropbox.pollBridgeSession(m_bridgeSession);
                if (status == "approved") {
                    m_dropboxState = DropboxAuthState::Approved;
                    setStatus(tr("auth.status_approved", "Authorized! Finishing connection..."));
                } else if (status == "expired") {
                    m_dropboxState = DropboxAuthState::Expired;
                    setStatus(tr("auth.status_expired", "Session expired. Please try again."));
                } else if (status == "failed") {
                    m_dropboxState = DropboxAuthState::Failed;
                    setStatus(tr("auth.status_failed", "Session failed. Please try again."));
                }
            }
        } else if (m_dropboxState == DropboxAuthState::Approved) {
            // One turn transition to Connecting to show the status message
            m_dropboxState = DropboxAuthState::Connecting;
            setStatus(tr("auth.status_connecting", "Please wait a moment..."));
            m_overlayIndex = 0;
        } else if (m_dropboxState == DropboxAuthState::Connecting) {
            // Actually consume the session
            if (m_dropbox.consumeBridgeSession(m_bridgeSession)) {
                m_dropboxState = DropboxAuthState::Success;
                setStatus(tr("auth.status_success", "Successfully connected to Dropbox!"));
                rebuildRootScreen();
            } else {
                m_dropboxState = DropboxAuthState::Failed;
                setStatus(tr("auth.status_consume_failed", "Failed to exchange tokens. Please try again."));
            }
        }
    }

    if (m_controller.gotDown(Button::B)) {
        if (m_overlay == Overlay::DropboxAuth && m_dropboxState == DropboxAuthState::WaitingForScan) {
            m_dropboxState = DropboxAuthState::ConfirmCancel;
            m_overlayIndex = 1; // Default to 'No'
            setStatus(tr("auth.confirm_cancel", "Auth in progress. Are you sure you want to cancel?"));
        } else if (m_overlay == Overlay::DropboxAuth && m_dropboxState == DropboxAuthState::ConfirmCancel) {
            // Cancel the cancellation (go back to QR)
            m_dropboxState = DropboxAuthState::WaitingForScan;
            setStatus(tr("auth.status_waiting_scan", "Scan the QR code with your phone to login."));
            m_overlayIndex = 0;
        } else {
            closeOverlay();
        }
        return;
    }
    if (m_controller.gotDown(Button::Up) && itemCount > 0) {
        m_overlayIndex = (m_overlayIndex - 1 + itemCount) % itemCount;
    }
    if (m_controller.gotDown(Button::Down) && itemCount > 0) {
        m_overlayIndex = (m_overlayIndex + 1) % itemCount;
    }
    if (m_controller.gotDown(Button::A)) {
        activateOverlaySelection();
    }

    if (m_touch.isClicked) {
        const int tx = m_touch.x;
        const int ty = m_touch.y;

        if (m_overlay == Overlay::DropboxAuth) {
            SDL_Rect panel{128, 140, 1024, 475};
            if (m_dropboxState == DropboxAuthState::Success) {
                SDL_Rect row{panel.x + 80, panel.y + 220, panel.w - 160, 60};
                if (tx >= row.x && tx <= row.x + row.w && ty >= row.y && ty <= row.y + row.h) {
                    m_overlayIndex = 0;
                    activateOverlaySelection();
                }
            } else if (m_dropboxState == DropboxAuthState::ConfirmLogout || 
                m_dropboxState == DropboxAuthState::ConfirmCancel ||
                m_dropbox.isAuthenticated()) {
                for (int i = 0; i < 2; ++i) {
                    SDL_Rect row{panel.x + 80, panel.y + 220 + i * 80, panel.w - 160, 60};
                    if (tx >= row.x && tx <= row.x + row.w && ty >= row.y && ty <= row.y + row.h) {
                        m_overlayIndex = i;
                        activateOverlaySelection();
                        break;
                    }
                }
            } else if (m_dropboxState == DropboxAuthState::Failed || 
                       m_dropboxState == DropboxAuthState::Idle || m_dropboxState == DropboxAuthState::Expired) {
                const int qrBox = 300;
                const int qrX = panel.x + 32;
                const int textX = qrX + qrBox + 40;
                const int textY = panel.y + 150;
                const int textW = panel.w - (qrBox + 104);
                
                SDL_Rect startBtn{textX, textY + 115, textW, 56};
                SDL_Rect closeBtn{textX, textY + 185, textW, 56};
                
                if (tx >= startBtn.x && tx <= startBtn.x + startBtn.w && ty >= startBtn.y && ty <= startBtn.y + startBtn.h) {
                    m_overlayIndex = 0;
                    activateOverlaySelection();
                } else if (tx >= closeBtn.x && tx <= closeBtn.x + closeBtn.w && ty >= closeBtn.y && ty <= closeBtn.y + closeBtn.h) {
                    m_overlayIndex = 1;
                    activateOverlaySelection();
                }
            } else if (m_dropboxState == DropboxAuthState::WaitingForScan) {
                const int qrBox = 300;
                const int qrX = panel.x + 32;
                const int textX = qrX + qrBox + 40;
                const int textY = panel.y + 150;
                const int textW = panel.w - (qrBox + 104);
                
                SDL_Rect closeBtn{textX, textY + 265, textW, 48};
                if (tx >= closeBtn.x && tx <= closeBtn.x + closeBtn.w && ty >= closeBtn.y && ty <= closeBtn.y + closeBtn.h) {
                    m_overlayIndex = 0;
                    activateOverlaySelection();
                }
            }
        } else if (m_overlay == Overlay::UserPicker) {
            SDL_Rect panel{260, 128, 760, 464};
            const auto& users = m_saveManager.getUsers();
            for (int i = 0; i < static_cast<int>(users.size()); ++i) {
                SDL_Rect row{panel.x + 28, panel.y + 110 + i * 64, panel.w - 56, 50};
                if (tx >= row.x && tx <= row.x + row.w && ty >= row.y && ty <= row.y + row.h) {
                    m_overlayIndex = i;
                    activateOverlaySelection();
                    break;
                }
            }
        }

        // Always check status chip in the top right, regardless of overlay (unless in Auth itself)
        if (m_overlay != Overlay::DropboxAuth) {
            SDL_Rect statusChip{1076, 33, 180, 30};
            if (tx >= statusChip.x && tx <= statusChip.x + statusChip.w && 
                ty >= statusChip.y && ty <= statusChip.y + statusChip.h) {
                openDropboxOverlay();
            }
        }
    }
}

void SaveShell::activateOverlaySelection() {
    if (m_overlay == Overlay::UserPicker) {
        const auto& users = m_saveManager.getUsers();
        if (m_overlayIndex >= 0 && m_overlayIndex < static_cast<int>(users.size()) &&
            m_saveManager.selectUser(static_cast<std::size_t>(m_overlayIndex))) {
            rebuildRootScreen();
            setStatus(tr("ui.user_changed", "Selection changed"));
        }
        closeOverlay();
        return;
    }

    if (m_overlay != Overlay::DropboxAuth) {
        return;
    }

    if (m_dropboxState == DropboxAuthState::Success) {
        closeOverlay();
        return;
    }

    if (m_dropboxState == DropboxAuthState::ConfirmCancel) {
        if (m_overlayIndex == 0) { // Yes, cancel
            closeOverlay();
        } else { // No, go back to QR
            m_dropboxState = DropboxAuthState::WaitingForScan;
            setStatus(tr("auth.status_waiting_scan", "Scan the QR code with your phone to login."));
            m_overlayIndex = 0;
        }
        return;
    }

    if (m_dropboxState == DropboxAuthState::ConfirmLogout) {
        if (m_overlayIndex == 0) { // Yes
            m_dropbox.logout();
            setStatus(tr("status.disconnected", "Not connected"));
            m_dropboxState = DropboxAuthState::Idle;
            closeOverlay();
            rebuildRootScreen();
        } else { // No
            m_dropboxState = DropboxAuthState::Idle;
            m_overlayIndex = 0;
        }
        return;
    }

    if (m_dropbox.isAuthenticated()) {
        if (m_overlayIndex == 0) { // Close
            closeOverlay();
        } else if (m_overlayIndex == 1) { // Logout
            m_dropboxState = DropboxAuthState::ConfirmLogout;
            m_overlayIndex = 1; // Default to 'No'
            setStatus(tr("auth.confirm_logout", "Are you sure you want to disconnect from Dropbox?"));
        }
        return;
    }

    if (m_dropboxState == DropboxAuthState::Failed || m_dropboxState == DropboxAuthState::Idle || m_dropboxState == DropboxAuthState::Expired) {
        if (m_overlayIndex == 0) { // Start/Retry
            m_dropboxState = DropboxAuthState::Starting;
            setStatus(tr("auth.status_starting_session", "Starting login session..."));
            if (m_dropbox.startBridgeSession(m_bridgeSession)) {
                m_dropboxState = DropboxAuthState::WaitingForScan;
                m_authUrl = m_bridgeSession.authorizeUrl;
                updateAuthQrCode(m_authUrl);
                setStatus(tr("auth.status_waiting_scan", "Scan the QR code with your phone to login."));
                m_overlayIndex = 0;
            } else {
                m_dropboxState = DropboxAuthState::Failed;
                setStatus(tr("auth.status_bridge_failed", "Failed to start session. Check your internet or bridge server."));
                LOG_ERROR("Dropbox: Bridge session start failed. Check your internet.");
            }
        } else { // Cancel
            closeOverlay();
        }
        return;
    }
    
    if (m_dropboxState == DropboxAuthState::WaitingForScan) {
        if (m_overlayIndex == 0) { // Close button pressed
            m_dropboxState = DropboxAuthState::ConfirmCancel;
            m_overlayIndex = 1; // Default to 'No'
            setStatus(tr("auth.confirm_cancel", "Auth in progress. Are you sure you want to cancel?"));
        }
        return;
    }

    // If not authenticated, we might have some actions, but currently it's all automatic except 'Cancel' (which is B)
}

void SaveShell::renderDropboxOverlay() {
    SDL_Rect shade{0, 0, 1280, 720};
    fillRect(m_renderer, shade, color(10, 10, 15, 220)); // Even darker shade

    SDL_Rect panel{128, 140, 1024, 475};
    drawPanel(m_renderer, panel, CAT_MANTLE, CAT_BLUE); // Darker base, Blue border
    
    // Header - Use Blue for the title to match icon
    renderText(tr("auth.title", "Dropbox Setup"), panel.x + 24, panel.y + 31, m_fontLarge, CAT_BLUE);
    
    // Status Bar
    SDL_Rect statusRect{panel.x + 24, panel.y + 80, panel.w - 48, 44};
    fillRect(m_renderer, statusRect, CAT_CRUST); // Deepest black for status bar
    strokeRect(m_renderer, statusRect, CAT_SURFACE0);
    
    SDL_Color statusColor = CAT_SAPPHIRE; // Default to sapphire blue
    if (m_dropboxState == DropboxAuthState::Success) statusColor = CAT_GREEN;
    if (m_dropboxState == DropboxAuthState::Failed || m_dropboxState == DropboxAuthState::Expired) statusColor = CAT_RED;
    
    renderTextCentered(fitText(m_fontSmall, m_statusMessage, statusRect.w - 16),
                       statusRect,
                       m_fontSmall,
                       statusColor);

    if (m_dropboxState == DropboxAuthState::Success) {
        // --- SUCCESS VIEW ---
        // Removed broken checkmark icon per user request.
        // The status message above already indicates success.

        SDL_Rect infoRect{panel.x + 48, panel.y + 160, panel.w - 96, 120};
        renderTextCentered(tr("auth.finish_description", "Dropbox is now ready to use."), 
                           infoRect, m_fontMedium, CAT_TEXT);

        SDL_Rect closeBtn{panel.x + 80, panel.y + 340, panel.w - 160, 60};

        const bool selected = m_overlayIndex == 0;
        fillRect(m_renderer, closeBtn, selected ? CAT_BLUE : CAT_SURFACE0); // Blue when selected
        strokeRect(m_renderer, closeBtn, selected ? CAT_SKY : CAT_SURFACE1);
        if (selected) drawFocus(m_renderer, closeBtn);
        
        SDL_Color btnTextColor = selected ? CAT_BASE : CAT_TEXT;
        renderTextCentered(tr("auth.close_and_finish", "Close and Finish"), closeBtn, m_fontMedium, btnTextColor);

    } else if (m_dropboxState == DropboxAuthState::Connecting || m_dropboxState == DropboxAuthState::Starting || m_dropboxState == DropboxAuthState::Approved) {
        // --- LOADING / TRANSITION VIEW ---
        renderTextCentered(tr("auth.status_connecting", "Please wait a moment..."), 
                           SDL_Rect{panel.x, panel.y + 200, panel.w, 100}, 
                           m_fontMedium, CAT_SAPPHIRE);
        
        int dots = (SDL_GetTicks() / 500) % 4;
        std::string dotsStr = "";
        for(int i=0; i<dots; ++i) dotsStr += ".";
        renderText(dotsStr, panel.x + 640, panel.y + 235, m_fontMedium, CAT_SAPPHIRE);

    } else if (m_dropboxState == DropboxAuthState::ConfirmLogout || m_dropboxState == DropboxAuthState::ConfirmCancel) {
        const char* options[2] = { "ui.yes", "ui.no" };
        const char* fallbacks[2] = { "Yes", "No" };
        for (int i = 0; i < 2; ++i) {
            SDL_Rect row{panel.x + 80, panel.y + 220 + i * 80, panel.w - 160, 60};
            const bool selected = i == m_overlayIndex;
            fillRect(m_renderer, row, selected ? CAT_BLUE : CAT_SURFACE0);
            strokeRect(m_renderer, row, selected ? CAT_SKY : CAT_SURFACE1);
            if (selected) drawFocus(m_renderer, row);
            
            SDL_Color textColor = selected ? CAT_BASE : CAT_TEXT;
            renderTextCentered(tr(options[i], fallbacks[i]), row, m_fontMedium, textColor);
        }
    } else if (m_dropbox.isAuthenticated()) {
        const char* options[2] = { "detail.back", "ui.logout" };
        const char* fallbacks[2] = { "Close", "Logout" };
        for (int i = 0; i < 2; ++i) {
            SDL_Rect row{panel.x + 80, panel.y + 220 + i * 80, panel.w - 160, 60};
            const bool selected = i == m_overlayIndex;
            fillRect(m_renderer, row, selected ? (i == 1 ? CAT_RED : CAT_BLUE) : CAT_SURFACE0);
            strokeRect(m_renderer, row, selected ? (i == 1 ? CAT_MAROON : CAT_SKY) : CAT_SURFACE1);
            if (selected) drawFocus(m_renderer, row);
            
            SDL_Color textColor = selected ? CAT_BASE : (i == 1 ? CAT_RED : CAT_TEXT);
            renderTextCentered(tr(options[i], fallbacks[i]), row, m_fontMedium, textColor);
        }
    } else {
        // Layout: QR on the left, Instructions/Vertical buttons on the right
        const int qrBox = 300;
        const int qrX = panel.x + 32;
        const int qrY = panel.y + 150;
        const int textX = qrX + qrBox + 40;
        const int textY = panel.y + 150;
        const int textW = panel.w - (qrBox + 104);

        // --- LEFT SIDE: QR or Dummy ---
        if (m_dropboxState == DropboxAuthState::WaitingForScan && m_authQrCode.size > 0) {
            SDL_Rect qrArea{qrX, qrY, qrBox, qrBox};
            fillRect(m_renderer, qrArea, {255, 255, 255, 255});
            strokeRect(m_renderer, qrArea, CAT_BLUE); // Blue border for QR

            const int quietZone = 2;
            const int matrixSize = m_authQrCode.size + quietZone * 2;
            const int moduleSize = qrBox / matrixSize;
            const int drawSize = matrixSize * moduleSize;
            const int offsetX = qrX + (qrBox - drawSize) / 2;
            const int offsetY = qrY + (qrBox - drawSize) / 2;

            for (int y = 0; y < m_authQrCode.size; ++y) {
                for (int x = 0; x < m_authQrCode.size; ++x) {
                    const std::size_t idx = static_cast<std::size_t>(y * m_authQrCode.size + x);
                    if (idx < m_authQrCode.modules.size() && m_authQrCode.modules[idx]) {
                        SDL_Rect pixel{
                            offsetX + (x + quietZone) * moduleSize,
                            offsetY + (y + quietZone) * moduleSize,
                            moduleSize,
                            moduleSize,
                        };
                        fillRect(m_renderer, pixel, CAT_CRUST);
                    }
                }
            }
        } else {
            SDL_Rect qrFrame{qrX, qrY, qrBox, qrBox};
            fillRect(m_renderer, qrFrame, CAT_CRUST);
            strokeRect(m_renderer, qrFrame, CAT_SURFACE0);
            if (SDL_Texture* icon = loadIcon("romfs:/gfx/icon.png")) {
                SDL_Rect iconRect{qrX + 50, qrY + 50, qrBox - 100, qrBox - 100};
                SDL_RenderCopy(m_renderer, icon, nullptr, &iconRect);
            }
        }

        // --- RIGHT SIDE: Actions and Instructions ---
        if (m_dropboxState == DropboxAuthState::Idle || m_dropboxState == DropboxAuthState::Failed || m_dropboxState == DropboxAuthState::Expired) {
            renderText(tr("auth.steps_title", "How It Works"), textX, textY, m_fontMedium, CAT_BLUE);
            renderText(tr("auth.waiting_link_hint", "Start by generating the sign-in link."), textX, textY + 45, m_fontSmall, CAT_TEXT);
            renderText(tr("auth.waiting_link_hint2", "Press the button below to begin."), textX, textY + 70, m_fontSmall, CAT_SUBTEXT0);
            
            SDL_Rect startBtn{textX, textY + 115, textW, 56};
            SDL_Rect closeBtn{textX, textY + 185, textW, 56};
            
            const bool startSelected = m_overlayIndex == 0;
            const bool closeSelected = m_overlayIndex == 1;
            
            fillRect(m_renderer, startBtn, startSelected ? CAT_BLUE : CAT_SURFACE0);
            strokeRect(m_renderer, startBtn, startSelected ? CAT_SKY : CAT_SURFACE1);
            if (startSelected) drawFocus(m_renderer, startBtn);
            
            std::string btnText = (m_dropboxState == DropboxAuthState::Idle) ? tr("auth.start_login", "Start Login") : tr("ui.retry", "Retry");
            renderTextCentered(btnText, startBtn, m_fontMedium, startSelected ? CAT_BASE : CAT_TEXT);
            
            fillRect(m_renderer, closeBtn, closeSelected ? CAT_RED : CAT_SURFACE0);
            strokeRect(m_renderer, closeBtn, closeSelected ? CAT_MAROON : CAT_SURFACE1);
            if (closeSelected) drawFocus(m_renderer, closeBtn);
            renderTextCentered(tr("ui.cancel", "Cancel"), closeBtn, m_fontMedium, closeSelected ? CAT_BASE : CAT_RED);
            
            renderText(tr("ui.auth_footer_new", "Use your phone to scan and approve the connection."), textX, textY + 265, m_fontSmall, CAT_OVERLAY0);
        } else if (m_dropboxState == DropboxAuthState::WaitingForScan) {
            renderText(tr("auth.steps_title", "How It Works"), textX, textY, m_fontMedium, CAT_BLUE);
            renderText(tr("auth.status_waiting_scan", "Scan the QR code with your phone to login."), textX, textY + 45, m_fontSmall, CAT_TEXT);
            renderText(tr("auth.step2_short", "Open the link on your phone or PC,"), textX, textY + 80, m_fontSmall, CAT_SUBTEXT0);
            renderText(tr("auth.step2_cont", "then finish sign-in and approval there."), textX, textY + 103, m_fontSmall, CAT_SUBTEXT0);
            renderText(tr("auth.step4_short", "Once approved, connection completes"), textX, textY + 140, m_fontSmall, CAT_SUBTEXT0);
            renderText(tr("auth.step4_cont", "automatically on this device."), textX, textY + 163, m_fontSmall, CAT_SUBTEXT0);
            
            SDL_Rect tipBox{textX, textY + 205, textW, 50};
            fillRect(m_renderer, tipBox, CAT_CRUST);
            renderText(tr("auth.tip", "One-time PKCE setup, then the saved refresh token is reused."), tipBox.x + 12, tipBox.y + 14, m_fontSmall, CAT_SAPPHIRE);
            
            SDL_Rect closeBtn{textX, textY + 265, textW, 48};
            const bool closeSelected = m_overlayIndex == 0;
            fillRect(m_renderer, closeBtn, closeSelected ? CAT_RED : CAT_SURFACE0);
            strokeRect(m_renderer, closeBtn, closeSelected ? CAT_MAROON : CAT_SURFACE1);
            if (closeSelected) drawFocus(m_renderer, closeBtn);
            renderTextCentered(tr("detail.back", "Close"), closeBtn, m_fontMedium, closeSelected ? CAT_BASE : CAT_RED);
        } else {
            renderTextCentered(m_statusMessage, SDL_Rect{textX, textY + 80, textW, 40}, m_fontMedium, CAT_TEXT);
        }
    }

    renderFooter(tr("footer.hint.select_close", "A: Select  B: Close"),
                  tr("ui.auth_footer_new", "Use your phone to scan and approve the connection."));
}

void SaveShell::renderUserPickerOverlay() {
    SDL_Rect shade{0, 0, 1280, 720};
    fillRect(m_renderer, shade, color(2, 6, 14, 180));
    SDL_Rect panel{260, 128, 760, 464};
    drawPanel(m_renderer, panel, color(11, 18, 31, 250), color(51, 65, 85));
    renderText(tr("ui.pick_user", "Choose Save Target"), panel.x + 28, panel.y + 22, m_fontLarge, color(241, 245, 249));
    renderText(tr("ui.pick_user_hint", "Switch between user saves and device saves here."), panel.x + 28, panel.y + 62, m_fontSmall, color(148, 163, 184));

    const auto& users = m_saveManager.getUsers();
    const auto* selectedUser = m_saveManager.getSelectedUser();
    for (int i = 0; i < static_cast<int>(users.size()); ++i) {
        const auto& user = users[i];
        SDL_Rect row{panel.x + 28, panel.y + 110 + i * 64, panel.w - 56, 50};
        const bool selected = i == m_overlayIndex;
        fillRect(m_renderer, row, selected ? color(19, 42, 79) : color(17, 24, 39));
        strokeRect(m_renderer, row, color(51, 65, 85));
        if (selected) {
            drawFocus(m_renderer, row);
        }
        renderText(user.name, row.x + 14, row.y + 12, m_fontMedium, color(241, 245, 249));
        const bool isDevice = user.id == "device";
        const std::string mode = isDevice ? tr("ui.mode_device", "Device saves") : tr("ui.mode_user", "User saves");
        renderText(mode, row.x + 280, row.y + 15, m_fontSmall, color(148, 163, 184));
        if (selectedUser && selectedUser->id == user.id) {
            renderStatusChip(tr("ui.current", "Current"), row.x + row.w - 130, row.y + 10, 110, color(20, 83, 45), color(74, 222, 128));
        }
    }

    renderFooter(tr("footer.hint.select_move_close", "A: Select  Up/Down: Move  B: Close"),
                 tr("ui.selection_footer", "The title list will rescan for the selected save type."));
}

bool SaveShell::currentLanguageIsKorean() const {
    return utils::Language::instance().currentLang() == "ko";
}

bool SaveShell::textNeedsFallbackFont(const std::string& text) const {
    if (!currentLanguageIsKorean()) {
        return false;
    }
    for (unsigned char c : text) {
        if (c & 0x80U) {
            return true;
        }
    }
    return false;
}

TTF_Font* SaveShell::selectFont(TTF_Font* preferred, const std::string& text) const {
    if (!preferred) {
        return nullptr;
    }
    if (!textNeedsFallbackFont(text)) {
        return preferred;
    }
    if (preferred == m_fontLarge && m_fontLargeFallback) {
        return m_fontLargeFallback;
    }
    if (preferred == m_fontMedium && m_fontMediumFallback) {
        return m_fontMediumFallback;
    }
    if (preferred == m_fontSmall && m_fontSmallFallback) {
        return m_fontSmallFallback;
    }
    return preferred;
}

std::string SaveShell::tr(const char* key, const char* fallback) const {
    const std::string value = utils::Language::instance().get(key);
    if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
        return fallback;
    }
    return value;
}

} // namespace ui::saves
