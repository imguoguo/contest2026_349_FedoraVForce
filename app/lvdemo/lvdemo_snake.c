/****************************************************************************
 * app/lvdemo/lvdemo_snake.c
 *
 * The snake game of packages/demos/snake_game, built here instead of as
 * its own app, whose main loop never returns.  Its source is not changed:
 *
 * - snake_game.h is included first with a grid for the portrait screen.
 *   The game is made for a 1200px wide landscape screen (60 x 35 cells of
 *   20px); 52 x 76 cells fill 1080 x 1920 instead.  The header guard keeps
 *   snake_game.c from including it again.
 * - Its state is static and outlives LVGL in the flat build, so it is
 *   cleared before each start and the snake is freed on exit.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>

#include "snake_game.h"

#undef GAME_GRID_WIDTH
#undef GAME_GRID_HEIGHT
#define GAME_GRID_WIDTH       52
#define GAME_GRID_HEIGHT      76

#include "snake_game.c"

#include "lvdemo.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void lvdemo_snake_create(void)
{
  memset(&g_game, 0, sizeof(g_game));
  last_score = 0;
  no_score_frames = 0;
  snake_game_create();
}

void lvdemo_snake_destroy(void)
{
  snake_game_deinit();
  g_game.snake_head = NULL;
}
