#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

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

#define GRID_W 40
#define GRID_H 30
#define BASE_CELL_SIZE 20
#define MAX_WINDOW_DIM 1080

#define BASE_TICK_HZ 7
#define RAMP_EVERY 3
#define MAX_TICK_HZ 20

#define RENDER_CAP_HZ 240

// Above this TPS, disable "snappy head" and interpolate the whole snake
#define FULL_INTERP_TPS 12

// Above this TPS (bot mode), disable interpolation entirely.
#define BOT_INTERP_CUTOFF_TPS (RENDER_CAP_HZ)

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

static int cell_size_for_grid(int grid_w, int grid_h) {
  int max_dim = (grid_w > grid_h) ? grid_w : grid_h;
  int cell = BASE_CELL_SIZE;
  if (max_dim <= 0)
    return cell;
  if (max_dim * cell > MAX_WINDOW_DIM) {
    cell = MAX_WINDOW_DIM / max_dim;
    if (cell < 1)
      cell = 1;
  }
  return cell;
}

static void window_for_grid(int grid_w, int grid_h, int *out_w, int *out_h) {
  int cell = cell_size_for_grid(grid_w, grid_h);
  if (out_w)
    *out_w = grid_w * cell;
  if (out_h)
    *out_h = grid_h * cell;
}

static MIX_Audio *load_bgm(MIX_Mixer *mixer) {
  const char *bgm_files[] = {"assets/bgm.wav", "assets/bgm.opus",
                             "assets/bgm.mp3"};
  const int num_files = (int)(sizeof(bgm_files) / sizeof(bgm_files[0]));
  const char *base = SDL_GetBasePath();
  MIX_Audio *audio = NULL;

  for (int i = 0; i < num_files; i++) {
    char path[1024];
    if (base) {
      SDL_snprintf(path, (int)sizeof(path), "%s%s", base, bgm_files[i]);
      audio = MIX_LoadAudio(mixer, path, false);
      if (audio) {
        SDL_Log("BGM loaded: %s", path);
        break;
      }
    }
    SDL_snprintf(path, (int)sizeof(path), "%s", bgm_files[i]);
    audio = MIX_LoadAudio(mixer, path, false);
    if (audio) {
      SDL_Log("BGM loaded: %s", path);
      break;
    }
  }

  return audio;
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

static void Snake_ForceWinFill(Snake *s) {
  if (!s)
    return;
  while (s->grow > 0 && s->len < s->max_len) {
    IVec2 tail_prev = s->prev[s->len - 1];
    s->seg[s->len] = tail_prev;
    s->prev[s->len] = tail_prev;
    s->len += 1;
    s->grow -= 1;
  }
}

static void Snake_SyncPrevToSeg(Snake *s) {
  if (!s || !s->prev || !s->seg)
    return;
  for (int i = 0; i < s->len; i++) {
    s->prev[i] = s->seg[i];
  }
}

static const char *log_priority_name(SDL_LogPriority priority) {
  switch (priority) {
    case SDL_LOG_PRIORITY_VERBOSE:
      return "VERBOSE";
    case SDL_LOG_PRIORITY_DEBUG:
      return "DEBUG";
    case SDL_LOG_PRIORITY_INFO:
      return "INFO";
    case SDL_LOG_PRIORITY_WARN:
      return "WARN";
    case SDL_LOG_PRIORITY_ERROR:
      return "ERROR";
    case SDL_LOG_PRIORITY_CRITICAL:
      return "CRITICAL";
    default:
      return "LOG";
  }
}

static void log_to_file(void *userdata, int category, SDL_LogPriority priority,
                        const char *message) {
  FILE *fp = (FILE *)userdata;
  if (!fp)
    return;
  time_t now = time(NULL);
  struct tm tm_now;
  bool has_time = true;
#ifdef _WIN32
  if (localtime_s(&tm_now, &now) != 0)
    has_time = false;
#else
  if (!localtime_r(&now, &tm_now))
    has_time = false;
#endif
  char ts[32];
  if (has_time &&
      strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now) > 0) {
    fprintf(fp, "[%s] [%s] [%d] %s\n", ts, log_priority_name(priority),
            category, message);
  } else {
    fprintf(fp, "[unknown-time] [%s] [%d] %s\n", log_priority_name(priority),
            category, message);
  }
  fflush(fp);
}

static FILE *g_log_file = NULL;

static void Log_EnsureDir(const char *path) {
  if (!path || !*path)
    return;
#ifdef _WIN32
  _mkdir(path);
#else
  mkdir(path, 0755);
#endif
}

static void Log_OpenFile(void) {
  if (g_log_file)
    return;
  char dir_path[1024];
  char path[1024];
#ifdef _WIN32
  char exe_path[1024];
  DWORD exe_len = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof(exe_path));
  if (exe_len > 0 && exe_len < sizeof(exe_path)) {
    char *last_sep = strrchr(exe_path, '\\');
    if (last_sep) {
      *last_sep = '\0';
      if (snprintf(dir_path, sizeof(dir_path), "%s\\logs", exe_path) < 0) {
        snprintf(dir_path, sizeof(dir_path), "logs");
      }
    } else {
      snprintf(dir_path, sizeof(dir_path), "logs");
    }
  } else {
    snprintf(dir_path, sizeof(dir_path), "logs");
  }
  if (snprintf(path, sizeof(path), "%s\\snake.log", dir_path) < 0) {
    snprintf(path, sizeof(path), "logs\\snake.log");
  }
#else
  snprintf(dir_path, sizeof(dir_path), "logs");
  snprintf(path, sizeof(path), "%s/snake.log", dir_path);
#endif
  Log_EnsureDir(dir_path);
  #ifdef _WIN32
  if (fopen_s(&g_log_file, path, "a") != 0)
    g_log_file = NULL;
  #else
  g_log_file = fopen(path, "a");
  #endif
  if (g_log_file) {
    setvbuf(g_log_file, NULL, _IOLBF, 0);
    fprintf(g_log_file, "Logging to: %s\n", path);
    fflush(g_log_file);
  }
}

static void Log_AttachToSDL(void) {
  if (!g_log_file)
    return;
  SDL_SetLogOutputFunction(log_to_file, g_log_file);
}

static void Log_CloseFile(void) {
  if (!g_log_file)
    return;
  fclose(g_log_file);
  g_log_file = NULL;
}

static void Game_Reset(Snake *snake, Apple *apple, int *score, int *tick_hz,
                       uint64_t *tick_ns, uint64_t *acc, bool *game_over,
                       bool *you_win, bool *interp, bool interp_setting,
                       DeathFx *death_fx, const App *app, bool bot_enabled,
                       int bot_tps) {
  Snake_Destroy(snake);

  Dir start_dir = (Dir)SDL_rand(4);
  Snake_Init(snake, app->grid_w, app->grid_h, app->grid_w * app->grid_h,
             start_dir);

  *score = snake->len - 1;
  if (*score < 0)
    *score = 0;

  Apple_Init(apple, snake);

  *tick_hz =
      (bot_enabled && bot_tps > 0) ? bot_tps : tick_hz_for_score(*score);
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

static bool arg_eq(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

static bool parse_preset_name(const char *name, Preset *out) {
  if (!name || !out)
    return false;
  if (strcmp(name, "safe") == 0) {
    *out = PRESET_SAFE;
    return true;
  }
  if (strcmp(name, "aggressive") == 0) {
    *out = PRESET_AGGRESSIVE;
    return true;
  }
  if (strcmp(name, "greedy") == 0 || strcmp(name, "greedy-apple") == 0 ||
      strcmp(name, "greedy_apple") == 0) {
    *out = PRESET_GREEDY_APPLE;
    return true;
  }
  if (strcmp(name, "chaotic") == 0) {
    *out = PRESET_CHAOTIC;
    return true;
  }
  return false;
}

typedef struct CycleMeta {
  int grid_w;
  int grid_h;
  int window_w;
  int window_h;
  unsigned int seed;
} CycleMeta;

static bool parse_cycle_meta(const char *path, CycleMeta *out, char *err,
                             int err_len) {
  if (!path || !out) {
    snprintf(err, (size_t)err_len, "invalid cycle path");
    return false;
  }

  FILE *f = fopen(path, "rb");
  if (!f) {
    snprintf(err, (size_t)err_len, "failed to open cycle file");
    return false;
  }

  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0) {
    fclose(f);
    snprintf(err, (size_t)err_len, "cycle file is empty");
    return false;
  }

  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    snprintf(err, (size_t)err_len, "out of memory reading cycle file");
    return false;
  }
  size_t got = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[got] = '\0';

  int grid_w = -1;
  int grid_h = -1;
  int window_w = -1;
  int window_h = -1;
  unsigned int seed = 0;

  char *p = buf;
  while (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')
    p++;
  char *line_end = strpbrk(p, "\r\n");
  if (!line_end) {
    free(buf);
    snprintf(err, (size_t)err_len, "cycle file missing header");
    return false;
  }
  *line_end = '\0';
  if (strcmp(p, "SNAKECYCLE 1") != 0) {
    free(buf);
    snprintf(err, (size_t)err_len, "cycle file header invalid");
    return false;
  }

  p = line_end + 1;
  while (*p) {
    while (*p == '\r' || *p == '\n')
      p++;
    if (!*p)
      break;

    char *e = strpbrk(p, "\r\n");
    if (e)
      *e = '\0';

    char *s = p;
    while (*s == ' ' || *s == '\t')
      s++;

    if (*s == '\0' || *s == '#') {
      // skip
    } else if (strcmp(s, "DATA") == 0) {
      break;
    } else {
      char *eq = strchr(s, '=');
      if (eq) {
        *eq = '\0';
        char *key = s;
        char *val = eq + 1;
        while (*val == ' ' || *val == '\t')
          val++;
        int iv = atoi(val);
        if (strcmp(key, "width") == 0)
          grid_w = iv;
        else if (strcmp(key, "height") == 0)
          grid_h = iv;
        else if (strcmp(key, "window_w") == 0)
          window_w = iv;
        else if (strcmp(key, "window_h") == 0)
          window_h = iv;
        else if (strcmp(key, "seed") == 0)
          seed = (unsigned int)iv;
      }
    }

    if (!e)
      break;
    p = e + 1;
  }

  free(buf);

  if (grid_w <= 0 || grid_h <= 0) {
    snprintf(err, (size_t)err_len, "cycle metadata missing dimensions");
    return false;
  }
  if (grid_w < 2 || grid_h < 2) {
    snprintf(err, (size_t)err_len, "grid width/height must be >= 2");
    return false;
  }
  if (window_w > 0 && window_h > 0) {
    if ((window_w % grid_w) != 0 || (window_h % grid_h) != 0) {
      snprintf(err, (size_t)err_len,
               "window size must be divisible by grid size");
      return false;
    }
  }
  if (seed == 0) {
    snprintf(err, (size_t)err_len, "cycle seed is required");
    return false;
  }

  out->grid_w = grid_w;
  out->grid_h = grid_h;
  out->window_w = window_w;
  out->window_h = window_h;
  out->seed = seed;
  return true;
}

int main(int argc, char **argv) {
  // ------------------------------
  // Bot mode (off by default)
  // ------------------------------
  bool bot_enabled = false;
  bool bot_gui = false;
  int bot_tps = 0;
  const char *bot_cycle_path = NULL;
  BotTuning bot_tuning = {0};
  apply_preset(PRESET_SAFE, &bot_tuning);
  // Human-mode overrides; keep defaults unless explicitly set.
  int cli_grid_w = GRID_W;
  int cli_grid_h = GRID_H;
  unsigned int cli_seed = 0;
  bool cli_seed_set = false;
  bool bgm_enabled = true;

  // CLI:
  //   --bot                 enable bot mode
  //   --bot-cycle <file>    optional .cycle container file (refused if not
  //   .cycle)
  //   --no-bgm              disable background music
  for (int i = 1; i < argc; i++) {
    if (arg_eq(argv[i], "--bot")) {
      bot_enabled = true;
    } else if (arg_eq(argv[i], "--bot-gui")) {
      bot_gui = true;
    } else if (arg_eq(argv[i], "--bot-cycle") && i + 1 < argc) {
      bot_cycle_path = argv[i + 1];
      i++;
    } else if (arg_eq(argv[i], "--bot-tps") && i + 1 < argc) {
      bot_tps = atoi(argv[i + 1]);
      if (bot_tps > 7000) {
        SDL_Log("Bot TPS cannot exceed 7000 (requested %d).", bot_tps);
        bot_tps = -1;
      }
      if (bot_tps < 7) {
        SDL_Log("Invalid bot TPS (%d); ignoring.", bot_tps);
        bot_tps = -1;
      }
      i++;
    } else if (arg_eq(argv[i], "--bot-preset") && i + 1 < argc) {
      Preset p;
      if (!parse_preset_name(argv[i + 1], &p)) {
        SDL_Log("Unknown preset: %s", argv[i + 1]);
        return 1;
      }
      apply_preset(p, &bot_tuning);
      i++;
    } else if (arg_eq(argv[i], "--bot-k-progress") && i + 1 < argc) {
      bot_tuning.k_progress = strtod(argv[i + 1], NULL);
      i++;
    } else if (arg_eq(argv[i], "--bot-k-away") && i + 1 < argc) {
      bot_tuning.k_away = strtod(argv[i + 1], NULL);
      i++;
    } else if (arg_eq(argv[i], "--bot-k-skip") && i + 1 < argc) {
      bot_tuning.k_skip = strtod(argv[i + 1], NULL);
      i++;
    } else if (arg_eq(argv[i], "--bot-k-slack") && i + 1 < argc) {
      bot_tuning.k_slack = strtod(argv[i + 1], NULL);
      i++;
    } else if (arg_eq(argv[i], "--bot-k-loop") && i + 1 < argc) {
      bot_tuning.k_loop = strtod(argv[i + 1], NULL);
      i++;
    } else if (arg_eq(argv[i], "--bot-loop-window") && i + 1 < argc) {
      bot_tuning.loop_window = atoi(argv[i + 1]);
      i++;
    } else if (arg_eq(argv[i], "--bot-aggression-scale") && i + 1 < argc) {
      bot_tuning.aggression_scale = strtod(argv[i + 1], NULL);
      i++;
    } else if (arg_eq(argv[i], "--bot-max-skip-cap") && i + 1 < argc) {
      bot_tuning.max_skip_cap = atoi(argv[i + 1]);
      i++;
    } else if (arg_eq(argv[i], "--grid-w") && i + 1 < argc) {
      cli_grid_w = atoi(argv[i + 1]);
      i++;
    } else if (arg_eq(argv[i], "--grid-h") && i + 1 < argc) {
      cli_grid_h = atoi(argv[i + 1]);
      i++;
    } else if (arg_eq(argv[i], "--seed") && i + 1 < argc) {
      cli_seed = (unsigned int)atoi(argv[i + 1]);
      cli_seed_set = true;
      i++;
    } else if (arg_eq(argv[i], "--no-bgm")) {
      bgm_enabled = false;
    }
  }

  CycleMeta meta = {0};
  if (bot_enabled) {
    if (!bot_gui) {
      SDL_Log("Bot mode requires GUI launch (--bot-gui).");
      return 1;
    }
    if (!bot_cycle_path) {
      SDL_Log("Bot mode requires --bot-cycle.");
      return 1;
    }
    if (bot_tps <= 0) {
      SDL_Log("Bot mode requires a valid --bot-tps (7..7000).");
      return 1;
    }
    char meta_err[256];
    if (!parse_cycle_meta(bot_cycle_path, &meta, meta_err,
                          (int)sizeof(meta_err))) {
      SDL_Log("Bot cycle metadata invalid: %s", meta_err);
      return 1;
    }
  }

  int init_grid_w = GRID_W;
  int init_grid_h = GRID_H;
  if (bot_enabled) {
    init_grid_w = meta.grid_w;
    init_grid_h = meta.grid_h;
  } else {
    init_grid_w = cli_grid_w;
    init_grid_h = cli_grid_h;
  }
  if (init_grid_w < 2 || init_grid_h < 2) {
    SDL_Log("Grid width/height must be >= 2.");
    return 1;
  }
  int init_window_w = 0;
  int init_window_h = 0;
  window_for_grid(init_grid_w, init_grid_h, &init_window_w, &init_window_h);

  Log_OpenFile();

  App app = {0};
  if (!App_Init(&app, init_window_w, init_window_h, init_grid_w, init_grid_h)) {
    return 1;
  }
  Log_AttachToSDL();

  MIX_Mixer *mixer = NULL;
  MIX_Audio *bgm_audio = NULL;
  MIX_Track *bgm_track = NULL;
  bool mix_inited = false;

  if (bgm_enabled && MIX_Init()) {
      mix_inited = true;
      // Create a mixer connected to the default playback device
      mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
      
      if (mixer) {
          bgm_audio = load_bgm(mixer);
          if (bgm_audio) {
              // To play audio, we need a Track
              bgm_track = MIX_CreateTrack(mixer);
              if (bgm_track) {
                  MIX_SetTrackAudio(bgm_track, bgm_audio);

                  // Create properties to tell it to loop infinitely
                  SDL_PropertiesID props = SDL_CreateProperties();
                  SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
                  
                  MIX_PlayTrack(bgm_track, props);
                  
                  SDL_DestroyProperties(props);
              }
          } else {
              SDL_Log("No BGM found (checked assets/bgm.{wav,opus,mp3}).");
          }
      } else {
          SDL_Log("MIX_CreateMixerDevice failed: %s", SDL_GetError());
      }
  } else if (bgm_enabled) {
      SDL_Log("MIX_Init failed: %s", SDL_GetError());
  }

  if (bot_enabled) {
    SDL_srand((Uint64)meta.seed);
  } else if (cli_seed_set) {
    // Optional deterministic seed for human-mode launches.
    if (cli_seed == 0) {
      SDL_Log("Seed must be a positive integer.");
      App_Shutdown(&app);
      return 1;
    }
    SDL_srand((Uint64)cli_seed);
  }

  bool running = true;
  bool show_grid = true;

  Bot bot = {0};
  bool bot_ready = false;

  // Render preference: whether we interpolate between ticks.
  // This is meant to be user-controlled and should survive resets.
  bool interp_setting = true;
  bool interp = interp_setting;

  Dir start_dir = (Dir)SDL_rand(4);

  Snake snake;
  if (!Snake_Init(&snake, app.grid_w, app.grid_h, app.grid_w * app.grid_h,
                  start_dir)) {
    App_Shutdown(&app);
    return 1;
  }

  // Bot is embedded in-game, but only initialized/used when enabled.
  if (bot_enabled) {
    bot_ready = Bot_Init(&bot, app.grid_w, app.grid_h);
    if (!bot_ready) {
      SDL_Log("Bot_Init failed.");
      App_Shutdown(&app);
      return 1;
    }
    if (bot_cycle_path) {
      if (!Bot_LoadCycleFromFile(&bot, bot_cycle_path)) {
        SDL_Log("Bot cycle load failed (%s).", bot_cycle_path);
        Bot_Destroy(&bot);
        App_Shutdown(&app);
        return 1;
      }
    }
    Bot_SetTuning(&bot, &bot_tuning);
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

  int tick_hz =
      (bot_enabled && bot_tps > 0) ? bot_tps : tick_hz_for_score(score);
  uint64_t tick_ns = ns_from_hz(tick_hz);
  if (bot_enabled && bot_tps >= BOT_INTERP_CUTOFF_TPS) {
    interp_setting = false;
    interp = false;
  }

  uint64_t last = SDL_GetTicksNS();
  uint64_t acc = 0;

  bool game_over = false;
  bool you_win = false;

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

    if (ev.quit)
      break;

    // Continue after win or game over
    if ((game_over || you_win) && ev.continue_game) {
      Game_Reset(&snake, &apple, &score, &tick_hz, &tick_ns, &acc, &game_over,
                 &you_win, &interp, interp_setting, &death_fx, &app,
                 bot_enabled, bot_tps);
      if (bot_enabled && bot_ready) {
        bot.cycle_pos = -1;
      }
      continue;
    }

    // Head snapping threshold (render behavior only)
    const bool full_interp = (tick_hz >= FULL_INTERP_TPS || snake.len == 1);
    style_green.snap_head = !full_interp;
    style_blue.snap_head = !full_interp;

    if (ev.toggle_grid)
      show_grid = !show_grid;

    if (!game_over && !you_win) {
      if (ev.toggle_interp) {
        // Keep both the live render flag and the "remembered" preference in
        // sync. We don't want end states (win/death) or resets to implicitly
        // flip it.
        if (!(bot_enabled && bot_tps >= BOT_INTERP_CUTOFF_TPS)) {
          interp_setting = !interp_setting;
          interp = interp_setting;
        }
      }

      if (!bot_enabled) {
        for (int i = 0; i < ev.dir_count; i++) {
          Snake_QueueDir(&snake, ev.dirs[i]);
        }
      }

      // Difficulty ramp: adjust tick rate based on current score.
      if (!bot_enabled) {
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
      }

      // Fixed-timestep update: consume whole ticks from the accumulator.
      while (acc >= tick_ns) {
        if (bot_enabled && bot_ready) {
          Bot_OnTick(&bot, &snake, &apple);
        }
        Snake_Tick(&snake);

        if (Apple_TryEatAndRespawn(&apple, &snake)) {
          score += 1;

          if (score >= max_score) {
            Snake_ForceWinFill(&snake);
            Snake_SyncPrevToSeg(&snake);
            you_win = true;

            // Freeze pose + mode on win so the final frame stays visually
            // stable.
            freeze_alpha = 1.0f;

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
      if (show_grid) {
        Render_GridLinesEx(&app, 40, 40, 40, 120);
      }
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

  if (bot_ready) {
    Bot_Destroy(&bot);
  }

  if (bgm_track) {
      MIX_DestroyTrack(bgm_track);
  }
  if (bgm_audio) {
      MIX_DestroyAudio(bgm_audio);
  }
  if (mixer) {
      MIX_DestroyMixer(mixer);
  }
  if (mix_inited) {
      MIX_Quit();
  }

  Snake_Destroy(&snake);
  App_Shutdown(&app);
  Log_CloseFile();
  return 0;
}
