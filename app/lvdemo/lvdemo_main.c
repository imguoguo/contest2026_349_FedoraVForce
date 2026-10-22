/****************************************************************************
 * app/lvdemo/lvdemo_main.c
 *
 * Runs one LVGL demo or openvela app, chosen by the argument:
 *
 *   lvdemo benchmark [exit]   LVGL benchmark, summary on the console;
 *                             with "exit" it returns when it is done
 *   lvdemo widgets            LVGL widgets
 *   lvdemo snake              snake game of packages/demos
 *   lvdemo whackmole          whack-a-mole of packages/demos
 *
 * None of them can return by itself, lvdemo puts a back button in the
 * bottom left corner and releases LVGL on the way out like the other apps.
 * The LVGL performance overlay stays visible here.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <lvgl/lvgl.h>
#include <lvgl/demos/lv_demos.h>

#include "lvdemo.h"
#include "lvx_ui.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct lvdemo_s
{
  const char *name;
  void (*create)(void);
  void (*destroy)(void);    /* May be NULL */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_LV_USE_DEMO_BENCHMARK
static void lvdemo_benchmark(void);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct lvdemo_s g_demos[] =
{
#ifdef CONFIG_LV_USE_DEMO_BENCHMARK
  { "benchmark", lvdemo_benchmark, NULL },
#endif
#ifdef CONFIG_LV_USE_DEMO_WIDGETS
  { "widgets", lv_demo_widgets, NULL },
#endif
  { "snake", lvdemo_snake_create, lvdemo_snake_destroy },
  { "whackmole", lvdemo_whackmole_create, NULL },
};

static bool g_running;
static bool g_exit_when_done;       /* "benchmark exit" */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef CONFIG_LV_USE_DEMO_BENCHMARK
static void lvdemo_benchmark(void)
{
  static int runs;

  /* The demo sums up its results in a static table that it never clears,
   * a second run in the same boot reports the average of all runs
   */

  if (runs++ > 0)
    {
      printf("lvdemo: benchmark run %d since boot, the summary averages "
             "all of them\n", runs);
    }

  lv_demo_benchmark();
}
#endif

/* LVGL log goes to the console.  The benchmark writes its summary with
 * LV_LOG, the last line tells that it is done.
 */

#ifdef CONFIG_LV_USE_LOG
static void lvdemo_log_cb(lv_log_level_t level, const char *buf)
{
  UNUSED(level);

  fputs(buf, stdout);
  if (g_exit_when_done && strstr(buf, "All scenes avg.") != NULL)
    {
      g_running = false;
    }
}
#endif

static void lvdemo_back_cb(lv_event_t *e)
{
  UNUSED(e);
  g_running = false;
}

static void lvdemo_usage(void)
{
  int i;

  fprintf(stderr, "usage: lvdemo <demo>, one of:");
  for (i = 0; i < nitems(g_demos); i++)
    {
      fprintf(stderr, " %s", g_demos[i].name);
    }

  fprintf(stderr, "\n");
}

static const struct lvdemo_s *lvdemo_find(const char *name)
{
  int i;

  for (i = 0; i < nitems(g_demos); i++)
    {
      if (strcmp(name, g_demos[i].name) == 0)
        {
          return &g_demos[i];
        }
    }

  return NULL;
}

static int lvdemo_start(lv_nuttx_result_t *result,
                        const struct lvdemo_s *demo)
{
  lv_nuttx_dsc_t info;
  lv_obj_t *back;

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, result);

  if (result->disp == NULL || result->indev == NULL)
    {
      fprintf(stderr, "lvdemo: no display or touchscreen\n");
      return -1;
    }

#ifdef CONFIG_LV_USE_LOG
  lv_log_register_print_cb(lvdemo_log_cb);
#endif

  demo->create();

  back = ui_back(lv_layer_top(), UI_INK, lvdemo_back_cb);
  lv_obj_set_style_bg_opa(back, LV_OPA_80, 0);
  lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, UI_GAP, -UI_GAP);
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const struct lvdemo_s *demo;
  lv_nuttx_result_t result;
  uint32_t idle;

  demo = argc > 1 ? lvdemo_find(argv[1]) : NULL;
  if (demo == NULL)
    {
      lvdemo_usage();
      return 1;
    }

  /* LVGL is one global instance, shared by every task of the flat build */

  if (lv_is_initialized())
    {
      fprintf(stderr, "lvdemo: LVGL is busy, start it from the launcher\n");
      return 1;
    }

  g_exit_when_done = argc > 2 && strcmp(argv[2], "exit") == 0;
  g_running = lvdemo_start(&result, demo) == 0;
  while (g_running)
    {
      idle = lv_timer_handler();
      usleep(MAX(idle, 1) * 1000);
    }

  if (demo->destroy != NULL)
    {
      demo->destroy();
    }

  lv_nuttx_deinit(&result);
  lv_deinit();
  return 0;
}
