/**
 * Drop-Keep - Dropbox Save Sync for Nintendo Switch
 * Main UI - SX OS style interface
 */

#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <string>

#include "core/SaveManager.hpp"
#include "network/Dropbox.hpp"
#include "utils/Logger.hpp"

namespace ui {

// Color scheme
namespace Color {
    constexpr SDL_Color Background = {30, 30, 40, 255};
    constexpr SDL_Color Header = {45, 45, 60, 255};
    constexpr SDL_Color Card = {50, 50, 65, 255};
    constexpr SDL_Color CardHover = {60, 60, 80, 255};
    constexpr SDL_Color Accent = {0, 150, 255, 255};      // Dropbox Blue
    constexpr SDL_Color Warning = {255, 180, 0, 255};
    constexpr SDL_Color Error = {255, 80, 80, 255};
    constexpr SDL_Color Text = {255, 255, 255, 255};
    constexpr SDL_Color TextDim = {180, 180, 180, 255};
    constexpr SDL_Color Synced = {80, 200, 120, 255};
    constexpr SDL_Color NotSynced = {200, 100, 100, 255};
}

// UI State
enum class State {
    Main,           // Game list
    GameDetail,     // Selected game options
    VersionHistory, // Version list
    Auth,           // Dropbox token input
    SyncAll,        // Batch sync progress
    Settings
};

// Button
struct Button {
    SDL_Rect rect;
    std::string text;
    bool hover;
    
    Button(int x, int y, int w, int h, const std::string& t) : text(t), hover(false) {
        rect = {x, y, w, h};
    }
    
    bool contains(int px, int py) const {
        return px >= rect.x && px < rect.x + rect.w &&
               py >= rect.y && py < rect.y + rect.h;
    }
};

// Game card
struct GameCard {
    core::TitleInfo* title;
    SDL_Rect rect;
    SDL_Texture* icon;
    bool selected;
    bool synced;
};

// Version item
struct VersionItem {
    std::string name;
    std::string path;
    std::string deviceLabel;
    std::string userName;
    std::string sourceLabel;
    std::time_t timestamp;
    size_t size;
    bool isLocal;
    SDL_Rect rect;
    bool selected;
};

class MainUI {
public:
    MainUI(SDL_Renderer* renderer, network::Dropbox& dropbox, core::SaveManager& saveMgr);
    ~MainUI();
    
    bool initialize();
    void handleEvent(const SDL_Event& event);
    void update();
    void render();
    
    bool shouldExit() const { return m_shouldExit; }
    
private:
    SDL_Renderer* m_renderer;
    network::Dropbox& m_dropbox;
    core::SaveManager& m_saveManager;
    
    // Fonts
    TTF_Font* m_fontLarge;
    TTF_Font* m_fontMedium;
    TTF_Font* m_fontSmall;
    
    // State
    State m_state;
    bool m_shouldExit;
    int m_selectedIndex;
    
    // Game cards
    std::vector<GameCard> m_gameCards;
    
    // Version items
    std::vector<VersionItem> m_versionItems;
    int m_selectedVersionIndex;
    
    // Buttons
    std::vector<Button> m_buttons;
    
    // Auth
    std::string m_authToken;
    
    // Current game
    core::TitleInfo* m_selectedTitle;
    
    // Sync progress
    int m_syncProgress;
    int m_syncTotal;
    std::string m_syncStatus;
    
    // Touch state
    int m_touchX, m_touchY;
    bool m_touchPressed;
    
    // Dimensions
    static constexpr int CARD_WIDTH = 200;
    static constexpr int CARD_HEIGHT = 280;
    static constexpr int CARD_MARGIN = 20;
    static constexpr int HEADER_HEIGHT = 80;
    static constexpr int FOOTER_HEIGHT = 60;
    
    // Render methods
    void renderHeader();
    void renderFooter();
    void renderGameList();
    void renderGameDetail();
    void renderVersionHistory();
    void renderAuthScreen();
    void renderSyncProgress();
    
    // Helpers
    void renderText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color = Color::Text);
    void renderTextCentered(const std::string& text, int x, int y, int w, TTF_Font* font, SDL_Color color = Color::Text);
    void renderButton(const Button& btn);
    void renderCard(const GameCard& card);
    void renderSyncBadge(int x, int y, bool synced);
    
    SDL_Texture* loadIcon(const std::string& path);
    
    void updateGameCards();
    void handleTouch(int x, int y, bool pressed);
    void handleButtonPress(const Button& btn);
    
    // Actions
    void syncToDropbox();
    void downloadFromDropbox();
    void backupLocal();
    void showVersionHistory();
    void restoreVersion(VersionItem* item);
    void syncAllGames();
    void connectDropbox();
};

} // namespace ui
