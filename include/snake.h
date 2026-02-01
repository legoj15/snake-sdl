#pragma once

/*
 * snake.h
 *
 * Core game simulation (no rendering, no SDL).
 *
 * Responsibilities:
 * - Maintain the snake’s segment positions on a wrapping grid.
 * - Apply a “nice-feel” input model: a small direction buffer so quick turns
 *   register even if they happen between ticks.
 * - Track previous positions for interpolation. Rendering can lerp between
 *   prev and seg while simulation stays in clean, discrete steps.
 *
 * Important invariants:
 * - seg[0] is always the head.
 * - len is the number of active segments (<= max_len).
 * - prev mirrors seg for the active segment range; it is updated at the start
 *   of each tick.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int x, y;
} IVec2;

typedef enum Dir { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Dir;

typedef struct Snake {
  int grid_w, grid_h;

  int len;     // number of active segments
  int max_len; // capacity
  int grow;    // pending growth (segments to add)

  Dir dir;

  // Small input buffer ("2-turn buffer").
  // We queue up to two direction changes and apply at most one per tick.
  // This feels better than “one direction per frame” when tick rates are low.
  bool has_q1, has_q2;
  Dir q1, q2;

  // Current + previous segment positions (for interpolation)
  IVec2 *seg;  // [len]
  IVec2 *prev; // [len]
} Snake;

// Allocates segment arrays and places the snake at the center of the grid.
bool Snake_Init(Snake *s, int grid_w, int grid_h, int max_len, Dir start_dir);

// Frees allocations from Snake_Init.
void Snake_Destroy(Snake *s);

// Queue a direction change. Invalid turns (reverse direction, repeated
// direction) are ignored. The simulation applies at most one queued turn per
// tick.
void Snake_QueueDir(Snake *s, Dir d);

// Advances the simulation by one tick:
// - copies seg -> prev
// - applies one buffered direction change
// - shifts body and moves head with wraparound
// - applies one unit of growth if requested
void Snake_Tick(Snake *s);

// Request growth. Growth is applied on subsequent ticks so movement stays
// consistent.
void Snake_AddGrowth(Snake *s, int n);

// Returns true if any snake segment occupies p.
bool Snake_Occupies(const Snake *s, IVec2 p);

// Convenience: return current head position (seg[0]).
IVec2 Snake_Head(const Snake *s);

#ifdef __cplusplus
}
#endif
