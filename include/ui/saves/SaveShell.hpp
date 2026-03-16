#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "ui/saves/Types.hpp"
#include "utils/QRCode.hpp"
#include "network/Dropbox.hpp"

#include <memory>
#include <deque>
#include <map>
#include <vector>
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
    void showProcessingOverlay(const std::string& message);

    bool shouldExit() const {
        return m_shouldExit;
    }

private:
    enum class Overlay {
        None,
        UserPicker,
        DropboxAuth,
    };

    enum class DropboxAuthState {
        Idle,
        Starting,
        WaitingForScan,
        Approved,
        Connecting,
        Success,
        Failed,
        Expired,
        ConfirmLogout,
        ConfirmCancel,
    };

    void pushRootScreen();
    void rebuildRootScreen();
    void resetInput();
    void dispatchCurrent();
    void updateOverlay();
    void renderFooter(const std::string& leftHint, const std::string& rightHint = "");
    void renderSaveMenu(const SaveMenuScreen& screen);
    void renderRevisionMenu(const RevisionMenuScreen& screen);
    void renderSidebar(const Sidebar& sidebar);
    void renderDropboxOverlay();
    void renderUserPickerOverlay();
    void renderHeader(const std::string& title, const std::string& subtitle = "");
    void renderStatusChip(const std::string& text, int x, int y, int w, SDL_Color fill, SDL_Color border);
    void renderText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color);
    void renderTextCentered(const std::string& text, const SDL_Rect& rect, TTF_Font* font, SDL_Color color);
    std::string fitText(TTF_Font* font, const std::string& text, int maxWidth) const;
    SDL_Texture* loadIcon(const std::string& path);
    void rememberCachedIcon(const std::string& path);
    void trimIconCache();
    void openUserPicker();
    void openDropboxOverlay();
    void closeOverlay();
    void refreshCurrentScreen();
    void activateOverlaySelection();
    void updateAuthQrCode(const std::string& value);
    bool currentLanguageIsKorean() const;
    bool textNeedsFallbackFont(const std::string& text) const;
    TTF_Font* selectFont(TTF_Font* preferred, const std::string& text) const;
    std::string tr(const char* key, const char* fallback) const;
    void setButtonDown(Button button);
    void setStatus(const std::string& message);

    SDL_Renderer* m_renderer;
    network::Dropbox& m_dropbox;
    core::SaveManager& m_saveManager;
    std::shared_ptr<SaveBackend> m_backend;
    std::shared_ptr<SaveMenuScreen> m_rootScreen;

    TTF_Font* m_fontLarge = nullptr;
    TTF_Font* m_fontMedium = nullptr;
    TTF_Font* m_fontSmall = nullptr;
    TTF_Font* m_fontLargeFallback = nullptr;
    TTF_Font* m_fontMediumFallback = nullptr;
    TTF_Font* m_fontSmallFallback = nullptr;
#ifdef __SWITCH__
    bool m_plInitialized = false;
#endif

    Controller m_controller{};
    TouchInfo m_touch{};
    bool m_shouldExit = false;
    Overlay m_overlay = Overlay::None;
    DropboxAuthState m_dropboxState = DropboxAuthState::Idle;
    int m_overlayIndex = 0;
    std::string m_statusMessage;
    u64 m_statusTime = 0;
    std::string m_authUrl;
    utils::QRCodeMatrix m_authQrCode;
    network::DropboxBridgeSession m_bridgeSession{};
    u64 m_lastPollTime = 0;
    bool m_hostTextInput = false;
    bool m_isAppletMode = false;
    static constexpr std::size_t MAX_ICON_CACHE_ITEMS = 24;
    static constexpr int ICON_TEXTURE_SIZE = 96;
    std::map<std::string, SDL_Texture*> m_iconCache;
    std::deque<std::string> m_iconCacheOrder;
};

} // namespace ui::saves
