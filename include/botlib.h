#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * botlib.h
 *
 * Native bot library interface (for bot mode).
 */

#include "apple.h"
#include "bot.h"
#include "snake.h"


// Lifecycle
bool BotLib_Init(int grid_w, int grid_h);
void BotLib_Shutdown(void);

// Logic
void BotLib_Update(Snake *snake, Apple *apple);

#ifdef __cplusplus
}
#endif
