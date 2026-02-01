#include "snake_draw.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

/*
 * snake_draw.c
 * Snake rendering with interpolation + wrap-aware bridging.
 */

typedef struct {
  float x, y;
} FVec2;

static inline float clamp01f(float x) {
  return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

// Interpolate on a wrapping grid, taking shortest wrapped path.
static float wrap_interp(float prev, float curr, int size, float a) {
  float d = curr - prev;

  if (d > (float)size / 2.0f)
    d -= (float)size;
  if (d < -(float)size / 2.0f)
    d += (float)size;

  float v = prev + d * a;

  while (v < 0.0f)
    v += (float)size;
  while (v >= (float)size)
    v -= (float)size;

  return v;
}

static FVec2 grid_to_px_center(const App *app, float gx, float gy) {
  FVec2 c;
  c.x = gx * (float)app->cell_w + (float)app->cell_w * 0.5f;
  c.y = gy * (float)app->cell_h + (float)app->cell_h * 0.5f;
  return c;
}

// Make b the nearest wrapped version of b relative to a in pixel space.
static FVec2 nearest_wrapped_px(const App *app, FVec2 a, FVec2 b) {
  float w = (float)app->window_w;
  float h = (float)app->window_h;

  float dx = b.x - a.x;
  float dy = b.y - a.y;

  if (dx > w * 0.5f)
    b.x -= w;
  if (dx < -w * 0.5f)
    b.x += w;

  if (dy > h * 0.5f)
    b.y -= h;
  if (dy < -h * 0.5f)
    b.y += h;

  return b;
}

// Draw a filled rect that wraps around screen edges (splits if needed).
static void draw_wrapped_rect(const App *app, float x, float y, float w,
                              float h, uint8_t r, uint8_t g, uint8_t b) {
  float W = (float)app->window_w;
  float H = (float)app->window_h;

  // normalize into a "reasonable" range for splitting
  while (x < -W)
    x += W;
  while (x > 2 * W)
    x -= W;
  while (y < -H)
    y += H;
  while (y > 2 * H)
    y -= H;

  // Split horizontally if needed
  if (x < 0.0f) {
    float left_w = -x;
    Render_RectFilledPx(app, 0.0f, y, w - left_w, h, r, g, b, 255);
    Render_RectFilledPx(app, W - left_w, y, left_w, h, r, g, b, 255);
    return;
  }
  if (x + w > W) {
    float right_w = (x + w) - W;
    Render_RectFilledPx(app, x, y, w - right_w, h, r, g, b, 255);
    Render_RectFilledPx(app, 0.0f, y, right_w, h, r, g, b, 255);
    return;
  }

  // Split vertically if needed
  if (y < 0.0f) {
    float top_h = -y;
    Render_RectFilledPx(app, x, 0.0f, w, h - top_h, r, g, b, 255);
    Render_RectFilledPx(app, x, H - top_h, w, top_h, r, g, b, 255);
    return;
  }
  if (y + h > H) {
    float bot_h = (y + h) - H;
    Render_RectFilledPx(app, x, y, w, h - bot_h, r, g, b, 255);
    Render_RectFilledPx(app, x, 0.0f, w, bot_h, r, g, b, 255);
    return;
  }

  Render_RectFilledPx(app, x, y, w, h, r, g, b, 255);
}

static void draw_h_bridge(const App *app, FVec2 a, FVec2 b, uint8_t r,
                          uint8_t g, uint8_t bl) {
  float cw = (float)app->cell_w;
  float ch = (float)app->cell_h;

  float x0 = a.x - cw * 0.5f;
  float x1 = b.x - cw * 0.5f;

  float x = fminf(x0, x1);
  float w = fabsf(x1 - x0) + cw;

  float y = a.y - ch * 0.5f;
  float h = ch;

  draw_wrapped_rect(app, x, y, w, h, r, g, bl);
}

static void draw_v_bridge(const App *app, FVec2 a, FVec2 b, uint8_t r,
                          uint8_t g, uint8_t bl) {
  float cw = (float)app->cell_w;
  float ch = (float)app->cell_h;

  float y0 = a.y - ch * 0.5f;
  float y1 = b.y - ch * 0.5f;

  float y = fminf(y0, y1);
  float h = fabsf(y1 - y0) + ch;

  float x = a.x - cw * 0.5f;
  float w = cw;

  draw_wrapped_rect(app, x, y, w, h, r, g, bl);
}

// Wrap-aware integer delta between two grid coords (expects step of -1/0/1).
static int wrap_delta_i(int prev, int curr, int size) {
  int d = curr - prev;
  if (d > size / 2)
    d -= size;
  if (d < -size / 2)
    d += size;
  return d;
}

// Draw an L-shaped bridge between centers using the correct elbow.
// horiz_first determines which leg is drawn first at corners.
static void draw_bridge_L(const App *app, FVec2 c0, FVec2 c1, bool horiz_first,
                          uint8_t r, uint8_t g, uint8_t bl) {
  c1 = nearest_wrapped_px(app, c0, c1);

  float dx = c1.x - c0.x;
  float dy = c1.y - c0.y;

  if (fabsf(dx) < 0.001f) {
    draw_v_bridge(app, c0, c1, r, g, bl);
    return;
  }
  if (fabsf(dy) < 0.001f) {
    draw_h_bridge(app, c0, c1, r, g, bl);
    return;
  }

  if (horiz_first) {
    FVec2 elbow = {c1.x, c0.y};
    draw_h_bridge(app, c0, elbow, r, g, bl);
    draw_v_bridge(app, elbow, c1, r, g, bl);
  } else {
    FVec2 elbow = {c0.x, c1.y};
    draw_v_bridge(app, c0, elbow, r, g, bl);
    draw_h_bridge(app, elbow, c1, r, g, bl);
  }
}

void SnakeDraw_Render(const App *app, const Snake *snake, float alpha,
                      SnakeDrawStyle style) {
  if (!app || !app->renderer || !snake || !snake->seg || !snake->prev)
    return;
  if (snake->len <= 0)
    return;

  alpha = clamp01f(alpha);

  int n = snake->len;
  if (n <= 0)
    return;

  FVec2 *centers_px = (FVec2 *)malloc((size_t)n * sizeof(*centers_px));
  if (!centers_px)
    return;

  for (int i = 0; i < n; i++) {
    float gx, gy;

    if (i == 0 && style.snap_head) {
      gx = (float)snake->seg[0].x;
      gy = (float)snake->seg[0].y;
    } else {
      gx = wrap_interp((float)snake->prev[i].x, (float)snake->seg[i].x,
                       snake->grid_w, alpha);
      gy = wrap_interp((float)snake->prev[i].y, (float)snake->seg[i].y,
                       snake->grid_h, alpha);
    }

    centers_px[i] = grid_to_px_center(app, gx, gy);
  }

  // Bridges first so segments sit on top and wrap seams look continuous.
  if (style.draw_bridges) {
    for (int i = 1; i < n; i++) {
      int dx = wrap_delta_i(snake->prev[i - 1].x, snake->seg[i - 1].x,
                            snake->grid_w);
      int dy = wrap_delta_i(snake->prev[i - 1].y, snake->seg[i - 1].y,
                            snake->grid_h);
      bool horiz_first = (dx != 0);

      if (dx == 0 && dy == 0)
        horiz_first = true;

      draw_bridge_L(app, centers_px[i - 1], centers_px[i], horiz_first,
                    style.body_r, style.body_g, style.body_b);
    }
  }

  // ---- Draw head
  if (style.snap_head) {
    // snapped head (classic look)
    Render_CellFilled(app, snake->seg[0], style.head_r, style.head_g,
                      style.head_b);
  } else {
    // interpolated head (use computed pixel center like body)
    float x = centers_px[0].x - (float)app->cell_w * 0.5f;
    float y = centers_px[0].y - (float)app->cell_h * 0.5f;
    Render_RectFilledPx(app, x, y, (float)app->cell_w, (float)app->cell_h,
                        style.head_r, style.head_g, style.head_b, 255);
  }

  // ---- Draw body squares at centers
  for (int i = 1; i < n; i++) {
    float x = centers_px[i].x - (float)app->cell_w * 0.5f;
    float y = centers_px[i].y - (float)app->cell_h * 0.5f;
    Render_RectFilledPx(app, x, y, (float)app->cell_w, (float)app->cell_h,
                        style.body_r, style.body_g, style.body_b, 255);
  }

  free(centers_px);
}
