/**
 * oc-save-keeper - Safe save backup and sync for Nintendo Switch
 * Main entry point
 */

#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <curl/curl.h>
#include <cstdio>
#include <sys/stat.h>

#include "core/SaveManager.hpp"
#include "network/Dropbox.hpp"
#include "ui/MainUI.hpp"
#include "utils/Logger.hpp"

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;

static bool g_running = true;
static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
#ifdef __SWITCH__
static PadState g_pad;
#endif

namespace dropkeep {

namespace {

void logBootStage(const char* stage) {
    mkdir("/switch/oc-save-keeper", 0777);
    mkdir("/switch/oc-save-keeper/logs", 0777);

    FILE* file = fopen("/switch/oc-save-keeper/logs/boot.log", "a");
    if (!file) {
        return;
    }

    std::fprintf(file, "%s\n", stage);
    fclose(file);
}

} // namespace

bool initialize() {
#ifdef __SWITCH__
    padInitializeDefault(&g_pad);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        logBootStage("boot: SDL_Init failed");
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    logBootStage("boot: SDL_Init ok");
    
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
        logBootStage("boot: SDL_CreateWindow failed");
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    logBootStage("boot: SDL_CreateWindow ok");
    
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        logBootStage("boot: SDL_CreateRenderer failed");
        LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    logBootStage("boot: SDL_CreateRenderer ok");
    
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        logBootStage("boot: IMG_Init failed");
        LOG_ERROR("IMG_Init failed: %s", IMG_GetError());
        return false;
    }
    logBootStage("boot: IMG_Init ok");
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        SDL_JoystickOpen(i);
    }
    
    // Create directories
    mkdir("/switch/oc-save-keeper", 0777);
    mkdir("/switch/oc-save-keeper/backups", 0777);
    mkdir("/switch/oc-save-keeper/logs", 0777);
    logBootStage("boot: directories ready");
    
    LOG_INFO("oc-save-keeper initialized");
    return true;
}

void cleanup() {
    LOG_INFO("Shutting down oc-save-keeper...");
    
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
    logBootStage("boot: run start");
    // Initialize Dropbox
    network::Dropbox dropbox;
    logBootStage("boot: Dropbox constructed");
    
    // Initialize save manager
    core::SaveManager saveManager;
    if (!saveManager.initialize()) {
        logBootStage("boot: SaveManager initialize failed");
        LOG_ERROR("Failed to initialize SaveManager");
        return;
    }
    logBootStage("boot: SaveManager initialize ok");
    
    // Initialize UI
    ui::MainUI mainUI(g_renderer, dropbox, saveManager);
    if (!mainUI.initialize()) {
        logBootStage("boot: MainUI initialize failed");
        LOG_ERROR("Failed to initialize UI");
        return;
    }
    logBootStage("boot: MainUI initialize ok");
    
    // Main loop
    SDL_Event event;
    while (g_running && appletMainLoop()) {
#ifdef __SWITCH__
        padUpdate(&g_pad);
        const u64 keysDown = padGetButtonsDown(&g_pad);
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
