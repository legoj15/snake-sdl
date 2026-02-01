#include "fps.h"
#include "render.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * fps.c
 * FPS/TPS tracking for diagnostics and window title.
 *
 * We recompute fps/tps about once per second to reduce noise, but we still
 * rewrite the title every call so end-state titles remain authoritative.
 */

#include <stdio.h>

void Fps_Init(FpsCounter *c) {
  if (!c)
    return;

  c->last_sample_ns = SDL_GetTicksNS();
  c->frame_count = 0;
  c->tick_count = 0;
  c->fps = 0.0;
  c->tps = 0.0;
}

void Fps_OnFrame(FpsCounter *c) {
  if (!c)
    return;
  c->frame_count++;
}

void Fps_OnTick(FpsCounter *c) {
  if (!c)
    return;
  c->tick_count++;
}

void Fps_UpdateWindowTitle(FpsCounter *c, SDL_Window *window, bool interp_on,
                           int score, bool game_over, bool you_win) {
  if (!c || !window)
    return;

  uint64_t now = SDL_GetTicksNS();
  uint64_t elapsed = now - c->last_sample_ns;

  // Recompute FPS/TPS roughly once per second
  if (elapsed >= 1000000000ull) {
    double secs = (double)elapsed / 1e9;

    c->fps = (secs > 0.0) ? (c->frame_count / secs) : 0.0;
    c->tps = (secs > 0.0) ? (c->tick_count / secs) : 0.0;

    c->frame_count = 0;
    c->tick_count = 0;
    c->last_sample_ns = now;
  }

  // Always write the title using the *current* game state.
  // This prevents end-state strings from being overwritten by a stale title.
  char title[260];

  if (you_win) {
    snprintf(title, sizeof(title),
             "snake-sdl | YOU WIN! \xe2\x80\x94 Continue? (L) | Score: %d | "
             "FPS: %.1f | TPS: %.1f | Interp: %s",
             score, c->fps, c->tps, interp_on ? "ON" : "OFF");
  } else if (game_over) {
    snprintf(title, sizeof(title),
             "snake-sdl | GAME OVER \xe2\x80\x94 Continue? (L) | Score: %d | "
             "FPS: %.1f | TPS: %.1f | Interp: %s",
             score, c->fps, c->tps, interp_on ? "ON" : "OFF");
  } else {
    snprintf(title, sizeof(title),
             "snake-sdl | Score: %d | FPS: %.1f | TPS: %.1f | Interp: %s",
             score, c->fps, c->tps, interp_on ? "ON" : "OFF");
  }

  SDL_SetWindowTitle(window, title);
}
