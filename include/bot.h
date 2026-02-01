#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "apple.h"
#include "snake.h"
#include <stdint.h>

// Bot tuning parameters.
typedef struct BotTuning {
  double k_progress;       // Weight for moving toward apple.
  double k_away;           // Weight for moving away from snake body.
  double k_skip;           // Weight for skipping steps in cycle (pathfinding).
  double k_slack;          // Slack for pathfinding.
  double k_loop;           // Weight for cycle length.
  int loop_window;         // Search window for cycles.
  double aggression_scale; // How aggressively to move toward apple.
  int max_skip_cap;        // Max segments to skip in cycle.
} BotTuning;

typedef enum Preset {
  PRESET_SAFE,
  PRESET_FAST,
  PRESET_AGGRESSIVE,
  PRESET_GREEDY_APPLE,
  PRESET_CHAOTIC
} Preset;

typedef struct Bot {
  int grid_w, grid_h;
  int n_cells;

  Dir *cycle_next_dir;
  int *cycle_index;
  IVec2 *pos_of_idx;
  int *next_cycle_idx;
  Dir *cycle_dirs;
  uint8_t *occupied_idx;
  int *last_visit_idx;

  int cycle_pos;

  bool debug_shortcuts;
  uint32_t tick;
  bool cycle_wrap;

  BotTuning tuning;
} Bot;

// Allocates cycle array.
bool Bot_Init(Bot *b, int grid_w, int grid_h);

// Frees bot resources.
void Bot_Destroy(Bot *b);

// Loads a pre-baked cycle from a .cycle file.
bool Bot_LoadCycleFromFile(Bot *b, const char *path);

// Updates the bot instance for the current tick.
void Bot_OnTick(Bot *b, Snake *s, const Apple *a);

// Helper to parse preset name from string.
bool parse_preset_name(const char *name, Preset *p);

// Applies a tuning preset.
void apply_preset(Preset p, BotTuning *tuning);

// Applies current bot tuning parameters to the bot instance.
void Bot_SetTuning(Bot *b, const BotTuning *tuning);

#ifdef __cplusplus
}
#endif
