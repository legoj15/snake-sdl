#include "events.h"
#include <stdbool.h>


/*
 * events.c
 * SDL input polling and translation into a compact per-frame struct.
 */


// Helper: record direction presses (bounded) so the main loop can feed them into the snake buffer.
static void push_dir(EventsFrame* out, Dir d) {
    if (out->dir_count < (int)(sizeof(out->dirs) / sizeof(out->dirs[0]))) {
        out->dirs[out->dir_count++] = d;
    }
}

void Events_Poll(EventsFrame* out) {
    if (!out) return;

    out->quit = false;
    out->toggle_grid = false;
    out->toggle_interp = false;
    out->next_track = false;
    out->dir_count = 0;
    out->continue_game = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                out->quit = true;
                break;

            case SDL_EVENT_KEY_DOWN: {
                if (e.key.repeat) break;

                if (e.key.scancode == SDL_SCANCODE_G) {
                    out->toggle_grid = true;
                    break;
                }

                if (e.key.scancode == SDL_SCANCODE_P) {
                    out->toggle_interp = true;
                    break;
                }

                // NEW: Media Next Track (or 'N' for debug)
                if (e.key.scancode == SDL_SCANCODE_MEDIA_NEXT_TRACK ||
                    e.key.scancode == SDL_SCANCODE_N) {
                    out->next_track = true;
                    break;
                }

                if (e.key.scancode == SDL_SCANCODE_UP) {
                    push_dir(out, DIR_UP);
                } else if (e.key.scancode == SDL_SCANCODE_DOWN) {
                    push_dir(out, DIR_DOWN);
                } else if (e.key.scancode == SDL_SCANCODE_LEFT) {
                    push_dir(out, DIR_LEFT);
                } else if (e.key.scancode == SDL_SCANCODE_RIGHT) {
                    push_dir(out, DIR_RIGHT);
                } else if (e.key.scancode == SDL_SCANCODE_ESCAPE) {
                    out->quit = true;
                    break;
                } else if (e.key.scancode == SDL_SCANCODE_L) {
                    out->continue_game = true;
                }

            } break;

            default:
                break;
        }
    }
}
