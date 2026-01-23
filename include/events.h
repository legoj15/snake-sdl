#pragma once

/*
 * events.h
 *
 * Input in this project is “frame-based”: each rendered frame we poll SDL,
 * translate raw events into a small, game-friendly struct, and then the game
 * loop decides what to do with it.
 *
 * Why not push events directly into the snake/game state?
 * - It keeps SDL-specific types out of gameplay modules.
 * - It makes the main loop easy to reason about (poll → interpret → apply).
 * - The direction array supports multiple key presses in one frame, which plays nicely with the snake’s internal 2-turn buffer.
 */

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "snake.h"   // for Dir

typedef struct EventsFrame {
    bool quit;
    bool toggle_grid;
    bool toggle_interp;   // bound to P
    bool continue_game;   // bound to L
    bool next_track;     // NEW: Request to skip current track

    // One frame can produce multiple direction inputs.
    // The snake module decides how many of these to accept.
    int dir_count;
    Dir dirs[8];
} EventsFrame;

// Polls SDL events and fills out an EventsFrame for the current frame.
// The struct is “one-shot”: callers should reinitialize it every frame.
void Events_Poll(EventsFrame* out);
