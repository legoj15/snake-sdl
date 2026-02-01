#pragma once

#include "app.h"
#include "snake.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "render.h"

// Settings for how the snake is drawn.
typedef struct SnakeDrawStyle {
  bool snap_head;    // If true, the head doesn't interpolate.
  bool draw_bridges; // If true, we draw solid blocks between segments.

  uint8_t head_r, head_g, head_b;
  uint8_t body_r, body_g, body_b;
} SnakeDrawStyle;

// Render the snake to the screen.
void SnakeDraw_Render(const App *app, const Snake *snake, float alpha,
                      SnakeDrawStyle style);

#ifdef __cplusplus
}
#endif
