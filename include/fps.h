#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * fps.h
 *
 * Frame-rate monitoring and window title updates.
 */

struct SDL_Window;

typedef struct FpsCounter {
  uint64_t last_sample_ns;
  int frame_count;
  int tick_count;

  float fps;
  float tps;
} FpsCounter;

void Fps_Init(FpsCounter *fps);
void Fps_OnFrame(FpsCounter *fps);
void Fps_OnTick(FpsCounter *fps);

// Updates the SDL window title with current stats (FPS, score, interp status).
void Fps_UpdateWindowTitle(FpsCounter *fps, struct SDL_Window *window,
                           bool interp, int score, bool game_over,
                           bool you_win);

#ifdef __cplusplus
}
#endif
