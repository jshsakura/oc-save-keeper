#include "ui/saves/SaveShell.hpp"

#include "ui/saves/RevisionMenuScreen.hpp"
#include "ui/saves/Runtime.hpp"
#include "ui/saves/SaveBackendAdapter.hpp"
#include "ui/saves/SaveMenuScreen.hpp"
#include "ui/saves/Sidebar.hpp"
#include "ui/saves/Widget.hpp"

#include "core/SaveManager.hpp"
#include "network/Dropbox.hpp"
#include "utils/Logger.hpp"

#include <algorithm>
#include <cstdio>

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

} // namespace

SaveShell::SaveShell(SDL_Renderer* renderer, network::Dropbox& dropbox, core::SaveManager& saveManager)
    : m_renderer(renderer)
    , m_dropbox(dropbox)
    , m_saveManager(saveManager) {}

SaveShell::~SaveShell() {
    if (m_fontLarge) TTF_CloseFont(m_fontLarge);
    if (m_fontMedium) TTF_CloseFont(m_fontMedium);
    if (m_fontSmall) TTF_CloseFont(m_fontSmall);
}

bool SaveShell::initialize() {
    if (TTF_WasInit() == 0 && TTF_Init() < 0) {
        LOG_ERROR("TTF_Init failed: %s", TTF_GetError());
        return false;
    }

    m_fontLarge = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 34);
    m_fontMedium = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 24);
    m_fontSmall = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18);
    if (!m_fontLarge || !m_fontMedium || !m_fontSmall) {
        LOG_ERROR("SaveShell font initialization failed");
        return false;
    }

    m_backend = std::make_shared<SaveBackendAdapter>(m_saveManager, m_dropbox);
    pushRootScreen();
    return true;
}

void SaveShell::pushRootScreen() {
    m_rootScreen = std::make_shared<SaveMenuScreen>(m_backend);
    Runtime::instance().push(m_rootScreen);
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
                case SDLK_LEFT: setButtonDown(Button::Left); break;
                case SDLK_RIGHT: setButtonDown(Button::Right); break;
                case SDLK_UP: setButtonDown(Button::Up); break;
                case SDLK_DOWN: setButtonDown(Button::Down); break;
                default: break;
            }
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
            return;
        case ObjectKind::RevisionMenuScreen:
            renderRevisionMenu(*static_cast<RevisionMenuScreen*>(current.get()));
            return;
        default:
            break;
    }

    renderHeader("Save Browser");
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
    SDL_SetRenderDrawColor(m_renderer, 51, 65, 85, 255);
    SDL_RenderDrawLine(m_renderer, 0, header.h - 1, 1280, header.h - 1);
}

void SaveShell::renderSaveMenu(const SaveMenuScreen& screen) {
    renderHeader("Saves", "A Open   X Refresh   B Exit");

    const auto& entries = screen.entries();
    const int selected = screen.index();
    const int cols = 4;
    const int cardW = 272;
    const int cardH = 148;
    const int startX = 56;
    const int startY = 122;
    const int gap = 20;

    SDL_Rect contentFrame{40, 108, screen.sidebar() ? 792 : 1200, 580};
    drawPanel(m_renderer, contentFrame, color(9, 15, 26, 245), color(51, 65, 85));

    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const auto& entry = entries[i];
        const int row = i / cols;
        const int col = i % cols;
        SDL_Rect card{startX + col * (cardW + gap), startY + row * (cardH + gap), cardW, cardH};
        const bool isSelected = i == selected;

        drawPanel(m_renderer, card, isSelected ? color(19, 42, 79) : color(17, 24, 39), color(51, 65, 85));
        if (isSelected) {
            drawFocus(m_renderer, card);
        }

        SDL_Rect iconRect{card.x + 14, card.y + 14, 112, 112};
        SDL_Rect iconFrame{iconRect.x - 1, iconRect.y - 1, iconRect.w + 2, iconRect.h + 2};
        fillRect(m_renderer, iconFrame, color(8, 12, 22));
        if (SDL_Texture* icon = loadIcon(entry.iconPath)) {
            SDL_RenderCopy(m_renderer, icon, nullptr, &iconRect);
            SDL_DestroyTexture(icon);
        } else {
            fillRect(m_renderer, iconRect, color(15, 23, 42));
            renderText("?", iconRect.x + 45, iconRect.y + 30, m_fontLarge, color(148, 163, 184));
        }
        strokeRect(m_renderer, iconFrame, color(51, 65, 85));

        const int textX = card.x + 140;
        renderText(fitText(m_fontMedium, entry.name, 118), textX, card.y + 16, m_fontMedium, color(241, 245, 249));
        renderText(fitText(m_fontSmall, entry.author, 120), textX, card.y + 46, m_fontSmall, color(148, 163, 184));
        renderText(fitText(m_fontSmall, entry.subtitle, 120), textX, card.y + 68, m_fontSmall, color(100, 116, 139));

        SDL_Rect localChip{textX, card.y + 100, 102, 26};
        SDL_Rect cloudChip{textX + 112, card.y + 100, 110, 26};
        fillRect(m_renderer, localChip, entry.hasLocalBackup ? color(8, 47, 73) : color(39, 39, 42));
        fillRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(20, 83, 45) : color(39, 39, 42));
        strokeRect(m_renderer, localChip, entry.hasLocalBackup ? color(56, 189, 248) : color(82, 82, 91));
        strokeRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(74, 222, 128) : color(82, 82, 91));
        renderText(entry.hasLocalBackup ? "LOCAL" : "EMPTY", localChip.x + 14, localChip.y + 5, m_fontSmall, color(241, 245, 249));
        renderText(entry.hasCloudBackup ? "CLOUD" : "OFFLINE", cloudChip.x + 12, cloudChip.y + 5, m_fontSmall, color(241, 245, 249));
    }

    if (screen.sidebar()) {
        renderSidebar(*screen.sidebar());
    }
}

void SaveShell::renderRevisionMenu(const RevisionMenuScreen& screen) {
    renderHeader(screen.shortTitle(), screen.titleLabel());
    const auto& entries = screen.entries();
    const int selected = screen.index();

    SDL_Rect listRect{48, 116, 1184, 572};
    drawPanel(m_renderer, listRect, color(9, 15, 26, 245), color(51, 65, 85));

    renderText("Revision", 76, 126, m_fontSmall, color(100, 116, 139));
    renderText("Device", 450, 126, m_fontSmall, color(100, 116, 139));
    renderText("Source", 760, 126, m_fontSmall, color(100, 116, 139));
    renderText("Size", 1040, 126, m_fontSmall, color(100, 116, 139));

    for (int i = 0; i < static_cast<int>(entries.size()) && i < 8; ++i) {
        const auto& entry = entries[i];
        SDL_Rect row{64, 154 + i * 62, 1152, 50};
        const bool isSelected = i == selected;
        fillRect(m_renderer, row, isSelected ? color(19, 42, 79) : color(17, 24, 39));
        strokeRect(m_renderer, row, color(51, 65, 85));
        if (isSelected) {
            drawFocus(m_renderer, row);
        }

        SDL_Rect sourceBadge{row.x + 702, row.y + 12, 108, 24};
        fillRect(m_renderer, sourceBadge, entry.source == SaveSource::Cloud ? color(8, 47, 73) : color(20, 83, 45));
        strokeRect(m_renderer, sourceBadge, entry.source == SaveSource::Cloud ? color(56, 189, 248) : color(74, 222, 128));

        char sizeBuf[32];
        std::snprintf(sizeBuf, sizeof(sizeBuf), "%.1f MB", entry.size / (1024.0 * 1024.0));

        renderText(fitText(m_fontMedium, entry.label, 320), row.x + 16, row.y + 12, m_fontMedium, color(241, 245, 249));
        renderText(fitText(m_fontSmall, entry.deviceLabel.empty() ? "Unknown device" : entry.deviceLabel, 220), row.x + 386, row.y + 16, m_fontSmall, color(148, 163, 184));
        renderText(fitText(m_fontSmall, entry.sourceLabel.empty() ? "Unknown source" : entry.sourceLabel, 82), sourceBadge.x + 10, sourceBadge.y + 4, m_fontSmall, color(241, 245, 249));
        renderText(sizeBuf, row.x + 972, row.y + 16, m_fontSmall, color(148, 163, 184));

        SDL_SetRenderDrawColor(m_renderer, 51, 65, 85, 255);
        SDL_RenderDrawLine(m_renderer, row.x, row.y + row.h + 11, row.x + row.w, row.y + row.h + 11);
    }
}

void SaveShell::renderSidebar(const Sidebar& sidebar) {
    SDL_Rect panel{844, 0, 436, 720};
    fillRect(m_renderer, panel, color(17, 24, 39, 250));
    SDL_SetRenderDrawColor(m_renderer, 51, 65, 85, 255);
    SDL_RenderDrawLine(m_renderer, panel.x, 0, panel.x, 720);
    SDL_Rect topLine{panel.x + 24, 90, panel.w - 48, 1};
    fillRect(m_renderer, topLine, color(51, 65, 85));
    renderText("Save Actions", panel.x + 24, 24, m_fontLarge, color(241, 245, 249));
    renderText("A Confirm   B Close", panel.x + 24, 60, m_fontSmall, color(100, 116, 139));

    const auto& items = sidebar.items();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        SDL_Rect row{panel.x + 24, 116 + i * 64, panel.w - 48, 52};
        const bool isSelected = i == sidebar.index();
        fillRect(m_renderer, row, isSelected ? color(19, 42, 79) : color(17, 24, 39));
        strokeRect(m_renderer, row, color(51, 65, 85));
        if (isSelected) {
            drawFocus(m_renderer, row);
        }
        renderText(items[i]->title(), row.x + 14, row.y + 14, m_fontMedium, color(241, 245, 249));
        if (!items[i]->info().empty()) {
            renderText(fitText(m_fontSmall, items[i]->info(), 38), row.x + 180, row.y + 16, m_fontSmall, color(100, 116, 139));
        }
    }
}

void SaveShell::renderText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color colorValue) {
    if (!font || text.empty()) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), colorValue);
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

std::string SaveShell::fitText(TTF_Font* font, const std::string& text, int maxWidth) const {
    if (!font) {
        return text;
    }
    int width = 0;
    int height = 0;
    TTF_SizeUTF8(font, text.c_str(), &width, &height);
    if (width <= maxWidth) {
        return text;
    }

    std::string clipped = text;
    while (!clipped.empty()) {
        clipped.pop_back();
        std::string trial = clipped + "...";
        TTF_SizeUTF8(font, trial.c_str(), &width, &height);
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
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

} // namespace ui::saves
