#pragma once

/*
 * bot.h
 *
 * Optional "bot mode" bridge for external controllers (e.g. a Python solver).
 *
 * Design goals:
 * - Keep SDL input (events.c) separate from bot input.
 * - Simple, robust TCP protocol over localhost.
 * - Non-blocking I/O: the game loop must never stall waiting for a bot.
 *
 * Enablement (runtime):
 * - Set env var SNAKE_BOT=1 to enable.
 * - Optional env var SNAKE_BOT_PORT=<port> (default 5555).
 *
 * Protocol (TCP):
 * - Game listens on 127.0.0.1:<port> and accepts a single client.
 * - Each simulation tick, the game sends:
 *     [BotStateMsg header] then [snake segments payload]
 * - The bot may send 1 byte at any time representing a direction:
 *     0=UP, 1=DOWN, 2=LEFT, 3=RIGHT (matches Dir enum in snake.h)
 *
 * Encoding:
 * - All integer fields are sent in network byte order (big-endian).
 *
 * Versioning:
 * - version=1: header only (no segment payload)
 * - version=2: header + snake segments payload:
 *       snake_len * (int32 x, int32 y) for seg[0..snake_len-1]
 */

#include <stdbool.h>
#include <stdint.h>

#include "snake.h" // Snake, Dir, IVec2

typedef struct BotStateMsg {
  // Magic 'SNKB' (0x534E4B42) to sanity-check stream.
  uint32_t magic;
  uint16_t version; // currently 2
  uint16_t flags;   // bit0=game_over, bit1=you_win

  int32_t grid_w;
  int32_t grid_h;

  int32_t head_x;
  int32_t head_y;

  int32_t apple_x;
  int32_t apple_y;

  int32_t snake_len;
  int32_t score;
} BotStateMsg;

typedef struct Bot {
  bool enabled;
  bool connected;
  uint16_t port;

  // internal socket handles live in bot.c
  uintptr_t _listen_sock;
  uintptr_t _client_sock;
} Bot;

// Initializes the bot server (non-blocking). Returns true if initialized.
// If enable is false, this is a no-op that returns true.
bool Bot_Init(Bot *b, bool enable, uint16_t port);

// Shuts down sockets and resets state.
void Bot_Shutdown(Bot *b);

// Polls for a new client connection (non-blocking). Safe to call every frame.
void Bot_PollAccept(Bot *b);

// Attempts to receive one direction byte from the client (non-blocking).
// Returns true and sets *out_dir if a valid direction was received.
bool Bot_TryRecvDir(Bot *b, Dir *out_dir);

// Sends one state message to the connected client (non-blocking best effort).
// If no client is connected, this is a cheap no-op.
//
// Version 2 sends header + snake segments payload.
void Bot_SendState(Bot *b, const Snake *s, IVec2 apple, int score,
                   bool game_over, bool you_win);
