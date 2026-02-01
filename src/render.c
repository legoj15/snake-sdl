#include "render.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * render.c
 * Centralized SDL drawing helpers used by gameplay + effects.
 *
 * Note: Render_QuadCenteredPx uses SDL_RenderGeometry so we can rotate quads
 * without setting up textures or a separate math layer.
 */

void Render_Clear(SDL_Renderer *r) {
  SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
  SDL_RenderClear(r);
}

void Render_Present(SDL_Renderer *r) { SDL_RenderPresent(r); }

void Render_CellFilled(const App *app, IVec2 grid_pos, Uint8 r, Uint8 g,
                       Uint8 b) {
  if (!app || !app->renderer)
    return;

  SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);

  SDL_FRect rect;
  rect.x = (float)(grid_pos.x * app->cell_w);
  rect.y = (float)(grid_pos.y * app->cell_h);
  rect.w = (float)app->cell_w;
  rect.h = (float)app->cell_h;

  SDL_RenderFillRect(app->renderer, &rect);
}

void Render_CellFilledF(const App *app, float gx, float gy, uint8_t r,
                        uint8_t g, uint8_t b) {
  if (!app || !app->renderer)
    return;

  SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);

  SDL_FRect rect = {.x = gx * (float)app->cell_w,
                    .y = gy * (float)app->cell_h,
                    .w = (float)app->cell_w,
                    .h = (float)app->cell_h};

  SDL_RenderFillRect(app->renderer, &rect);
}

void Render_RectFilledPx(const App *app, float x, float y, float w, float h,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if (!app || !app->renderer)
    return;

  SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(app->renderer, r, g, b, a);

  SDL_FRect rect = {x, y, w, h};
  SDL_RenderFillRect(app->renderer, &rect);
}

void Render_QuadCenteredPx(const App *app, float cx, float cy, float w, float h,
                           float angle_rad, uint8_t r, uint8_t g, uint8_t b,
                           uint8_t a) {
  if (!app || !app->renderer)
    return;

  SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);

  float hw = w * 0.5f;
  float hh = h * 0.5f;

  float c = cosf(angle_rad);
  float s = sinf(angle_rad);

  SDL_FPoint p[4];
  p[0].x = cx + (-hw * c - -hh * s);
  p[0].y = cy + (-hw * s + -hh * c);

  p[1].x = cx + (hw * c - -hh * s);
  p[1].y = cy + (hw * s + -hh * c);

  p[2].x = cx + (hw * c - hh * s);
  p[2].y = cy + (hw * s + hh * c);

  p[3].x = cx + (-hw * c - hh * s);
  p[3].y = cy + (-hw * s + hh * c);

  SDL_Vertex v[4];
  for (int i = 0; i < 4; i++) {
    v[i].position = p[i];
    v[i].color.r = r;
    v[i].color.g = g;
    v[i].color.b = b;
    v[i].color.a = a;
    v[i].tex_coord.x = 0.0f;
    v[i].tex_coord.y = 0.0f;
  }

  const int idx[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(app->renderer, NULL, v, 4, idx, 6);
}

void Render_GridLinesEx(const App *app, uint8_t r, uint8_t g, uint8_t b,
                        uint8_t a) {
  if (!app || !app->renderer)
    return;

  SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(app->renderer, r, g, b, a);

  for (int x = 0; x <= app->grid_w; x++) {
    const int px = x * app->cell_w;
    SDL_RenderLine(app->renderer, (float)px, 0.0f, (float)px,
                   (float)app->window_h);
  }

  for (int y = 0; y <= app->grid_h; y++) {
    const int py = y * app->cell_h;
    SDL_RenderLine(app->renderer, 0.0f, (float)py, (float)app->window_w,
                   (float)py);
  }
}

void Render_GridLines(const App *app) {
  // original look
  Render_GridLinesEx(app, 40, 40, 40, 255);
}
