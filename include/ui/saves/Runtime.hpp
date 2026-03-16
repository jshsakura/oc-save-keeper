#pragma once

#include "ui/saves/Types.hpp"

#include <memory>
#include <string>
#include <vector>

struct SDL_Renderer;
struct _TTF_Font;
typedef struct _TTF_Font TTF_Font;

namespace ui::saves {

class Object;

class Runtime {
public:
    static Runtime& instance();

    void push(std::shared_ptr<Object> object);
    std::shared_ptr<Object> current() const;
    void popToMenu();
    void pop();

    void notify(const std::string& text);
    void pushError(const std::string& text);
    
    std::string consumeNotification();
    bool hasNotification() const { return !m_lastNotification.empty(); }

    void playSound(SoundEffect effect);

    const Theme& theme() const;
    Theme& theme();

    std::vector<AccountProfileBase> getAccountList() const;

    void setRenderer(SDL_Renderer* renderer) { m_renderer = renderer; }
    void setFont(TTF_Font* font) { m_font = font; }
    void setShell(class SaveShell* shell) { m_shell = shell; }
    void forceRender();
    
    void setLoading(bool loading, const std::string& message = "");
    bool isLoading() const { return m_isLoading; }
    const std::string& loadingMessage() const { return m_loadingMessage; }

private:
    Runtime();
    void drawLoadingOverlay();

    SDL_Renderer* m_renderer = nullptr;
    TTF_Font* m_font = nullptr;
    class SaveShell* m_shell = nullptr;
    Theme m_theme{};
    std::vector<std::shared_ptr<Object>> m_stack{};
    std::string m_lastNotification{};
    bool m_isLoading = false;
    std::string m_loadingMessage;
};

} // namespace ui::saves
