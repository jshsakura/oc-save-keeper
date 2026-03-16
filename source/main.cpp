/**
 * oc-save-keeper - Safe save backup and sync for Nintendo Switch
 * Main entry point
 */

#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <curl/curl.h>
#include <sys/stat.h>

#include "core/SaveManager.hpp"
#include "network/Dropbox.hpp"
#include "ui/saves/Runtime.hpp"
#include "ui/saves/SaveShell.hpp"
#include "utils/Language.hpp"
#include "utils/Logger.hpp"
#include "utils/Paths.hpp"

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;

static bool g_running = true;
static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
#ifdef __SWITCH__
static PadState g_pad;
#endif

namespace dropkeep {

bool initialize() {
#ifdef __SWITCH__
    Result rc = socketInitializeDefault();
    if (R_FAILED(rc)) {
        LOG_ERROR("socketInitializeDefault failed: 0x%x", rc);
        return false;
    }
    padInitializeDefault(&g_pad);
    
    rc = romfsInit();
    if (R_FAILED(rc)) {
        LOG_ERROR("romfsInit failed: 0x%x", rc);
        return false;
    }
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    
#ifdef __SWITCH__
    g_window = SDL_CreateWindow(
        "oc-save-keeper",
        0,
        0,
        0,
        0,
        SDL_WINDOW_FULLSCREEN_DESKTOP
    );
#else
    g_window = SDL_CreateWindow(
        "oc-save-keeper",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
#endif
    
    if (!g_window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    if (SDL_RenderSetLogicalSize(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT) < 0) {
        LOG_ERROR("SDL_RenderSetLogicalSize failed: %s", SDL_GetError());
        return false;
    }

    if (SDL_RenderSetIntegerScale(g_renderer, SDL_FALSE) < 0) {
        LOG_ERROR("SDL_RenderSetIntegerScale failed: %s", SDL_GetError());
        return false;
    }
    
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        LOG_ERROR("IMG_Init failed: %s", IMG_GetError());
        return false;
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        SDL_JoystickOpen(i);
    }
    
    utils::paths::ensureBaseDirectories();
    utils::Language::instance().init();

#ifdef __SWITCH__
    if (appletGetAppletType() != AppletType_Application) {
        LOG_WARNING("Running in Applet Mode. Some features might be restricted.");
    }
#endif

    return true;
}

void cleanup() {
#ifdef __SWITCH__
    accountExit();
    romfsExit();
    socketExit();
#endif
    curl_global_cleanup();
    IMG_Quit();
    
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = nullptr;
    }
    
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }
    
    SDL_Quit();
}

void run() {
    network::Dropbox dropbox;
    
    core::SaveManager saveManager;
    if (!saveManager.initialize()) {
        LOG_ERROR("Failed to initialize SaveManager");
        return;
    }
    
    ui::saves::SaveShell mainUI(g_renderer, dropbox, saveManager);
    if (!mainUI.initialize()) {
        LOG_ERROR("Failed to initialize UI");
        return;
    }
    
    ui::saves::Runtime::instance().setRenderer(g_renderer);
    
    // Main loop
    SDL_Event event;
    while (g_running && appletMainLoop()) {
#ifdef __SWITCH__
        padUpdate(&g_pad);
        const u64 keysDown = padGetButtonsDown(&g_pad);
        mainUI.handlePadButtons(keysDown);
        if (keysDown & (HidNpadButton_Plus | HidNpadButton_Minus)) {
            g_running = false;
        }
#endif

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                g_running = false;
            }
            mainUI.handleEvent(event);
        }
        
        mainUI.update();
        
        SDL_SetRenderDrawColor(g_renderer, 30, 30, 40, 255);
        SDL_RenderClear(g_renderer);
        mainUI.render();
        SDL_RenderPresent(g_renderer);
        
        if (mainUI.shouldExit()) {
            g_running = false;
        }
    }
}

} // namespace dropkeep

int main(int argc, char* argv[]) {
    if (!dropkeep::initialize()) {
        dropkeep::cleanup();
        return -1;
    }
    
    dropkeep::run();
    dropkeep::cleanup();
    
    return 0;
}
