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
    const std::string cloud = dropbox.isAuthenticated()
        ? lang.get("status.connected")
        : lang.get("status.disconnected");
    return mode + "  |  " + selected + "  |  Dropbox: " + cloud;
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

    utils::Language::instance().init();
    // Match the old MainUI startup flow so the first screen has data.
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
    if (keysDown & HidNpadButton_Left) setButtonDown(Button::Left);
    if (keysDown & HidNpadButton_Right) setButtonDown(Button::Right);
    if (keysDown & HidNpadButton_Up) setButtonDown(Button::Up);
    if (keysDown & HidNpadButton_Down) setButtonDown(Button::Down);
}
#endif

void SaveShell::setButtonDown(Button button) {
    m_controller.down |= static_cast<u64>(button);
}

void SaveShell::update() {
    if (m_overlay != Overlay::None) {
        updateOverlay();
        resetInput();
        return;
    }

    if (m_controller.gotDown(Button::Y)) {
        openDropboxOverlay();
        resetInput();
        return;
    }
    if (m_controller.gotDown(Button::L)) {
        openUserPicker();
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
}

void SaveShell::renderHeader(const std::string& title, const std::string& subtitle) {
    SDL_Rect header{0, 0, 1280, 96};
    fillRect(m_renderer, header, color(15, 23, 42));
    SDL_SetRenderDrawColor(m_renderer, 56, 189, 248, 255);
    SDL_Rect accent{24, 24, 8, 40};
    SDL_RenderFillRect(m_renderer, &accent);
    renderText(title, 46, 18, m_fontLarge, color(241, 245, 249));
    if (!subtitle.empty()) {
        renderText(subtitle, 46, 54, m_fontSmall, color(148, 163, 184));
    }
    renderStatusChip(tr(m_dropbox.isAuthenticated() ? "status.connected" : "status.disconnected",
                        m_dropbox.isAuthenticated() ? "Connected" : "Not connected"),
                     1048,
                     22,
                     208,
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
        renderText(rightHint, 880, footer.y + 10, m_fontSmall, color(100, 116, 139));
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
    const int cols = 4;
    const int frameX = 24;
    const int frameY = 108;
    const int frameH = 552;
    const int frameW = 1280 - frameX * 2;
    const int gap = 10;
    const int innerPad = 12;
    const int cardW = (frameW - innerPad * 2 - gap * (cols - 1)) / cols;
    const int cardH = 120;
    const int startX = frameX + innerPad;
    const int startY = frameY + innerPad;

    SDL_Rect contentFrame{frameX, frameY, frameW, frameH};
    drawPanel(m_renderer, contentFrame, color(9, 15, 26, 245), color(51, 65, 85));

    renderText(lang.get("ui.section_titles"), frameX + 20, frameY + 12, m_fontSmall, color(100, 116, 139));
    if (!m_statusMessage.empty()) {
        renderText(fitText(m_fontSmall, m_statusMessage, 640), frameX + 240, frameY + 12, m_fontSmall, color(125, 211, 252));
    }

    for (int i = firstVisible; i < lastVisible; ++i) {
        const auto& entry = entries[i];
        const int visibleIndex = i - firstVisible;
        const int row = visibleIndex / cols;
        const int col = visibleIndex % cols;
        SDL_Rect card{startX + col * (cardW + gap), startY + row * (cardH + gap), cardW, cardH};
        const bool isSelected = i == selected;

        drawPanel(m_renderer, card, isSelected ? color(19, 42, 79) : color(17, 24, 39), color(51, 65, 85));
        if (isSelected) {
            drawFocus(m_renderer, card);
        }

        const int iconSize = 64;
        SDL_Rect iconRect{card.x + 10, card.y + (card.h - iconSize) / 2, iconSize, iconSize};
        SDL_Rect iconFrame{iconRect.x - 1, iconRect.y - 1, iconRect.w + 2, iconRect.h + 2};
        fillRect(m_renderer, iconFrame, color(8, 12, 22));
        if (SDL_Texture* icon = loadIcon(entry.iconPath)) {
            SDL_RenderCopy(m_renderer, icon, nullptr, &iconRect);
        } else {
            fillRect(m_renderer, iconRect, color(15, 23, 42));
            renderText("?", iconRect.x + 26, iconRect.y + 18, m_fontMedium, color(148, 163, 184));
        }
        strokeRect(m_renderer, iconFrame, color(51, 65, 85));

        const int textX = iconRect.x + iconRect.w + 12;
        const int textW = card.x + card.w - textX - 10;
        renderText(fitText(m_fontMedium, entry.name, textW), textX, card.y + 12, m_fontMedium, color(241, 245, 249));
        renderText(fitText(m_fontSmall, entry.author, textW), textX, card.y + 38, m_fontSmall, color(148, 163, 184));
        renderText(fitText(m_fontSmall, entry.subtitle, textW), textX, card.y + 58, m_fontSmall, color(100, 116, 139));

        const int chipGap = 6;
        const int chipW = std::max(60, (textW - chipGap) / 2);
        SDL_Rect localChip{textX, card.y + 88, chipW, 20};
        SDL_Rect cloudChip{textX + chipW + chipGap, card.y + 88, chipW, 20};
        fillRect(m_renderer, localChip, entry.hasLocalBackup ? color(8, 47, 73) : color(39, 39, 42));
        fillRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(20, 83, 45) : color(39, 39, 42));
        strokeRect(m_renderer, localChip, entry.hasLocalBackup ? color(56, 189, 248) : color(82, 82, 91));
        strokeRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(74, 222, 128) : color(82, 82, 91));
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

    renderFooter("A: Open  X: Refresh  Y: Dropbox  L: Users  B: Exit");
}

void SaveShell::renderRevisionMenu(const RevisionMenuScreen& screen) {
    renderHeader(screen.shortTitle(), screen.titleLabel());
    const auto& entries = screen.entries();
    const int selected = screen.index();
    const int firstVisible = screen.firstVisibleIndex();
    const int lastVisible = std::min(static_cast<int>(entries.size()), firstVisible + std::max(1, screen.visibleCount()));

    SDL_Rect listRect{24, 108, 1232, 552};
    drawPanel(m_renderer, listRect, color(9, 15, 26, 245), color(51, 65, 85));

    renderText(tr("ui.revision", "Revision"), 52, 126, m_fontSmall, color(100, 116, 139));
    renderText(tr("ui.device", "Device"), 500, 126, m_fontSmall, color(100, 116, 139));
    renderText(tr("ui.source", "Source"), 862, 126, m_fontSmall, color(100, 116, 139));
    renderText(tr("ui.size", "Size"), 1130, 126, m_fontSmall, color(100, 116, 139));

    for (int i = firstVisible; i < lastVisible; ++i) {
        const auto& entry = entries[i];
        const int visibleIndex = i - firstVisible;
        SDL_Rect row{40, 154 + visibleIndex * 62, 1200, 50};
        const bool isSelected = i == selected;
        fillRect(m_renderer, row, isSelected ? color(19, 42, 79) : color(17, 24, 39));
        strokeRect(m_renderer, row, color(51, 65, 85));
        if (isSelected) {
            drawFocus(m_renderer, row);
        }

        SDL_Rect sourceBadge{row.x + 820, row.y + 12, 108, 24};
        fillRect(m_renderer, sourceBadge, entry.source == SaveSource::Cloud ? color(8, 47, 73) : color(20, 83, 45));
        strokeRect(m_renderer, sourceBadge, entry.source == SaveSource::Cloud ? color(56, 189, 248) : color(74, 222, 128));

        char sizeBuf[32];
        std::snprintf(sizeBuf, sizeof(sizeBuf), "%.1f MB", entry.size / (1024.0 * 1024.0));

        renderText(fitText(m_fontMedium, entry.label, 440), row.x + 16, row.y + 12, m_fontMedium, color(241, 245, 249));
        renderText(fitText(m_fontSmall, entry.deviceLabel.empty() ? tr("history.unknown_device", "Unknown device") : entry.deviceLabel, 300), row.x + 460, row.y + 16, m_fontSmall, color(148, 163, 184));
        renderText(fitText(m_fontSmall, entry.sourceLabel.empty() ? tr("history.unknown_source", "Unknown source") : entry.sourceLabel, 82), sourceBadge.x + 10, sourceBadge.y + 4, m_fontSmall, color(241, 245, 249));
        renderText(sizeBuf, row.x + 1080, row.y + 16, m_fontSmall, color(148, 163, 184));

        SDL_SetRenderDrawColor(m_renderer, 51, 65, 85, 255);
        SDL_RenderDrawLine(m_renderer, row.x, row.y + row.h + 11, row.x + row.w, row.y + row.h + 11);
    }

    renderFooter("A: Restore/Download  X: Refresh  Y: Dropbox  L: Users  B: Back",
                 m_statusMessage.empty() ? screen.titleLabel() : m_statusMessage);
}

void SaveShell::renderSidebar(const Sidebar& sidebar) {
    SDL_Rect panel{904, 96, 352, 580};
    fillRect(m_renderer, panel, color(17, 24, 39, 250));
    SDL_SetRenderDrawColor(m_renderer, 51, 65, 85, 255);
    SDL_RenderDrawLine(m_renderer, panel.x, panel.y, panel.x, panel.y + panel.h);
    SDL_Rect topLine{panel.x + 24, panel.y + 76, panel.w - 48, 1};
    fillRect(m_renderer, topLine, color(51, 65, 85));
    renderText(tr("ui.save_actions", "Save Actions"), panel.x + 24, panel.y + 18, m_fontSmall, color(56, 189, 248));
    renderText(fitText(m_fontMedium, sidebar.title(), panel.w - 48), panel.x + 24, panel.y + 42, m_fontMedium, color(241, 245, 249));
    renderText(tr("ui.actions_hint", "A Confirm   B Close"), panel.x + 24, panel.y + 90, m_fontSmall, color(100, 116, 139));

    const auto& items = sidebar.items();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        SDL_Rect row{panel.x + 24, panel.y + 126 + i * 66, panel.w - 48, 54};
        const bool isSelected = i == sidebar.index();
        fillRect(m_renderer, row, isSelected ? color(19, 42, 79) : color(17, 24, 39));
        strokeRect(m_renderer, row, color(51, 65, 85));
        if (isSelected) {
            drawFocus(m_renderer, row);
        }
        renderText(items[i]->title(), row.x + 14, row.y + 15, m_fontMedium, color(241, 245, 249));
        if (!items[i]->info().empty()) {
            renderText(fitText(m_fontSmall, items[i]->info(), 170), row.x + 188, row.y + 17, m_fontSmall, color(100, 116, 139));
        }
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
    TTF_Font* actualFont = selectFont(font, text);
    if (!actualFont) {
        return text;
    }
    int width = 0;
    int height = 0;
    TTF_SizeUTF8(actualFont, text.c_str(), &width, &height);
    if (width <= maxWidth) {
        return text;
    }

    std::string clipped = text;
    while (!clipped.empty()) {
        clipped.pop_back();
        std::string trial = clipped + "...";
        TTF_SizeUTF8(actualFont, trial.c_str(), &width, &height);
        if (width <= maxWidth) {
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
    m_overlay = Overlay::DropboxAuth;
    m_overlayIndex = 0;
    m_authUrl.clear();
    m_authQrCode.size = 0;
    m_authQrCode.modules.clear();
    m_bridgeSession = {};

    if (m_dropbox.isAuthenticated()) {
        m_dropboxState = DropboxAuthState::Idle;
        m_statusMessage = tr("auth.status_authenticated", "You are already connected to Dropbox.");
    } else {
        m_dropboxState = DropboxAuthState::Starting;
        m_statusMessage = tr("auth.status_starting_session", "Starting login session...");
        if (m_dropbox.startBridgeSession(m_bridgeSession)) {
            m_dropboxState = DropboxAuthState::WaitingForScan;
            m_authUrl = m_bridgeSession.authorizeUrl;
            updateAuthQrCode(m_authUrl);
            m_statusMessage = tr("auth.status_waiting_scan", "Scan the QR code with your phone to login.");
        } else {
            m_dropboxState = DropboxAuthState::Failed;
            m_statusMessage = tr("auth.status_bridge_failed", "Failed to start login session. Check your internet.");
        }
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
                                                                      screen->titleLabel()));
    }
}

void SaveShell::updateOverlay() {
    int itemCount = 0;
    if (m_overlay == Overlay::UserPicker) {
        itemCount = static_cast<int>(m_saveManager.getUsers().size());
    } else if (m_overlay == Overlay::DropboxAuth) {
        if (m_dropboxState == DropboxAuthState::ConfirmLogout) {
            itemCount = 2; // Yes, No
        } else if (m_dropbox.isAuthenticated()) {
            itemCount = 1; // Logout
        } else if (m_dropboxState == DropboxAuthState::Failed) {
            itemCount = 1; // Retry
        } else {
            itemCount = 0;
        }

        // Handle polling
        if (m_dropboxState == DropboxAuthState::WaitingForScan) {
            const u64 now = static_cast<u64>(std::time(nullptr));
            if (now - m_lastPollTime >= 2) {
                m_lastPollTime = now;
                const std::string status = m_dropbox.pollBridgeSession(m_bridgeSession);
                if (status == "approved") {
                    m_dropboxState = DropboxAuthState::Approved;
                    m_statusMessage = tr("auth.status_approved", "Authorized! Finishing connection...");
                } else if (status == "failed" || status == "expired") {
                    m_dropboxState = DropboxAuthState::Failed;
                    m_statusMessage = tr("auth.status_failed", "Session expired or failed. Please try again.");
                }
            }
        } else if (m_dropboxState == DropboxAuthState::Approved) {
            m_dropboxState = DropboxAuthState::Connecting;
            if (m_dropbox.consumeBridgeSession(m_bridgeSession)) {
                m_dropboxState = DropboxAuthState::Success;
                m_statusMessage = tr("auth.status_success", "Successfully connected to Dropbox!");
                rebuildRootScreen();
            } else {
                m_dropboxState = DropboxAuthState::Failed;
                m_statusMessage = tr("auth.status_consume_failed", "Failed to exchange tokens. Please try again.");
            }
        }
    }

    if (m_controller.gotDown(Button::B)) {
        closeOverlay();
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
}

void SaveShell::activateOverlaySelection() {
    if (m_overlay == Overlay::UserPicker) {
        const auto& users = m_saveManager.getUsers();
        if (m_overlayIndex >= 0 && m_overlayIndex < static_cast<int>(users.size()) &&
            m_saveManager.selectUser(static_cast<std::size_t>(m_overlayIndex))) {
            rebuildRootScreen();
            m_statusMessage = tr("ui.user_changed", "Selection changed");
        }
        closeOverlay();
        return;
    }

    if (m_overlay != Overlay::DropboxAuth) {
        return;
    }

    if (m_dropboxState == DropboxAuthState::ConfirmLogout) {
        if (m_overlayIndex == 0) { // Yes
            m_dropbox.logout();
            m_statusMessage = tr("status.disconnected", "Not connected");
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
        if (m_overlayIndex == 0) { // Logout
            m_dropboxState = DropboxAuthState::ConfirmLogout;
            m_overlayIndex = 1; // Default to 'No'
            m_statusMessage = tr("auth.confirm_logout", "Are you sure you want to disconnect from Dropbox?");
        }
        return;
    }

    if (m_dropboxState == DropboxAuthState::Failed) {
        if (m_overlayIndex == 0) { // Retry
            openDropboxOverlay();
        }
        return;
    }

    // If not authenticated, we might have some actions, but currently it's all automatic except 'Cancel' (which is B)
}

void SaveShell::renderDropboxOverlay() {
    SDL_Rect shade{0, 0, 1280, 720};
    fillRect(m_renderer, shade, color(2, 6, 14, 180));
    SDL_Rect panel{240, 110, 800, 500};
    drawPanel(m_renderer, panel, color(11, 18, 31, 250), color(51, 65, 85));
    renderText(tr("auth.title", "Dropbox Setup"), panel.x + 28, panel.y + 22, m_fontLarge, color(241, 245, 249));
    
    SDL_Rect statusRect{panel.x + 28, panel.y + 70, panel.w - 56, 48};
    fillRect(m_renderer, statusRect, color(15, 23, 42));
    strokeRect(m_renderer, statusRect, color(51, 65, 85));
    renderTextCentered(fitText(m_fontSmall, m_statusMessage, statusRect.w - 24),
                       statusRect,
                       m_fontSmall,
                       color(125, 211, 252));

    if (m_dropboxState == DropboxAuthState::ConfirmLogout) {
        const char* options[2] = { "ui.yes", "ui.no" };
        const char* fallbacks[2] = { "Yes, disconnect", "No, stay connected" };
        for (int i = 0; i < 2; ++i) {
            SDL_Rect row{panel.x + 100, panel.y + 200 + i * 80, panel.w - 200, 60};
            const bool selected = i == m_overlayIndex;
            fillRect(m_renderer, row, selected ? color(19, 42, 79) : color(17, 24, 39));
            strokeRect(m_renderer, row, color(51, 65, 85));
            if (selected) drawFocus(m_renderer, row);
            renderTextCentered(tr(options[i], fallbacks[i]), row, m_fontMedium, color(241, 245, 249));
        }
    } else if (m_dropbox.isAuthenticated()) {
        SDL_Rect row{panel.x + 100, panel.y + 200, panel.w - 200, 60};
        const bool selected = m_overlayIndex == 0;
        fillRect(m_renderer, row, selected ? color(19, 42, 79) : color(17, 24, 39));
        strokeRect(m_renderer, row, color(51, 65, 85));
        if (selected) drawFocus(m_renderer, row);
        renderTextCentered(tr("ui.logout", "Logout"), row, m_fontMedium, color(248, 113, 113));
    } else if (m_dropboxState == DropboxAuthState::Failed) {
        SDL_Rect row{panel.x + 100, panel.y + 200, panel.w - 200, 60};
        const bool selected = m_overlayIndex == 0;
        fillRect(m_renderer, row, selected ? color(19, 42, 79) : color(17, 24, 39));
        strokeRect(m_renderer, row, color(51, 65, 85));
        if (selected) drawFocus(m_renderer, row);
        renderTextCentered(tr("ui.retry", "Retry"), row, m_fontMedium, color(241, 245, 249));
    } else {
        // Show QR code for login
        if (m_authQrCode.size > 0) {
            const int qrBox = 260;
            const int qrX = panel.x + (panel.w - qrBox) / 2;
            const int qrY = panel.y + 140;
            
            SDL_Rect qrFrame{qrX, qrY, qrBox, qrBox};
            fillRect(m_renderer, qrFrame, color(15, 23, 42));
            strokeRect(m_renderer, qrFrame, color(51, 65, 85));
            
            const int quietZone = 2;
            const int matrixSize = m_authQrCode.size + quietZone * 2;
            const int moduleSize = (qrBox - 20) / matrixSize;
            const int drawSize = matrixSize * moduleSize;
            const int offsetX = qrFrame.x + (qrBox - drawSize) / 2;
            const int offsetY = qrFrame.y + (qrBox - drawSize) / 2;

            SDL_Rect whiteBg{offsetX, offsetY, drawSize, drawSize};
            fillRect(m_renderer, whiteBg, color(255, 255, 255));

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
                        fillRect(m_renderer, pixel, color(17, 24, 39));
                    }
                }
            }
            renderTextCentered(tr("auth.qr_hint", "Scan to sign in"), 
                               SDL_Rect{qrX, qrY + qrBox + 10, qrBox, 30}, 
                               m_fontSmall, color(148, 163, 184));
        }
    }

    renderFooter("A: Select  B: Close",
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

    renderFooter("A: Select  Up/Down: Move  B: Close",
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
