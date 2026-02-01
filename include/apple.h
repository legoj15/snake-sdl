#pragma once

#include "snake.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * apple.h
 *
 * The apple is intentionally simple: it’s just a position on the grid.
 *
 * Design notes:
 * - Spawning avoids placing an apple inside the snake.
 * - When the board is nearly full, random sampling becomes inefficient, so the
 *   implementation falls back to a deterministic scan.
 *   That scan has two benefits:
 *     1) It guarantees progress.
 *     2) It makes “you win” states behave predictably when the snake fills the
 *        board.
 */

typedef struct Apple {
  IVec2 pos; // Grid position in cells.
} Apple;

// Picks an initial position not occupied by the snake.
void Apple_Init(Apple *a, const Snake *s);

// Checks whether the snake head is on the apple.
// If so, schedules growth (via Snake_AddGrowth) and respawns the apple.
// Returns true if the apple was eaten.
bool Apple_TryEatAndRespawn(Apple *a, Snake *s);

#ifdef __cplusplus
}
#endif
