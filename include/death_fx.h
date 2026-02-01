#pragma once

#include "app.h"
#include "snake.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * death_fx.h
 *
 * Visual feedback for when the snake dies.
 */

typedef struct DeathFx {
  bool active;
  bool finished;

  uint64_t start_ns;
  float death_alpha;
  bool interp_mode;
  uint32_t seed;

  float stagger_s;
  float seg_dur_s;
  float max_rot_deg;
} DeathFx;

// Initializes the death effect structure.
void DeathFx_Init(DeathFx *fx);

// Starts the death animation.
void DeathFx_Start(DeathFx *fx, bool interp_mode, float death_alpha,
                   uint64_t now_ns);

// Advanced the disintegration animation and renders to the screen.
void DeathFx_RenderAndAdvance(DeathFx *fx, const App *app, const Snake *snake,
                              uint64_t now_ns);

#ifdef __cplusplus
}
#endif
