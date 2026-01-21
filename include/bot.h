#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "apple.h"
#include "snake.h"

/*
 * bot.h
 *
 * A "perfect" autoplayer intended to be entertaining to watch.
 *
 * Strategy (high level):
 *  1) Follow a Hamiltonian cycle over the whole board.
 *  2) The cycle is precomputed and stored as a sequence of directions.
 *
 * Bot mode is meant to be embedded in-game and launched via the GUI.
 */

typedef struct BotTuning {
  // Scoring weights for shortcut selection (safety checks remain enforced).
  double k_progress;
  double k_away;
  double k_skip;
  double k_slack;
  double k_loop;
  double aggression_scale;
  int loop_window;
  int max_skip_cap;
} BotTuning;

typedef struct Bot {
  int grid_w, grid_h;
  int n_cells;

  // For each cell (y*grid_w+x), what direction advances to the next cell
  // on the cycle?
  Dir *cycle_next_dir;

  // Index of each cell along the cycle (0..n_cells-1), starting from the
  // top-right corner.
  int *cycle_index;

  // Reverse lookup: cycle index -> position.
  IVec2 *pos_of_idx;

  // Next cycle index for each cycle index (ordering only).
  int *next_cycle_idx;

  // Full cycle as a sequence of directions, starting at top-right.
  Dir *cycle_dirs;
  int cycle_pos;

  bool cycle_wrap;

  // Occupancy by cycle index for fast local safety checks.
  uint8_t *occupied_idx;

  // Loop avoidance: last tick a cycle index was visited by the head.
  int *last_visit_idx;
  uint32_t tick;

  // Debug: log when a shortcut is taken.
  bool debug_shortcuts;

  BotTuning tuning;
} Bot;

typedef enum Preset {
  PRESET_SAFE = 0,
  PRESET_AGGRESSIVE = 1,
  PRESET_GREEDY_APPLE = 2,
  PRESET_CHAOTIC = 3
} Preset;

// Initialize with a default built-in Hamiltonian cycle.
bool Bot_Init(Bot *b, int grid_w, int grid_h);

// Optional: load a custom cycle from a ".cycle" container file.
// The loader refuses any non-.cycle path.
//
// Format:
//   SNAKECYCLE 1
//   key=value (optional, e.g. width=40)
//   DATA
//   U/D/L/R direction letters (whitespace ignored), row-major
bool Bot_LoadCycleFromFile(Bot *b, const char *path);

void Bot_Destroy(Bot *b);

// Called once per simulation tick (right before Snake_Tick). This function
// queues at most one direction change into the snake.
void Bot_OnTick(Bot *b, Snake *s, const Apple *a);

// Preset helpers for tuning.
void apply_preset(Preset p, BotTuning *t);
bool preset_matches_current(Preset p, const BotTuning *t, double epsilon);

// Apply tuning values (clamped for safety).
void Bot_SetTuning(Bot *b, const BotTuning *t);
