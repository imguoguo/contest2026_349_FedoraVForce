/****************************************************************************
 * tools/preview/scene_lvdemo.c
 *
 * Each lvdemo demo a few seconds after it starts, with the back button.
 ****************************************************************************/

#define main lvdemo_main
#include "../../app/lvdemo/lvdemo_main.c"
#undef main

static void tap(int32_t x, int32_t y)
{
  preview_touch(x, y, true);
  preview_step(60);
  preview_touch(x, y, false);
}

static void run(const char *name, const char *shot, uint32_t ms,
                int32_t tap_x, int32_t tap_y)
{
  const struct lvdemo_s *demo = lvdemo_find(name);
  lv_nuttx_result_t result;
  char file[48];

  lvdemo_start(&result, demo);
  preview_step(500);
  if (tap_x >= 0)
    {
      tap(tap_x, tap_y);
    }

  preview_step(ms);
  snprintf(file, sizeof(file), "9-lvdemo-%s.png", shot);
  preview_shot(file);

  if (demo->destroy != NULL)
    {
      demo->destroy();
    }

  lv_nuttx_deinit(&result);
  lv_deinit();
}

int main(void)
{
  run("benchmark", "benchmark", 3000, -1, -1);
  run("widgets", "widgets", 1000, -1, -1);

  /* START of both games is in the middle and at the bottom.  Whackmole
   * is left while it runs and started again, its timers must not be
   * deleted twice.
   */

  run("snake", "snake", 3000, 540, 960);
  run("whackmole", "whackmole", 2500, 540, 1300);
  run("whackmole", "whackmole-again", 100, -1, -1);
  return 0;
}
