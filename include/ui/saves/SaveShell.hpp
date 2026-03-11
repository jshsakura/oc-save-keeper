#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "ui/saves/Types.hpp"

#include <memory>
#include <string>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace core {
class SaveManager;
}

namespace network {
class Dropbox;
}

namespace ui::saves {

class SaveBackend;
class SaveMenuScreen;
class RevisionMenuScreen;
class Sidebar;
class Widget;

class SaveShell {
public:
    SaveShell(SDL_Renderer* renderer, network::Dropbox& dropbox, core::SaveManager& saveManager);
    ~SaveShell();

    bool initialize();
    void handleEvent(const SDL_Event& event);
#ifdef __SWITCH__
    void handlePadButtons(u64 keysDown);
#endif
    void update();
    void render();

    bool shouldExit() const {
        return m_shouldExit;
    }

private:
    void pushRootScreen();
    void resetInput();
    void dispatchCurrent();
    void renderSaveMenu(const SaveMenuScreen& screen);
    void renderRevisionMenu(const RevisionMenuScreen& screen);
    void renderSidebar(const Sidebar& sidebar);
    void renderHeader(const std::string& title, const std::string& subtitle = "");
    void renderText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color);
    std::string fitText(TTF_Font* font, const std::string& text, int maxWidth) const;
    SDL_Texture* loadIcon(const std::string& path);
    void setButtonDown(Button button);

    SDL_Renderer* m_renderer;
    network::Dropbox& m_dropbox;
    core::SaveManager& m_saveManager;
    std::shared_ptr<SaveBackend> m_backend;
    std::shared_ptr<SaveMenuScreen> m_rootScreen;

    TTF_Font* m_fontLarge = nullptr;
    TTF_Font* m_fontMedium = nullptr;
    TTF_Font* m_fontSmall = nullptr;

    Controller m_controller{};
    TouchInfo m_touch{};
    bool m_shouldExit = false;
};

} // namespace ui::saves
