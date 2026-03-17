#include "ui/saves/Runtime.hpp"

#include "ui/saves/Object.hpp"
#include "ui/saves/SaveShell.hpp"
#include "utils/Logger.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

namespace ui::saves {

Runtime& Runtime::instance() {
    static Runtime runtime;
    return runtime;
}

Runtime::Runtime() {
    m_theme.colors[ThemeEntryID_BACKGROUND] = {15, 23, 42, 255};
    m_theme.colors[ThemeEntryID_GRID] = {30, 41, 68, 255};
    m_theme.colors[ThemeEntryID_POPUP] = {21, 32, 58, 245};
    m_theme.colors[ThemeEntryID_ERROR] = {248, 113, 113, 255};
    m_theme.colors[ThemeEntryID_TEXT] = {241, 245, 249, 255};
    m_theme.colors[ThemeEntryID_TEXT_INFO] = {148, 163, 184, 255};
    m_theme.colors[ThemeEntryID_TEXT_SELECTED] = {255, 255, 255, 255};
    m_theme.colors[ThemeEntryID_SELECTED_BACKGROUND] = {56, 189, 248, 40};
    m_theme.colors[ThemeEntryID_FOCUS] = {56, 189, 248, 255};
    m_theme.colors[ThemeEntryID_LINE] = {51, 65, 85, 255};
    m_theme.colors[ThemeEntryID_LINE_SEPARATOR] = {51, 65, 85, 255};
    m_theme.colors[ThemeEntryID_SIDEBAR] = {17, 24, 39, 255};
    m_theme.colors[ThemeEntryID_SCROLLBAR] = {56, 189, 248, 255};
    m_theme.colors[ThemeEntryID_SCROLLBAR_BACKGROUND] = {30, 41, 59, 255};
    m_theme.colors[ThemeEntryID_PROGRESSBAR] = {56, 189, 248, 255};
    m_theme.colors[ThemeEntryID_PROGRESSBAR_BACKGROUND] = {30, 41, 59, 255};
    m_theme.colors[ThemeEntryID_HIGHLIGHT_1] = {34, 197, 94, 255};
    m_theme.colors[ThemeEntryID_HIGHLIGHT_2] = {14, 165, 233, 255};
    m_theme.colors[ThemeEntryID_ICON_COLOUR] = {255, 255, 255, 255};
}

void Runtime::push(std::shared_ptr<Object> object) {
    if (object) {
        m_stack.push_back(std::move(object));
    }
}

std::shared_ptr<Object> Runtime::current() const {
    if (m_stack.empty()) {
        return {};
    }
    return m_stack.back();
}

void Runtime::popToMenu() {
    if (!m_stack.empty()) {
        m_stack.resize(1);
    }
}

void Runtime::pop() {
    if (!m_stack.empty()) {
        m_stack.pop_back();
    }
}

void Runtime::notify(const std::string& text) {
    LOG_INFO("ui runtime notify: %s", text.c_str());
    m_lastNotification = text;
}

void Runtime::pushError(const std::string& text) {
    LOG_ERROR("ui runtime error: %s", text.c_str());
    m_lastNotification = "Error: " + text;
}

std::string Runtime::consumeNotification() {
    std::string text = std::move(m_lastNotification);
    m_lastNotification.clear();
    return text;
}

void Runtime::playSound(SoundEffect effect) {
    (void)effect;
}

const Theme& Runtime::theme() const {
    return m_theme;
}

Theme& Runtime::theme() {
    return m_theme;
}

std::vector<AccountProfileBase> Runtime::getAccountList() const {
    return {};
}

void Runtime::setLoading(bool loading, const std::string& message) {
    m_isLoading = loading;
    m_loadingMessage = message;
    
    // Clean up cached texture when loading ends
    if (!loading && m_loadingTexture) {
        SDL_DestroyTexture(m_loadingTexture);
        m_loadingTexture = nullptr;
        m_loadingTextureText.clear();
    }
}

void Runtime::forceRender() {
    if (!m_renderer) return;
    
    if (m_shell) {
        m_shell->render();
    } else {
        SDL_SetRenderDrawColor(m_renderer, 10, 16, 28, 255);
        SDL_RenderClear(m_renderer);
        
        if (auto obj = current()) {
            obj->draw();
        }
        
        if (m_isLoading) {
            drawLoadingOverlay();
        }
    }
    
    SDL_RenderPresent(m_renderer);
}

void Runtime::drawLoadingOverlay() {
    if (!m_renderer) return;
    
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    
    SDL_Rect overlay{0, 0, 1280, 720};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(m_renderer, &overlay);
    
    SDL_Rect box{440, 300, 400, 120};
    SDL_SetRenderDrawColor(m_renderer, 17, 24, 39, 255);
    SDL_RenderFillRect(m_renderer, &box);
    SDL_SetRenderDrawColor(m_renderer, 56, 189, 248, 255);
    SDL_RenderDrawRect(m_renderer, &box);
    
    if (m_font && !m_loadingMessage.empty()) {
        // Recreate texture only if message changed
        if (!m_loadingTexture || m_loadingTextureText != m_loadingMessage) {
            if (m_loadingTexture) {
                SDL_DestroyTexture(m_loadingTexture);
                m_loadingTexture = nullptr;
            }
            
            SDL_Color white{241, 245, 249, 255};
            SDL_Surface* surface = TTF_RenderUTF8_Blended(m_font, m_loadingMessage.c_str(), white);
            if (surface) {
                m_loadingTexture = SDL_CreateTextureFromSurface(m_renderer, surface);
                m_loadingTextureText = m_loadingMessage;
                SDL_FreeSurface(surface);
            }
        }
        
        // Render cached texture
        if (m_loadingTexture) {
            int textW = 0, textH = 0;
            SDL_QueryTexture(m_loadingTexture, nullptr, nullptr, &textW, &textH);
            SDL_Rect dest{
                box.x + (box.w - textW) / 2,
                box.y + (box.h - textH) / 2,
                textW,
                textH
            };
            SDL_RenderCopy(m_renderer, m_loadingTexture, nullptr, &dest);
        }
    }
    
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
}

} // namespace ui::saves
