#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "apple.h"
#include "bot.h"
#include "death_fx.h"
#include "events.h"
#include "fps.h"
#include "render.h"
#include "snake.h"
#include "snake_draw.h"

/*
 * main.c
 * Game loop: fixed-tick simulation + optional interpolated rendering.
 * Tick rate ramps with score; render rate is capped separately.
 */

#define WINDOW_W 800
#define WINDOW_H 600
#define GRID_W 40
#define GRID_H 30

#define BASE_TICK_HZ 7
#define RAMP_EVERY 3
#define MAX_TICK_HZ 20

#define RENDER_CAP_HZ 240

#define DEBUG_START_LONG 0
#define DEBUG_START_LEN 20

// Above this TPS, disable "snappy head" and interpolate the whole snake
#define FULL_INTERP_TPS 12

static inline uint64_t ns_from_hz(int hz) {
  return (hz > 0) ? (1000000000ull / (uint64_t)hz) : 0;
}

static inline double clamp01(double x) {
  return (x < 0.0) ? 0.0 : (x > 1.0) ? 1.0 : x;
}

static inline int clampi(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static int tick_hz_for_score(int score) {
  int hz = BASE_TICK_HZ + (score / RAMP_EVERY);
  return clampi(hz, 1, MAX_TICK_HZ);
}

static bool snake_hit_self(const Snake *s) {
  if (!s || s->len <= 1)
    return false;
  IVec2 head = s->seg[0];
  for (int i = 1; i < s->len; i++) {
    if (s->seg[i].x == head.x && s->seg[i].y == head.y) {
      return true;
    }
  }
  return false;
}

static void debug_make_snake_long(Snake *s, int desired_len) {
  if (!s || !s->seg || !s->prev)
    return;

  int len = desired_len;
  if (len < 1)
    len = 1;
  if (len > s->max_len)
    len = s->max_len;

  IVec2 head = s->seg[0];

  int bx = 0, by = 0;
  switch (s->dir) {
  case DIR_UP:
    by = 1;
    break;
  case DIR_DOWN:
    by = -1;
    break;
  case DIR_LEFT:
    bx = 1;
    break;
  case DIR_RIGHT:
    bx = -1;
    break;
  }

  s->len = len;
  for (int i = 0; i < s->len; i++) {
    int x = head.x + bx * i;
    int y = head.y + by * i;

    while (x < 0)
      x += s->grid_w;
    while (x >= s->grid_w)
      x -= s->grid_w;
    while (y < 0)
      y += s->grid_h;
    while (y >= s->grid_h)
      y -= s->grid_h;

    s->seg[i].x = x;
    s->seg[i].y = y;
    s->prev[i] = s->seg[i];
  }

  s->grow = 0;
}

static void Game_Reset(Snake *snake, Apple *apple, int *score, int *tick_hz,
                       uint64_t *tick_ns, uint64_t *acc, bool *game_over,
                       bool *you_win, bool *interp, bool interp_setting,
                       DeathFx *death_fx, const App *app) {
  Snake_Destroy(snake);

  Dir start_dir = (Dir)SDL_rand(4);
  Snake_Init(snake, app->grid_w, app->grid_h, app->grid_w * app->grid_h,
             start_dir);

#if DEBUG_START_LONG
  debug_make_snake_long(snake, DEBUG_START_LEN);
#endif

  *score = snake->len - 1;
  if (*score < 0)
    *score = 0;

  Apple_Init(apple, snake);

  *tick_hz = tick_hz_for_score(*score);
  *tick_ns = ns_from_hz(*tick_hz);
  *acc = 0;

  *game_over = false;
  *you_win = false;

  // Preserve the player's interpolation toggle across rounds.
  // (A reset should restart gameplay, not silently change their render
  // preference.)
  *interp = interp_setting;

  DeathFx_Init(death_fx);
}

static void SetEndTitle(SDL_Window *window, bool you_win, int score) {
  if (!window)
    return;
  char title[256];
  if (you_win) {
    snprintf(title, sizeof(title),
             "snake-sdl | YOU WIN! - Continue? (L) | Score: %d", score);
  } else {
    snprintf(title, sizeof(title),
             "snake-sdl | GAME OVER - Continue? (L) | Score: %d", score);
  }
  SDL_SetWindowTitle(window, title);
}

int main(void) {
  App app = {0};
  if (!App_Init(&app, WINDOW_W, WINDOW_H, GRID_W, GRID_H)) {
    return 1;
  }

  // ------------------------------------------------------------
  // Optional bot server (disabled by default)
  // Enable with:
  //   SNAKE_BOT=1 [SNAKE_BOT_PORT=5555] ./snake
  // ------------------------------------------------------------
  bool bot_enable = false;
  uint16_t bot_port = 5555;

  const char *bot_env = getenv("SNAKE_BOT");
  if (bot_env && bot_env[0] != '\0' && bot_env[0] != '0') {
    bot_enable = true;
  }

  const char *port_env = getenv("SNAKE_BOT_PORT");
  if (port_env && port_env[0] != '\0') {
    unsigned long p = strtoul(port_env, NULL, 10);
    if (p > 0 && p <= 65535) {
      bot_port = (uint16_t)p;
    }
  }

  Bot bot;
  if (!Bot_Init(&bot, bot_enable, bot_port)) {
    // Bot is optional; failure should not kill the game.
    Bot_Shutdown(&bot);
  }
  // ------------------------------------------------------------

  bool running = true;
  bool show_grid = true;

  // Render preference: whether we interpolate between ticks.
  // This is meant to be user-controlled and should survive resets.
  bool interp_setting = true;
  bool interp = interp_setting;

  Dir start_dir = (Dir)SDL_rand(4);

  Snake snake;
  if (!Snake_Init(&snake, app.grid_w, app.grid_h, app.grid_w * app.grid_h,
                  start_dir)) {
    Bot_Shutdown(&bot);
    App_Shutdown(&app);
    return 1;
  }

#if DEBUG_START_LONG
  debug_make_snake_long(&snake, DEBUG_START_LEN);
#endif

  int score = snake.len - 1;
  if (score < 0)
    score = 0;

  const int max_score = snake.max_len - 1;

  Apple apple;
  Apple_Init(&apple, &snake);

  const uint64_t frame_ns = ns_from_hz(RENDER_CAP_HZ);

  int tick_hz = tick_hz_for_score(score);
  uint64_t tick_ns = ns_from_hz(tick_hz);

  uint64_t last = SDL_GetTicksNS();
  uint64_t acc = 0;

  bool game_over = false;
  bool you_win = false;

  bool freeze_interp = true;
  float freeze_alpha = 1.0f;

  DeathFx death_fx;
  DeathFx_Init(&death_fx);

  FpsCounter fps;
  Fps_Init(&fps);

  SnakeDrawStyle style_green = {.snap_head = true,
                                .draw_bridges = true,
                                .head_r = 0,
                                .head_g = 255,
                                .head_b = 0,
                                .body_r = 0,
                                .body_g = 200,
                                .body_b = 0};

  SnakeDrawStyle style_blue = {.snap_head = true,
                               .draw_bridges = true,
                               .head_r = 40,
                               .head_g = 140,
                               .head_b = 255,
                               .body_r = 40,
                               .body_g = 120,
                               .body_b = 220};

  while (running) {
    uint64_t frame_start = SDL_GetTicksNS();

    uint64_t now = frame_start;
    uint64_t dt = now - last;
    last = now;

    if (dt > 250000000ull)
      dt = 250000000ull;
    acc += dt;

    EventsFrame ev = {0};
    Events_Poll(&ev);

    // Let a bot client connect at any time (non-blocking).
    Bot_PollAccept(&bot);

    if (ev.quit)
      break;

    // Continue after win or game over
    if ((game_over || you_win) && ev.continue_game) {
      Game_Reset(&snake, &apple, &score, &tick_hz, &tick_ns, &acc, &game_over,
                 &you_win, &interp, interp_setting, &death_fx, &app);
      continue;
    }

    // Head snapping threshold (render behavior only)
    const bool full_interp = (tick_hz >= FULL_INTERP_TPS || snake.len == 1);
    style_green.snap_head = !full_interp;
    style_blue.snap_head = !full_interp;

    if (!game_over && !you_win) {
      if (ev.toggle_grid)
        show_grid = !show_grid;
      if (ev.toggle_interp) {
        // Keep both the live render flag and the "remembered" preference in
        // sync. We don't want end states (win/death) or resets to implicitly
        // flip it.
        interp_setting = !interp_setting;
        interp = interp_setting;
      }

      for (int i = 0; i < ev.dir_count; i++) {
        Snake_QueueDir(&snake, ev.dirs[i]);
      }

      // Difficulty ramp: adjust tick rate based on current score.
      int desired_hz = tick_hz_for_score(score);
      if (desired_hz != tick_hz) {
        uint64_t old_tick_ns = tick_ns;

        tick_hz = desired_hz;
        tick_ns = ns_from_hz(tick_hz);

        // Rescale acc so the interpolation fraction stays consistent when
        // tick_ns changes.
        if (old_tick_ns > 0 && tick_ns > 0) {
          double frac = (double)acc / (double)old_tick_ns;
          acc = (uint64_t)(frac * (double)tick_ns);
        }
        if (acc > tick_ns * 4)
          acc = tick_ns * 4;
      }

      // Fixed-timestep update: consume whole ticks from the accumulator.
      while (acc >= tick_ns) {
        // ----- BOT BRIDGE (tick-aligned) -----
        // Send state (best effort, non-blocking)
        Bot_SendState(&bot, &snake, apple.pos, score, game_over, you_win);

        // Receive at most one bot direction per tick
        Dir bot_dir;
        if (Bot_TryRecvDir(&bot, &bot_dir)) {
          Snake_QueueDir(&snake, bot_dir);
        }
        // ------------------------------------

        Snake_Tick(&snake);

        if (Apple_TryEatAndRespawn(&apple, &snake)) {
          score += 1;

          if (score >= max_score) {
            you_win = true;

            // Freeze pose + mode on win so the final frame stays visually
            // stable.
            freeze_interp = interp;
            freeze_alpha = freeze_interp
                               ? (float)clamp01((double)acc / (double)tick_ns)
                               : 1.0f;

            acc = 0;
            break;
          }
        }

        if (snake_hit_self(&snake)) {
          game_over = true;

          float death_alpha =
              interp ? (float)clamp01((double)acc / (double)tick_ns) : 1.0f;

          // Start death disintegration using the current interpolation fraction
          // as the snapshot pose.
          DeathFx_Start(&death_fx, interp, death_alpha, SDL_GetTicksNS());

          acc = 0;
          break;
        }

        Fps_OnTick(&fps);
        acc -= tick_ns;
      }
    }

    // -------- RENDER --------
    Render_Clear(app.renderer);

    if (you_win) {
      SnakeDraw_Render(&app, &snake, freeze_alpha, style_blue);
    } else if (game_over) {
      if (show_grid) {
        Render_GridLinesEx(&app, 40, 40, 40, 120);
      }
      DeathFx_RenderAndAdvance(&death_fx, &app, &snake, SDL_GetTicksNS());
    } else {
      if (show_grid) {
        Render_GridLines(&app);
      }

      Render_CellFilled(&app, apple.pos, 220, 40, 40);

      float alpha =
          interp ? (float)clamp01((double)acc / (double)tick_ns) : 1.0f;

      SnakeDraw_Render(&app, &snake, alpha, style_green);
    }

    Render_Present(app.renderer);

    Fps_OnFrame(&fps);

    // Title: end states are authoritative and bypass FPS text
    if (game_over || you_win) {
      SetEndTitle(app.window, you_win, score);
    } else {
      Fps_UpdateWindowTitle(&fps, app.window, interp, score, game_over,
                            you_win);
    }

    if (frame_ns > 0) {
      uint64_t elapsed = SDL_GetTicksNS() - frame_start;
      if (elapsed < frame_ns) {
        SDL_DelayNS(frame_ns - elapsed);
      }
    }
  }

  Snake_Destroy(&snake);
  Bot_Shutdown(&bot);
  App_Shutdown(&app);
  return 0;
}
