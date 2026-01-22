#include "app.h"
#include <stdbool.h>


/*
 * app.c
 * SDL startup/shutdown + grid sizing.
 * App is the shared hub other modules depend on.
 */


static int clamp_min(int v, int minv) { return (v < minv) ? minv : v; }

static bool App_InitAudioWithFallbacks(void) {
    #ifdef _WIN32
    const char* drivers[] = {"wasapi", "directsound", "winmm"};
    #else
    const char* drivers[] = {"pipewire", "pulseaudio", "alsa", "jack", "oss"};
    #endif
    const int driver_count = (int)(sizeof(drivers) / sizeof(drivers[0]));

    for (int i = 0; i < driver_count; i++) {
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, drivers[i]);
        if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            SDL_Log("SDL audio backend: %s", drivers[i]);
            return true;
        }
        SDL_Log("SDL audio init failed for %s: %s", drivers[i], SDL_GetError());
    }

    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, NULL);
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("SDL audio backend: default");
        return true;
    }

    SDL_Log("SDL audio init failed (continuing without audio): %s",
            SDL_GetError());
    return false;
}

bool App_Init(App* app, int window_w, int window_h, int grid_w, int grid_h) {
    if (!app) return false;

    app->window_w = window_w;
    app->window_h = window_h;
    app->grid_w = clamp_min(grid_w, 1);
    app->grid_h = clamp_min(grid_h, 1);

    app->cell_w = window_w / app->grid_w;
    app->cell_h = window_h / app->grid_h;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    App_InitAudioWithFallbacks();

    app->window = SDL_CreateWindow("snake-sdl", window_w, window_h, 0);
    if (!app->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    app->renderer = SDL_CreateRenderer(app->window, NULL);
    if (!app->renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(app->window);
        SDL_Quit();
        return false;
    }

    return true;
}

void App_Shutdown(App* app) {
    if (!app) return;
    if (app->renderer) SDL_DestroyRenderer(app->renderer);
    if (app->window) SDL_DestroyWindow(app->window);
    app->renderer = NULL;
    app->window = NULL;
    SDL_Quit();
}
