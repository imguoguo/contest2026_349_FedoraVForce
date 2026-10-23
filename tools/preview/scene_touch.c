/****************************************************************************
 * tools/preview/scene_touch.c
 *
 * Touch: a stroke being drawn, the trail fading, three fingers at once,
 * the calibration check and its result.
 ****************************************************************************/

#include <math.h>

#define main touch_main
#include "../../app/touch/touch_main.c"
#undef main

static void tap(int32_t x, int32_t y)
{
  preview_touch(x, y, true);
  preview_step(60);
  preview_touch(x, y, false);
  preview_step(60);
}

/* A loop through the trail area, one point per 10ms like the drag */

static void stroke(int32_t cx, int32_t cy, int32_t r, uint32_t ms)
{
  float a;
  uint32_t t;

  for (t = 0; t <= ms; t += 10)
    {
      a = 6.2832f * 1.3f * t / ms;
      preview_touch(cx + r * sinf(a) * 1.2f, cy - r * cosf(a * 1.5f), true);
      preview_step(10);
    }
}

int main(void)
{
  lv_nuttx_result_t result;
  int i;

  touch_start(&result);
  preview_step(1200);
  preview_shot("8a-touch-idle.png");

  stroke(540, 1000, 320, 1200);
  preview_shot("8-touch.png");
  preview_touch(540, 1000, false);
  preview_step(700);
  preview_shot("8b-touch-fading.png");
  preview_step(1000);

  /* Three fingers spreading out from the middle, one lifts first */

  for (i = 0; i <= 60; i++)
    {
      lv_point_t pts[3] =
        {
          { 540 - 6 * i, 1000 - 3 * i },
          { 540 + 6 * i, 1000 - 3 * i },
          { 540, 1000 + 6 * i },
        };

      preview_touch_points(i < 50 ? 3 : 2, pts);
      preview_step(10);
    }

  preview_shot("8e-touch-fingers.png");
  preview_touch_points(0, NULL);
  preview_step(2000);

  tap(540, 1733);
  preview_step(100);
  preview_shot("8c-touch-cal.png");

  /* A few pixels off, like a real panel */

  for (i = 0; i < TOUCH_TARGETS; i++)
    {
      lv_point_t p = touch_target(i);

      tap(p.x + 3 + i, p.y - 5 + 2 * i);
    }

  preview_shot("8d-touch-cal-result.png");
  tap(540, 960);
  preview_step(100);

  lv_nuttx_deinit(&result);
  lv_deinit();
  return 0;
}
