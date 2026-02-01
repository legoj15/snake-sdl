#pragma once

#include "snake.h"
#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * events.h
 *
 * SDL event polling and input mapping.
 */

typedef struct EventsFrame {
  bool quit;
  bool toggle_grid;
  bool toggle_interp;
  bool continue_game;
  bool next_track;
  bool toggle_ui;

  // Direction changes queued this frame.
  Dir dirs[4];
  int dir_count;
} EventsFrame;

// Polls the input state for the current frame.
void Events_Poll(EventsFrame *ev);

// Process a single SDL event and update the frame struct.
// Shared logic between Poll and unified event loops.
void Events_ProcessEvent(EventsFrame *out, const SDL_Event *e);

#ifdef __cplusplus
}
#endif
