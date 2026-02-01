#pragma once

#include "app.h"
#include "snake.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * render.h
 *
 * Low-level SDL_Renderer helpers.
 */

struct SDL_Renderer;

// Clears the screen to the background color.
void Render_Clear(struct SDL_Renderer *renderer);

// Draws a filled cell at grid position p.
void Render_CellFilled(const App *app, IVec2 p, uint8_t r, uint8_t g,
                       uint8_t b);

// Draws the standard grid lines.
void Render_GridLines(const App *app);

// Draws grid lines with a specific color.
void Render_GridLinesEx(const App *app, uint8_t r, uint8_t g, uint8_t b,
                        uint8_t a);

// Presents the backbuffer.
void Render_Present(struct SDL_Renderer *renderer);

// Centered quad (used for death effect)
void Render_QuadCenteredPx(const App *app, float cx, float cy, float w, float h,
                           float rotation, uint8_t r, uint8_t g, uint8_t b,
                           uint8_t a);

// Centered rect (used for snake drawing)
void Render_RectFilledPx(const App *app, float cx, float cy, float w, float h,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t a);

#ifdef __cplusplus
}
#endif
