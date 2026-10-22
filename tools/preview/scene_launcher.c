/****************************************************************************
 * tools/preview/scene_launcher.c
 *
 * Lock screen, home and About as in docs/launcher-v2/images.  Built once
 * more per PREVIEW_LAYOUT with a different CONFIG_LVX_LAUNCHER_APPS to
 * check the tile layout.
 ****************************************************************************/

#define main launcher_main
#include "../../app/launcher/launcher_main.c"
#undef main

static uint32_t g_start;

/* Step to the middle of a second, after the clock has rolled */

static void settle(void)
{
  preview_step(1000 - (preview_now() - g_start + 500) % 1000);
}

#ifndef PREVIEW_LAYOUT
static void tap(int32_t x, int32_t y)
{
  preview_touch(x, y, true);
  preview_step(60);
  preview_touch(x, y, false);
}
#endif

static void swipe_up(int32_t x, int32_t y, int32_t dist)
{
  preview_touch(x, y, true);
  preview_step(30);
  preview_drag(x, y, x, y - dist, 200);
  preview_touch(x, y - dist, false);
}

#ifdef PREVIEW_LAYOUT

int main(void)
{
  char name[32];

  g_start = preview_now();
  launcher_start(-1);
  preview_step(500);
  swipe_up(540, 1600, 400);
  preview_step(1500);
  settle();
  snprintf(name, sizeof(name), "layout-%d.png", PREVIEW_LAYOUT);
  preview_shot(name);

  lv_obj_scroll_to_y(g_home_scr, LV_COORD_MAX, LV_ANIM_OFF);
  snprintf(name, sizeof(name), "layout-%d-end.png", PREVIEW_LAYOUT);
  preview_shot(name);
  return 0;
}

#else

int main(void)
{
  size_t free0 = 0;
  lv_mem_monitor_t mon;
  int i;

  g_start = preview_now();
  launcher_start(-1);
  preview_step(1000);
  settle();
  preview_shot("1-lock.png");

  /* Not far enough: the block springs back */

  swipe_up(540, 1500, 150);
  preview_step(100);
  preview_shot("1b-lock-bounce.png");
  preview_step(1000);

  swipe_up(540, 1500, 400);
  preview_step(150);
  preview_shot("2a-home-entering.png");
  preview_step(500);
  settle();
  preview_shot("2-home.png");

  tap(780, 1226);
  preview_step(150);
  preview_shot("3a-about-entering.png");
  preview_step(500);
  settle();
  preview_shot("3-about.png");

  tap(540, 1740);
  preview_step(500);

  /* ALL APPS sits next to MONITOR below the first screen */

  lv_obj_scroll_to_y(g_home_scr, LV_COORD_MAX, LV_ANIM_OFF);
  preview_step(100);
  preview_shot("2b-home-end.png");
  tap(780, 1500);
  preview_step(500);
  preview_shot("7-apps.png");
  lv_obj_scroll_to_y(g_apps_scr, LV_COORD_MAX, LV_ANIM_OFF);
  preview_step(100);
  preview_shot("7b-apps-end.png");
  lv_screen_load(g_home_scr);
  preview_step(100);
  lv_obj_scroll_to_y(g_home_scr, LV_COORD_MAX, LV_ANIM_OFF);
  tap(540, 1840);
  preview_step(1000);
  preview_shot("1c-relocked.png");

  /* Leaving for an app and coming back must not leak LVGL memory */

  for (i = 0; i < 10; i++)
    {
      launcher_stop();
      launcher_start(0);
      preview_step(1000);
      lv_mem_monitor(&mon);
      free0 = free0 == 0 ? mon.free_size : free0;
      printf("restart %2d: LVGL heap free %zu (%+ld)\n", i + 1,
             mon.free_size, (long)mon.free_size - (long)free0);
    }

  launcher_stop();
  return 0;
}

#endif
