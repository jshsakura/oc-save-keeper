/**
 * OC Save Keeper
 * Main UI
 */

#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <string>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "core/SaveManager.hpp"
#include "network/Dropbox.hpp"
#include "utils/Logger.hpp"

namespace ui {

struct ColorPalette {
    SDL_Color Background;
    SDL_Color Header;
    SDL_Color Card;
    SDL_Color CardHover;
    SDL_Color Accent;
    SDL_Color AccentSoft;
    SDL_Color Warning;
    SDL_Color Error;
    SDL_Color Text;
    SDL_Color TextDim;
    SDL_Color Synced;
    SDL_Color NotSynced;
    SDL_Color Border;
    SDL_Color BorderStrong;
    SDL_Color Poster;
    SDL_Color TitleStrip;
    SDL_Color Shadow;
    SDL_Color SelectionGlow;
};

namespace Theme {
    ColorPalette Light();
    ColorPalette Dark();
}

// UI State
enum class State {
    Main,           // Game list
    GameDetail,     // Selected game options
    VersionHistory, // Version list
    CloudPicker,    // Remote download picker
    UserPicker,     // User selection
    Auth,           // Dropbox token input
    SyncAll         // Batch sync progress
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
    enum class SyncState {
        Unknown,
        Disconnected,
        LocalOnly,
        UpToDate,
        NeedsUpload,
        CloudNewer
    };

    core::TitleInfo* title;
    SDL_Rect rect;
    SDL_Texture* icon;
    bool selected;
    bool synced;
    SyncState syncState;
    std::string syncLabel;
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

struct UserChip {
    core::UserInfo* user;
    SDL_Rect rect;
    bool selected;
};

class MainUI {
public:
    MainUI(SDL_Renderer* renderer, network::Dropbox& dropbox, core::SaveManager& saveMgr);
    ~MainUI();
    
    bool initialize();
    void handleEvent(const SDL_Event& event);
#ifdef __SWITCH__
    void handlePadButtons(u64 keysDown);
#endif
    void update();
    void render();
    
    bool shouldExit() const { return m_shouldExit; }
    
private:
    SDL_Renderer* m_renderer;
    network::Dropbox& m_dropbox;
    core::SaveManager& m_saveManager;
    
    // Palette
    ColorPalette m_colors;
    
    // Fonts
    TTF_Font* m_fontLarge;
    TTF_Font* m_fontMedium;
    TTF_Font* m_fontSmall;
#ifdef __SWITCH__
    bool m_plInitialized;
    AppletFocusState m_lastFocusState;
#endif
    
    // State
    State m_state;
    bool m_shouldExit;
    int m_selectedIndex;
    
    // Game cards
    std::vector<GameCard> m_gameCards;
    
    // Version items
    std::vector<VersionItem> m_versionItems;
    std::vector<UserChip> m_userChips;
    int m_selectedVersionIndex;
    int m_selectedUserIndex;
    int m_selectedButtonIndex;
    // Buttons
    std::vector<Button> m_buttons;
    SDL_Rect m_authTokenBox;
    SDL_Rect m_languageButton;
    SDL_Rect m_refreshButton;
    SDL_Rect m_statusButton;
    SDL_Rect m_userButton;
    
    // Current game
    core::TitleInfo* m_selectedTitle;
    
    // Auth
    std::string m_authToken;
    std::string m_authUrl;
    bool m_authSessionStarted;
    // Sync progress
    int m_syncProgress;
    int m_syncTotal;
    std::string m_syncStatus;
    
    // Touch state
    int m_touchX, m_touchY;
    bool m_touchPressed;

    // Output size
    int m_screenWidth;
    int m_screenHeight;
    int m_gridCols;
    int m_gridRows;
    int m_scrollRow;
    float m_scrollOffset; // For smooth scrolling animation
    
    // Dimensions
    static constexpr int CARD_WIDTH = 190;
    static constexpr int CARD_HEIGHT = 252;
    static constexpr int CARD_MARGIN = 12;
    static constexpr int HEADER_HEIGHT = 90; // Slightly slimmer
    static constexpr int FOOTER_HEIGHT = 64; // Slightly slimmer
    
    // Render methods
    void renderHeader();
    void renderFooter();
    void renderGameList();
    void renderGameDetail();
    void renderVersionHistory();
    void renderAuthScreen();
    void renderUserPicker();
    void renderSyncProgress();
    void renderScrollBar();
    void renderSelectionGlow(const SDL_Rect& rect);
    
    // Premium UI helpers (The peak of beauty)
    void renderRoundedRect(const SDL_Rect& rect, int radius, SDL_Color color);
    void renderFilledRoundedRect(const SDL_Rect& rect, int radius, SDL_Color color);
    void renderVerticalGradient(const SDL_Rect& rect, SDL_Color top, SDL_Color bottom);
    void renderGlassPanel(const SDL_Rect& rect, int radius, SDL_Color baseColor, bool hasRimLight = true);
    void renderAuraBackground();
    void renderSoftShadow(const SDL_Rect& rect, int radius, int spread, SDL_Color color, int offsetY = 0);
    
    // Helpers
    void renderText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color = SDL_Color{32, 34, 39, 255});
    void renderTextCentered(const std::string& text, int x, int y, int w, TTF_Font* font, SDL_Color color = SDL_Color{32, 34, 39, 255});
    void renderTextWithShadow(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color);

    
    // Animation state for selected items
    float m_selectionScale = 1.0f;
    float m_selectionAlpha = 0.0f;
    
    void renderButton(const Button& btn);
    void renderCard(const GameCard& card, float scale = 1.0f);
    void renderSyncBadge(int x, int y, bool synced);
    void renderIcon(SDL_Texture* texture, const SDL_Rect& rect, int radius, bool selected);
    std::string fitText(TTF_Font* font, const std::string& text, int maxWidth) const;
    
    SDL_Texture* loadIcon(const std::string& path);
    
    void updateGameCards();
    void refreshSyncStates(bool autoUpload = false);
    void handleTouch(int x, int y, bool pressed);
    void handleButtonPress(const Button& btn);
    void toggleLanguage();
    void clampSelection();
    int getItemsPerPage() const;
    int getVisibleStartIndex() const;
    
    // Actions
    void syncToDropbox();
    void backupLocal();
    void showVersionHistory();
    void showCloudPicker();
    void downloadCloudItem(VersionItem* item);
    void restoreVersion(VersionItem* item);
    void showUserPicker();
    void selectUser(size_t index);
    void syncAllGames();
    void connectDropbox();
    void openTokenKeyboard();
};

} // namespace ui
