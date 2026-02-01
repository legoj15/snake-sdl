#include "death_fx.h"
#include "render.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>

/*
 * death_fx.c
 * Disintegration effect rendered after death.
 * Captures interpolation state at death to avoid visual popping.
 */

typedef struct {
  float x, y;
} FVec2;

static inline float clamp01f(float x) {
  return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

// Smoothstep (0..1 -> 0..1), gentle ease-in/out
static inline float smoothstep01(float x) {
  x = clamp01f(x);
  return x * x * (3.0f - 2.0f * x);
}

// Interpolate on a wrapping grid by taking the shortest wrapped path.
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

// Tiny integer hash used to derive deterministic per-segment "random" values.
// Tiny integer hash used to derive deterministic per-segment "random" values.
static uint32_t hash_u32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

static float deg_to_rad(float deg) { return deg * 0.01745329251994329577f; }

void DeathFx_Init(DeathFx *fx) {
  if (!fx)
    return;
  fx->active = false;
  fx->finished = false;
  fx->start_ns = 0;
  fx->death_alpha = 1.0f;
  fx->interp_mode = true;
  fx->seed = 0x12345678U;

  // slower + readable
  fx->stagger_s = 0.090f;
  fx->seg_dur_s = 0.700f;
  fx->max_rot_deg = 22.0f;
}

void DeathFx_Start(DeathFx *fx, bool interp_mode, float death_alpha,
                   uint64_t now_ns) {
  if (!fx)
    return;
  fx->active = true;
  fx->finished = false;
  fx->start_ns = now_ns;
  fx->interp_mode = interp_mode;
  fx->death_alpha = clamp01f(death_alpha);

  fx->seed = (uint32_t)(now_ns ^ (now_ns >> 32) ^ 0xA5A5A5A5U);
}

void DeathFx_RenderAndAdvance(DeathFx *fx, const App *app, const Snake *snake,
                              uint64_t now_ns) {
  if (!fx || !app || !snake || !fx->active || fx->finished)
    return;

  float t = (float)((double)(now_ns - fx->start_ns) / 1e9);

  float total = (snake->len > 0)
                    ? ((snake->len - 1) * fx->stagger_s + fx->seg_dur_s)
                    : 0.0f;

  if (t >= total + 0.10f) {
    fx->finished = true;
    return;
  }

  const uint8_t base_r = 0, base_g = 200, base_b = 0;

  // Animate head-to-tail with a stagger so the snake breaks apart
  // progressively.
  for (int i = 0; i < snake->len; i++) {
    float ti = t - (float)i * fx->stagger_s;
    float p = clamp01f(ti / fx->seg_dur_s);

    if (ti < 0.0f)
      p = 0.0f;
    if (p >= 1.0f)
      continue;

    // Scale + fade (with gentle easing)
    float scale = 1.0f - p;
    float fade = 1.0f - p;

    scale = scale * scale;
    fade = fade * (1.0f - 0.15f * p);

    // Rotation target: deterministic per segment
    uint32_t h = hash_u32((uint32_t)i ^ fx->seed);
    float u = (float)(h & 0xFFFF) / 65535.0f; // 0..1
    float target_deg = (u * 2.0f - 1.0f) * fx->max_rot_deg;

    // IMPORTANT: rotation should not appear instantly.
    // Ease rotation in as the segment disintegrates.
    float rot_t = smoothstep01((p - 0.25f) / 0.75f);
    float rot_deg = target_deg * rot_t;

    // Position based on interp mode frozen at death time
    float gx, gy;
    if (fx->interp_mode) {
      float a = fx->death_alpha;
      gx = wrap_interp((float)snake->prev[i].x, (float)snake->seg[i].x,
                       snake->grid_w, a);
      gy = wrap_interp((float)snake->prev[i].y, (float)snake->seg[i].y,
                       snake->grid_h, a);
    } else {
      gx = (float)snake->seg[i].x;
      gy = (float)snake->seg[i].y;
    }

    FVec2 c = grid_to_px_center(app, gx, gy);

    float w = (float)app->cell_w * scale;
    float hgt = (float)app->cell_h * scale;

    uint8_t a8 = (uint8_t)(255.0f * clamp01f(fade));

    Render_QuadCenteredPx(app, c.x, c.y, w, hgt, deg_to_rad(rot_deg), base_r,
                          base_g, base_b, a8);
  }
}
