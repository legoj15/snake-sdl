#pragma once

/*
 * app.h
 *
 * This module is the “SDL glue” for the game.
 *
 * The rest of the codebase mostly wants:
 *   - a renderer to draw into
 *   - window dimensions (pixels)
 *   - grid dimensions (cells)
 *   - derived cell size (pixels per cell)
 *
 * Bundling those together in App keeps the rendering + gameplay modules from
 * growing long parameter lists and makes it hard to accidentally mix units
 * (grid coords vs pixel coords).
 */

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // Window size in pixels.
  int window_w;
  int window_h;

  // Board size in grid cells.
  int grid_w;
  int grid_h;

  // Derived pixel size of a single cell. (Computed from window/grid sizes.)
  int cell_w;
  int cell_h;

  // Owned SDL objects.
  SDL_Window *window;
  SDL_Renderer *renderer;

  bool is_debug;
} App;

// Creates the SDL window/renderer and computes grid→pixel scaling.
// Returns false on failure and logs the SDL error.
bool App_Init(App *app, int window_w, int window_h, int grid_w, int grid_h);

// Destroys SDL resources created by App_Init.
void App_Shutdown(App *app);

#ifdef __cplusplus
}
#endif
