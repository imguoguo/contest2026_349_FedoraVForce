/****************************************************************************
 * app/bench/bench_main.c
 *
 * LVGL benchmark app
 *
 * Five render scenes that count real display refreshes and time each
 * frame, CoreMark on one core, and on a second page the CPU / memory
 * throughput tests, the two digit recognition implementations of paint
 * side by side and screen test patterns.
 *
 * "bench run" starts the run right away, then does the CPU page tests too
 * and exits, so the numbers can be taken from the console alone.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/arch.h>

#include <lvgl/lvgl.h>

#include "digit.h"
#include "lvx_ui.h"

#if defined(CONFIG_BENCHMARK_COREMARK) && defined(CONFIG_PIPES)
#  include <sys/wait.h>
#  include <nuttx/lib/builtin.h>
#  define BENCH_COREMARK 1
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BENCH_SCENE_TIME      3000    /* ms per render scene */
#define BENCH_SCENES          5
#define BENCH_ROW_Y           740
#define BENCH_ROW_H           120
#define BENCH_BAR_X           566
#define BENCH_BAR_W           220
#define BENCH_FPS_CAP         (1000 / LV_DEF_REFR_PERIOD)

#define BENCH_COREMARK_CPU    3
#define BENCH_COREMARK_PRIO   50      /* Below the UI */

#define BENCH_TIME_MS         500
#define BENCH_INT_LOOPS       1000000
#define BENCH_MATRIX_N        64
#define BENCH_MEM_SIZE        (4 * 1024 * 1024)
#define BENCH_GRID_STEP       120
#define BENCH_PATTERNS        (nitems(g_pattern_colors) + 2)
#define BENCH_DIGIT_Y         1000

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bench_scene_s
{
  const char *name;
  void (*create)(lv_obj_t *scr);
  void (*update)(lv_obj_t *scr, uint32_t t);
};

struct bench_digit_s
{
  const char *name;
  int (*classify)(const float *img, float *prob);
};

struct bench_test_s
{
  const char *name;
  void (*run)(void);
  uint32_t work;            /* Operations or bytes per run() */
  const char *unit;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void bench_rects_create(lv_obj_t *scr);
static void bench_rounded_create(lv_obj_t *scr);
static void bench_rects_update(lv_obj_t *scr, uint32_t t);
static void bench_text_create(lv_obj_t *scr);
static void bench_text_update(lv_obj_t *scr, uint32_t t);
static void bench_arcs_create(lv_obj_t *scr);
static void bench_arcs_update(lv_obj_t *scr, uint32_t t);
static void bench_slide_create(lv_obj_t *scr);
static void bench_slide_update(lv_obj_t *scr, uint32_t t);

static void bench_int(void);
static void bench_float(void);
static void bench_memcpy(void);
static void bench_memset(void);
static void bench_cpu_cb(lv_event_t *e);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct bench_scene_s g_scenes[BENCH_SCENES] =
{
  { "Rectangles", bench_rects_create, bench_rects_update },
  { "Rounded + border", bench_rounded_create, bench_rects_update },
  { "Text", bench_text_create, bench_text_update },
  { "Arcs", bench_arcs_create, bench_arcs_update },
  { "Full-screen slide", bench_slide_create, bench_slide_update },
};

static const uint32_t g_scene_colors[] =
{
  UI_ORANGE, UI_BLUE, UI_YELLOW, UI_PAPER
};

static const struct bench_test_s g_tests[] =
{
  { "Integer (xorshift)", bench_int, 6 * BENCH_INT_LOOPS, "MOPS" },
  { "Float (matmul)", bench_float,
    2 * BENCH_MATRIX_N * BENCH_MATRIX_N * BENCH_MATRIX_N, "MFLOPS" },
  { "Memory copy", bench_memcpy, BENCH_MEM_SIZE, "MB/s" },
  { "Memory fill", bench_memset, BENCH_MEM_SIZE, "MB/s" },
};

/* TFLite Micro first, it is what paint uses */

static const struct bench_digit_s g_digits[] =
{
#ifdef CONFIG_TFLITEMICRO
  { "TFLite Micro", digit_tflm_classify },
#endif
  { "Plain C", digit_classify },
};

static const uint32_t g_pattern_colors[] =
{
  0xff0000, 0x00ff00, 0x0000ff, 0xffffff, 0x000000
};

static float g_mat_a[BENCH_MATRIX_N][BENCH_MATRIX_N];
static float g_mat_b[BENCH_MATRIX_N][BENCH_MATRIX_N];
static float g_mat_c[BENCH_MATRIX_N][BENCH_MATRIX_N];
static float g_digit_img[DIGIT_INPUTS];
static volatile uint32_t g_sink;
static uint8_t *g_mem;

static lv_obj_t *g_main_scr;
static lv_obj_t *g_cpu_scr;
static lv_obj_t *g_scene_scr;
static lv_obj_t *g_ring;
static lv_obj_t *g_avg;
static lv_obj_t *g_coremark;
static lv_obj_t *g_coremark_note;
static lv_obj_t *g_run_label;
static lv_obj_t *g_row_ms[BENCH_SCENES];
static lv_obj_t *g_row_bar[BENCH_SCENES];
static lv_obj_t *g_row_fps[BENCH_SCENES];
static lv_obj_t *g_cpu_value[nitems(g_tests)];
static lv_obj_t *g_digit_us[nitems(g_digits)];
static lv_obj_t *g_digit_acc[nitems(g_digits)];

/* Scene in progress, -1 when idle */

static int g_scene = -1;
static uint32_t g_scene_start;     /* Of the scene, then of CoreMark */
static uint32_t g_frames;
static uint64_t g_frame_ticks;     /* Perf counter ticks */
static uint64_t g_flush_ticks;
static clock_t g_refr_start;
static clock_t g_flush_start;
static bool g_rendered;
static int g_fps_sum;

#ifdef BENCH_COREMARK
static volatile int g_coremark_score;   /* < 0 while running */
#endif

static int g_pattern;
static bool g_running;
static bool g_auto;                /* "bench run": run everything, exit */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t bench_ms(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* The system tick is 1ms, so frames are timed with the perf counter.
 * Only differences of it are used, they stay right when it wraps.
 */

static clock_t bench_ticks_since(clock_t start)
{
  return up_perf_gettime() - start;
}

static uint32_t bench_ticks_to_us(uint64_t ticks)
{
  return ticks * 1000000 / up_perf_getfreq();
}

/* Swing on a sine wave, x = mid + amp * sin(t + i * phase) */

static int32_t bench_swing(int32_t mid, int32_t amp, uint32_t t, int i)
{
  return mid + amp * lv_trigo_sin((t / 10 + i * 13 + 251) % 360) /
               LV_TRIGO_SIN_MAX;
}

static bool bench_busy(void)
{
#ifdef BENCH_COREMARK
  if (g_coremark_score < 0)
    {
      return true;
    }
#endif

  return g_scene >= 0;
}

static lv_obj_t *bench_rect(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t w, int32_t h)
{
  lv_obj_t *obj = ui_box(parent, 0xffffff, 0);

  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  return obj;
}

/* Flat dark button with a label */

static lv_obj_t *bench_button(lv_obj_t *parent, uint32_t color,
                              const lv_font_t *font, const char *text,
                              lv_event_cb_t cb)
{
  lv_obj_t *btn = ui_box(parent, color, UI_RADIUS);
  lv_obj_t *label;

  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(btn, lv_color_darken(lv_color_hex(color),
                                                 LV_OPA_20),
                            LV_STATE_PRESSED);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
  label = ui_label(btn, font, 0xffffff, text);
  lv_obj_center(label);
  return btn;
}

static lv_obj_t *bench_screen(void)
{
  lv_obj_t *scr = lv_obj_create(NULL);

  lv_obj_remove_style_all(scr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, lv_color_hex(UI_INK), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  return scr;
}

/* Back button and title on the dark pages */

static void bench_header(lv_obj_t *scr, const char *title, lv_event_cb_t cb)
{
  lv_obj_set_pos(ui_back(scr, UI_INK_2, cb), UI_MARGIN, 56);
  ui_place(ui_label(scr, UI_OSWALD_120, 0xffffff, title), UI_OSWALD_120,
           222, 50);
}

/****************************************************************************
 * Name: render scenes
 *
 * Each scene is a screen of its own, so nothing under it is drawn.  An
 * animation moves the objects by the time since the scene started, like
 * any LVGL animation it runs once per refresh period before the refresh.
 ****************************************************************************/

static void bench_rects(lv_obj_t *scr, int32_t radius, int32_t border)
{
  lv_obj_t *obj;
  int i;

  for (i = 0; i < 16; i++)
    {
      obj = bench_rect(scr, 0, 220 + i * 100, 160, 160);
      lv_obj_set_style_bg_color(obj,
        lv_color_hex(g_scene_colors[i % nitems(g_scene_colors)]), 0);
      lv_obj_set_style_radius(obj, radius, 0);
      lv_obj_set_style_border_width(obj, border, 0);
      lv_obj_set_style_border_color(obj, lv_color_hex(UI_INK_2), 0);
    }
}

static void bench_rects_create(lv_obj_t *scr)
{
  bench_rects(scr, 0, 0);
}

static void bench_rounded_create(lv_obj_t *scr)
{
  bench_rects(scr, 40, 10);
}

/* Children 0 is the scene title, the rest swing on a sine wave */

static void bench_rects_update(lv_obj_t *scr, uint32_t t)
{
  uint32_t i;

  for (i = 1; i < lv_obj_get_child_count(scr); i++)
    {
      lv_obj_set_x(lv_obj_get_child(scr, i), bench_swing(440, 380, t, i));
    }
}

static void bench_text_create(lv_obj_t *scr)
{
  int i;

  for (i = 0; i < 14; i++)
    {
      lv_obj_set_y(ui_label(scr, UI_OUTFIT_52,
                            g_scene_colors[i % nitems(g_scene_colors)],
                            "openvela on LogicPi"),
                   222 + i * 118);
    }
}

static void bench_text_update(lv_obj_t *scr, uint32_t t)
{
  uint32_t i;

  for (i = 1; i < lv_obj_get_child_count(scr); i++)
    {
      lv_obj_set_x(lv_obj_get_child(scr, i), bench_swing(400, 140, t, i));
    }
}

static void bench_arcs_create(lv_obj_t *scr)
{
  lv_obj_t *arc;
  int i;

  for (i = 0; i < 6; i++)
    {
      arc = lv_arc_create(scr);
      lv_obj_remove_style_all(arc);
      lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_size(arc, 900 - i * 130, 900 - i * 130);
      lv_obj_align(arc, LV_ALIGN_CENTER, 0, 60);
      lv_obj_set_style_arc_width(arc, 40, LV_PART_INDICATOR);
      lv_obj_set_style_arc_color(arc,
        lv_color_hex(g_scene_colors[i % nitems(g_scene_colors)]),
        LV_PART_INDICATOR);
      lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
      lv_arc_set_bg_angles(arc, 0, 360);
      lv_arc_set_angles(arc, 0, 240);
    }
}

static void bench_arcs_update(lv_obj_t *scr, uint32_t t)
{
  uint32_t i;

  for (i = 1; i < lv_obj_get_child_count(scr); i++)
    {
      lv_arc_set_rotation(lv_obj_get_child(scr, i),
                          (t * (i + 1) / 20) % 360);
    }
}

/* Two full pages next to each other, moving left all the time */

static void bench_slide_create(lv_obj_t *scr)
{
  lv_obj_t *page;
  int i;

  for (i = 0; i < 2; i++)
    {
      page = ui_box(scr, i ? UI_BLUE : UI_ORANGE, 0);
      lv_obj_set_size(page, UI_SCREEN_W, UI_SCREEN_H);
      ui_place(ui_label(page, UI_OSWALD_300, 0xffffff, i ? "02" : "01"),
               UI_OSWALD_300, UI_MARGIN, 600);
      ui_place(ui_label(page, UI_OUTFIT_52, 0xffffff,
                        "openvela on LogicPi A1"),
               UI_OUTFIT_52, UI_MARGIN, 960);
    }

  /* Keep the title on top */

  lv_obj_move_foreground(lv_obj_get_child(scr, 0));
}

static void bench_slide_update(lv_obj_t *scr, uint32_t t)
{
  int32_t x = t * UI_SCREEN_W / 1500 % (2 * UI_SCREEN_W);
  int i;

  /* A page that left on the left comes back on the right */

  for (i = 0; i < 2; i++)
    {
      int32_t pos = i * UI_SCREEN_W - x;

      lv_obj_set_x(lv_obj_get_child(scr, i),
                   pos <= -UI_SCREEN_W ? pos + 2 * UI_SCREEN_W : pos);
    }
}

static void bench_scene_anim_cb(void *scr, int32_t t)
{
  g_scenes[g_scene].update(scr, t);
}

static void bench_refr_start_cb(lv_event_t *e)
{
  UNUSED(e);
  g_refr_start = up_perf_gettime();
  g_rendered = false;
}

static void bench_render_start_cb(lv_event_t *e)
{
  UNUSED(e);
  g_rendered = true;
}

static void bench_flush_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_FLUSH_START)
    {
      g_flush_start = up_perf_gettime();
    }
  else if (g_scene >= 0)
    {
      g_flush_ticks += bench_ticks_since(g_flush_start);
    }
}

static void bench_refr_ready_cb(lv_event_t *e)
{
  UNUSED(e);

  if (g_scene >= 0 && g_rendered)
    {
      g_frames++;
      g_frame_ticks += bench_ticks_since(g_refr_start);
    }
}

static void bench_scene_start(int i)
{
  lv_obj_t *title;
  lv_anim_t a;

  g_scene_scr = bench_screen();
  title = ui_label(g_scene_scr, UI_OUTFIT_34, 0xffffff, "");
  lv_label_set_text_fmt(title, "SCENE %d / %d  -  %s", i + 1, BENCH_SCENES,
                        g_scenes[i].name);
  ui_place(title, UI_OUTFIT_34, UI_MARGIN, 86);
  g_scenes[i].create(g_scene_scr);
  lv_screen_load(g_scene_scr);

  g_frames = 0;
  g_frame_ticks = 0;
  g_flush_ticks = 0;
  g_scene_start = bench_ms();
  g_scene = i;

  lv_anim_init(&a);
  lv_anim_set_var(&a, g_scene_scr);
  lv_anim_set_exec_cb(&a, bench_scene_anim_cb);
  lv_anim_set_values(&a, 0, BENCH_SCENE_TIME);
  lv_anim_set_duration(&a, BENCH_SCENE_TIME);
  lv_anim_start(&a);
}

/* Show the result of the scene that just ended in its row */

static void bench_scene_done(void)
{
  uint32_t elapsed = bench_ms() - g_scene_start;
  uint32_t frames = MAX(g_frames, 1);
  uint32_t us = bench_ticks_to_us(g_frame_ticks / frames);
  uint32_t flush = bench_ticks_to_us(g_flush_ticks / frames);
  uint32_t fps = g_frames * 1000 / elapsed;
  int i = g_scene;

  g_scene = -1;
  g_fps_sum += fps;

  /* Also on the console, to compare runs while tuning the frame rate */

  printf("bench: %-18s %3u fps  %3u.%u ms/frame  (flush %u.%u ms)\n",
         g_scenes[i].name, fps, us / 1000, us % 1000 / 100,
         flush / 1000, flush % 1000 / 100);

  lv_label_set_text_fmt(g_row_ms[i], "%u.%u ms / frame", us / 1000,
                        us % 1000 / 100);
  lv_label_set_text_fmt(g_row_fps[i], "%u", fps);
  lv_obj_set_width(g_row_bar[i], MIN(fps, BENCH_FPS_CAP) * BENCH_BAR_W /
                                 BENCH_FPS_CAP);
  lv_arc_set_value(g_ring, (i + 1) * 100 / (BENCH_SCENES + 1));

  lv_screen_load(g_main_scr);
  lv_obj_delete(g_scene_scr);
}

/****************************************************************************
 * Name: CoreMark
 *
 * The coremark builtin runs as a task of its own on one core, below the
 * UI priority.  Its stdout goes into a pipe that a thread reads for the
 * "Iterations/Sec" line, so the UI keeps running (and keeps still).
 ****************************************************************************/

#ifdef BENCH_COREMARK
static void *bench_coremark_thread(void *arg)
{
  FILE *f = fdopen((int)(intptr_t)arg, "r");
  int score = 0;
  char line[128];
  char *p;

  while (fgets(line, sizeof(line), f) != NULL)
    {
      p = strstr(line, "Iterations/Sec");
      if (p != NULL && (p = strchr(p, ':')) != NULL)
        {
          score = atoi(p + 1);
        }
    }

  fclose(f);
  g_coremark_score = score;
  return NULL;
}

static int bench_coremark_start(void)
{
  FAR const struct builtin_s *app;
  pthread_t thread;
  cpu_set_t cpus;
  int index = builtin_isavail("coremark");
  int fd[2];
  int out;
  pid_t pid;

  if (index < 0 || pipe(fd) < 0)
    {
      return -1;
    }

  /* The task inherits stdout as it is while it is created */

  app = builtin_for_index(index);
  out = dup(STDOUT_FILENO);
  fflush(stdout);
  dup2(fd[1], STDOUT_FILENO);
  pid = task_create(app->name, BENCH_COREMARK_PRIO, app->stacksize,
                    app->main, NULL);
  dup2(out, STDOUT_FILENO);
  close(out);
  close(fd[1]);

  if (pid < 0)
    {
      close(fd[0]);
      return -1;
    }

  CPU_ZERO(&cpus);
  CPU_SET(BENCH_COREMARK_CPU, &cpus);
  sched_setaffinity(pid, sizeof(cpus), &cpus);

  g_coremark_score = -1;
  pthread_create(&thread, NULL, bench_coremark_thread,
                 (void *)(intptr_t)fd[0]);
  pthread_detach(thread);
  return 0;
}
#endif

static void bench_finish(void)
{
  lv_arc_set_value(g_ring, 100);
  lv_label_set_text(g_run_label, "RUN AGAIN");

  if (g_auto)
    {
      lv_screen_load(g_cpu_scr);
      bench_cpu_cb(NULL);
      printf("bench: done\n");
      g_running = false;
    }
}

/* Drives the run: scenes one after another, then CoreMark */

static void bench_run_timer_cb(lv_timer_t *timer)
{
  int next = g_scene + 1;

  if (g_scene >= 0)
    {
      if (bench_ms() - g_scene_start < BENCH_SCENE_TIME)
        {
          return;
        }

      bench_scene_done();
      if (next < BENCH_SCENES)
        {
          bench_scene_start(next);
          return;
        }

      lv_label_set_text_fmt(g_avg, "%d", g_fps_sum / BENCH_SCENES);

#ifdef BENCH_COREMARK
      g_scene_start = bench_ms();
      lv_label_set_text(g_coremark, "....");
      if (bench_coremark_start() < 0)
        {
          lv_label_set_text(g_coremark, "N/A");
          lv_label_set_text(g_coremark_note, "coremark failed to start");
        }
#endif
    }

#ifdef BENCH_COREMARK
  if (g_coremark_score < 0)
    {
      /* Keep showing that it runs, it takes about 15 seconds */

      lv_label_set_text_fmt(g_run_label, "COREMARK  %u S",
                            (unsigned)(bench_ms() - g_scene_start) / 1000);
      return;
    }

  if (g_coremark_score > 0)
    {
      lv_label_set_text_fmt(g_coremark, "%d", g_coremark_score);
      printf("bench: CoreMark %d iterations/s\n", g_coremark_score);
    }
#endif

  bench_finish();
  lv_timer_delete(timer);
}

static void bench_run_cb(lv_event_t *e)
{
  int i;

  UNUSED(e);

  if (bench_busy())
    {
      return;
    }

  for (i = 0; i < BENCH_SCENES; i++)
    {
      lv_label_set_text(g_row_ms[i], "");
      lv_label_set_text(g_row_fps[i], "--");
      lv_obj_set_width(g_row_bar[i], 0);
    }

  lv_label_set_text(g_avg, "--");
  lv_arc_set_value(g_ring, 0);
  g_fps_sum = 0;
  bench_scene_start(0);
  lv_timer_create(bench_run_timer_cb, 100, NULL);
}

/****************************************************************************
 * Name: CPU and memory tests
 ****************************************************************************/

static void bench_int(void)
{
  uint32_t x = g_sink | 1;
  int i;

  for (i = 0; i < BENCH_INT_LOOPS; i++)
    {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
    }

  g_sink = x;
}

static void bench_float(void)
{
  int i;
  int j;
  int k;

  for (i = 0; i < BENCH_MATRIX_N; i++)
    {
      for (j = 0; j < BENCH_MATRIX_N; j++)
        {
          float sum = 0.0f;

          for (k = 0; k < BENCH_MATRIX_N; k++)
            {
              sum += g_mat_a[i][k] * g_mat_b[k][j];
            }

          g_mat_c[i][j] = sum;
        }
    }

  g_sink = (uint32_t)g_mat_c[BENCH_MATRIX_N - 1][BENCH_MATRIX_N - 1];
}

static void bench_memcpy(void)
{
  memcpy(g_mem + BENCH_MEM_SIZE, g_mem, BENCH_MEM_SIZE);
}

static void bench_memset(void)
{
  memset(g_mem, g_sink, BENCH_MEM_SIZE);
}

/* Repeat a test for BENCH_TIME_MS, return millions of work units/s */

static uint32_t bench_cpu(const struct bench_test_s *t)
{
  uint32_t start = bench_ms();
  uint32_t elapsed;
  uint64_t runs = 0;

  do
    {
      t->run();
      runs++;
      elapsed = bench_ms() - start;
    }
  while (elapsed < BENCH_TIME_MS);

  return runs * t->work / elapsed / 1000;
}

/* Classify the MNIST samples, only the inference is timed.  Returns the
 * number recognized right, or -1 if the implementation cannot run.
 */

static int bench_digit(const struct bench_digit_s *d, uint32_t *us)
{
  float prob[DIGIT_CLASSES];
  uint64_t ticks = 0;
  clock_t start;
  int right = 0;
  int best;
  int i;
  int j;

  for (i = 0; i < DIGIT_SAMPLES; i++)
    {
      for (j = 0; j < DIGIT_INPUTS; j++)
        {
          g_digit_img[j] = g_digit_sample_images[i][j] / 255.0f;
        }

      /* The first TFLite Micro call also sets up the interpreter */

      if (i == 0 && d->classify(g_digit_img, prob) < 0)
        {
          return -1;
        }

      start = up_perf_gettime();
      best = d->classify(g_digit_img, prob);
      ticks += bench_ticks_since(start);
      right += best == g_digit_sample_labels[i];
    }

  *us = bench_ticks_to_us(ticks / DIGIT_SAMPLES);
  return right;
}

static void bench_cpu_cb(lv_event_t *e)
{
  uint32_t value;
  uint32_t us;
  int right;
  int i;

  UNUSED(e);

  for (i = 0; i < nitems(g_tests); i++)
    {
      lv_label_set_text(g_cpu_value[i], "...");
      lv_refr_now(NULL);
      value = bench_cpu(&g_tests[i]);
      lv_label_set_text_fmt(g_cpu_value[i], "%u %s", value,
                            g_tests[i].unit);
      printf("bench: %-18s %u %s\n", g_tests[i].name, value,
             g_tests[i].unit);
    }

  for (i = 0; i < nitems(g_digits); i++)
    {
      lv_label_set_text(g_digit_us[i], "...");
      lv_refr_now(NULL);
      right = bench_digit(&g_digits[i], &us);
      if (right < 0)
        {
          lv_label_set_text(g_digit_us[i], "N/A");
          lv_label_set_text(g_digit_acc[i], "failed to set up");
          printf("bench: digit %s failed to set up\n", g_digits[i].name);
          continue;
        }

      lv_label_set_text_fmt(g_digit_us[i], "%u us", us);
      lv_label_set_text_fmt(g_digit_acc[i], "%d.%d%% right, %d / %d",
                            right * 100 / DIGIT_SAMPLES,
                            right * 1000 / DIGIT_SAMPLES % 10, right,
                            DIGIT_SAMPLES);
      printf("bench: digit %-12s %u us/inference  %d / %d right\n",
             g_digits[i].name, us, right, DIGIT_SAMPLES);
    }
}

/****************************************************************************
 * Name: test patterns
 ****************************************************************************/

static void bench_pattern_show(lv_obj_t *overlay)
{
  static const uint32_t ramps[] =
  {
    0xff0000, 0x00ff00, 0x0000ff, 0xffffff
  };

  lv_obj_t *obj;
  int i;

  lv_obj_clean(overlay);
  lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);

  if (g_pattern < nitems(g_pattern_colors))
    {
      /* Solid colors to spot dead or stuck pixels */

      lv_obj_set_style_bg_color(overlay,
                                lv_color_hex(g_pattern_colors[g_pattern]),
                                0);
    }
  else if (g_pattern == nitems(g_pattern_colors))
    {
      /* Black to full intensity ramps to check color depth and banding */

      for (i = 0; i < nitems(ramps); i++)
        {
          obj = bench_rect(overlay, 0, i * UI_SCREEN_H / nitems(ramps),
                           UI_SCREEN_W, UI_SCREEN_H / nitems(ramps));
          lv_obj_set_style_bg_color(obj, lv_color_black(), 0);
          lv_obj_set_style_bg_grad_color(obj, lv_color_hex(ramps[i]), 0);
          lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, 0);
        }
    }
  else
    {
      /* 1px grid, including the outermost rows and columns */

      for (i = 0; i <= UI_SCREEN_W; i += BENCH_GRID_STEP)
        {
          bench_rect(overlay, MIN(i, UI_SCREEN_W - 1), 0, 1, UI_SCREEN_H);
        }

      for (i = 0; i <= UI_SCREEN_H; i += BENCH_GRID_STEP)
        {
          bench_rect(overlay, 0, MIN(i, UI_SCREEN_H - 1), UI_SCREEN_W, 1);
        }
    }
}

static void bench_pattern_cb(lv_event_t *e)
{
  lv_obj_t *overlay = lv_event_get_target(e);

  if (++g_pattern < BENCH_PATTERNS)
    {
      bench_pattern_show(overlay);
    }
  else
    {
      lv_obj_delete_async(overlay);
    }
}

static void bench_patterns_cb(lv_event_t *e)
{
  lv_obj_t *overlay = bench_rect(lv_layer_top(), 0, 0, UI_SCREEN_W,
                                 UI_SCREEN_H);

  UNUSED(e);

  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(overlay, bench_pattern_cb, LV_EVENT_CLICKED, NULL);
  g_pattern = 0;
  bench_pattern_show(overlay);
}

/****************************************************************************
 * Name: pages
 ****************************************************************************/

/* Leaving waits for nothing, so not while CoreMark runs */

static void bench_home_cb(lv_event_t *e)
{
  UNUSED(e);
  g_running = bench_busy();
}

static void bench_main_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_screen_load(g_main_scr);
}

static void bench_cpu_open_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_screen_load(g_cpu_scr);
}

/* Half width button to the second group, column 0 or 1 */

static void bench_more(lv_obj_t *scr, const char *text, int col, int32_t y,
                       lv_event_cb_t cb)
{
  lv_obj_t *btn = bench_button(scr, UI_INK_2, UI_OUTFIT_34, text, cb);

  lv_obj_set_pos(btn, UI_MARGIN + col * (UI_CONTENT_W + UI_GAP) / 2, y);
  lv_obj_set_size(btn, (UI_CONTENT_W - UI_GAP) / 2, 120);
  lv_obj_align(lv_obj_get_child(btn, 0), LV_ALIGN_LEFT_MID, 40, 0);
  lv_obj_align(ui_label(btn, UI_ICON_24, UI_MUTED, LV_SYMBOL_RIGHT),
               LV_ALIGN_RIGHT_MID, -40, 0);
}

static lv_obj_t *bench_tag(lv_obj_t *parent, const char *text, int32_t x,
                           int32_t y)
{
  lv_obj_t *label = ui_tag(parent, UI_MUTED, text);

  ui_place(label, UI_OUTFIT_26, x, y);
  return label;
}

static void bench_create_main(void)
{
  lv_obj_t *scr = bench_screen();
  lv_obj_t *obj;
  int32_t y;
  int i;

  bench_header(scr, "BENCH", bench_home_cb);

  /* Average frame rate in the ring, doubles as the run progress */

  g_ring = lv_arc_create(scr);
  lv_obj_remove_style_all(g_ring);
  lv_obj_remove_flag(g_ring, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(g_ring, UI_MARGIN, 220);
  lv_obj_set_size(g_ring, 460, 460);
  lv_obj_set_style_arc_width(g_ring, 40, 0);
  lv_obj_set_style_arc_color(g_ring, lv_color_hex(UI_INK_2), 0);
  lv_obj_set_style_arc_width(g_ring, 40, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(g_ring, lv_color_hex(UI_ORANGE),
                             LV_PART_INDICATOR);
  lv_arc_set_bg_angles(g_ring, 0, 360);
  lv_arc_set_rotation(g_ring, 270);
  lv_arc_set_value(g_ring, 0);

  g_avg = ui_label(g_ring, UI_OSWALD_120, 0xffffff, "--");
  lv_obj_align(g_avg, LV_ALIGN_CENTER, 0, -20);
  obj = ui_tag(g_ring, UI_MUTED, "AVG FPS");
  lv_obj_align(obj, LV_ALIGN_CENTER, 0, 70);

  bench_tag(scr, "COREMARK", 600, 263);
  g_coremark = ui_label(scr, UI_OSWALD_120, 0xffffff, "----");
  ui_place(g_coremark, UI_OSWALD_120, 600, 315);
  g_coremark_note = ui_label(scr, UI_OUTFIT_26, UI_MUTED, "");
  lv_obj_set_width(g_coremark_note, 408);
  ui_place(g_coremark_note, UI_OUTFIT_26, 600, 443);
#ifdef BENCH_COREMARK
  lv_label_set_text(g_coremark_note, "iterations / s, 1 core");
#else
  lv_label_set_text(g_coremark, "N/A");
  lv_label_set_text(g_coremark_note, "not built in: needs "
                    "BENCHMARK_COREMARK and PIPES");
#endif

  obj = ui_label(scr, UI_OUTFIT_26, UI_DIM, "");
  lv_obj_set_width(obj, 408);
  lv_label_set_text_fmt(obj, "Frame rate capped at %d by the %d ms "
                        "refresh period", BENCH_FPS_CAP,
                        LV_DEF_REFR_PERIOD);
  ui_place(obj, UI_OUTFIT_26, 600, 563);

  /* One row per scene: name, frame time, bar and frame rate */

  for (i = 0; i < BENCH_SCENES; i++)
    {
      y = BENCH_ROW_Y + i * BENCH_ROW_H;

      obj = ui_box(scr, UI_INK_2, 0);
      lv_obj_set_pos(obj, UI_MARGIN, y + BENCH_ROW_H - 2);
      lv_obj_set_size(obj, UI_CONTENT_W, 2);

      ui_place(ui_label(scr, UI_OUTFIT_34, 0xffffff, g_scenes[i].name),
               UI_OUTFIT_34, UI_MARGIN, y + 26);
      g_row_ms[i] = ui_label(scr, UI_OUTFIT_26, UI_MUTED, "");
      ui_place(g_row_ms[i], UI_OUTFIT_26, UI_MARGIN, y + 76);

      obj = ui_box(scr, UI_INK_2, 0);
      lv_obj_set_pos(obj, UI_MARGIN + BENCH_BAR_X, y + 52);
      lv_obj_set_size(obj, BENCH_BAR_W, 14);
      g_row_bar[i] = ui_box(obj, UI_ORANGE, 0);
      lv_obj_set_size(g_row_bar[i], 0, 14);

      g_row_fps[i] = ui_label(scr, UI_OSWALD_64, 0xffffff, "--");
      lv_obj_align(g_row_fps[i], LV_ALIGN_TOP_RIGHT, -UI_MARGIN,
                   y + 30 - ui_ink_top(UI_OSWALD_64));
    }

  /* Second group: the older CPU / memory tests and test patterns */

  y = BENCH_ROW_Y + BENCH_SCENES * BENCH_ROW_H + 60;
  bench_more(scr, "CPU & memory", 0, y, bench_cpu_open_cb);
  bench_more(scr, "Test patterns", 1, y, bench_patterns_cb);

  obj = bench_button(scr, UI_ORANGE, UI_OSWALD_64, "RUN BENCHMARK",
                     bench_run_cb);
  lv_obj_set_pos(obj, UI_MARGIN, 1658);
  lv_obj_set_size(obj, UI_CONTENT_W, 150);
  g_run_label = lv_obj_get_child(obj, 0);

  g_main_scr = scr;
}

static void bench_create_cpu(void)
{
  lv_obj_t *scr = bench_screen();
  lv_obj_t *obj;
  int32_t y;
  int i;

  bench_header(scr, "CPU", bench_main_cb);
  bench_tag(scr, "0.5 S PER TEST, 1 CORE", UI_MARGIN, 263);

  for (i = 0; i < nitems(g_tests); i++)
    {
      y = 340 + i * BENCH_ROW_H;

      obj = ui_box(scr, UI_INK_2, 0);
      lv_obj_set_pos(obj, UI_MARGIN, y + BENCH_ROW_H - 2);
      lv_obj_set_size(obj, UI_CONTENT_W, 2);

      ui_place(ui_label(scr, UI_OUTFIT_34, 0xffffff, g_tests[i].name),
               UI_OUTFIT_34, UI_MARGIN, y + 44);
      g_cpu_value[i] = ui_label(scr, UI_OSWALD_64, 0xffffff, "--");
      lv_obj_align(g_cpu_value[i], LV_ALIGN_TOP_RIGHT, -UI_MARGIN,
                   y + 30 - ui_ink_top(UI_OSWALD_64));
    }

  /* paint's recognition on TFLite Micro and the plain C version */

  bench_tag(scr, "DIGIT RECOGNITION, PER INFERENCE", UI_MARGIN,
            BENCH_DIGIT_Y - 77);

  for (i = 0; i < nitems(g_digits); i++)
    {
      y = BENCH_DIGIT_Y + i * BENCH_ROW_H;

      obj = ui_box(scr, UI_INK_2, 0);
      lv_obj_set_pos(obj, UI_MARGIN, y + BENCH_ROW_H - 2);
      lv_obj_set_size(obj, UI_CONTENT_W, 2);

      ui_place(ui_label(scr, UI_OUTFIT_34, 0xffffff, g_digits[i].name),
               UI_OUTFIT_34, UI_MARGIN, y + 26);
      g_digit_acc[i] = ui_label(scr, UI_OUTFIT_26, UI_MUTED, "");
      lv_label_set_text_fmt(g_digit_acc[i], "%d MNIST test images",
                            DIGIT_SAMPLES);
      ui_place(g_digit_acc[i], UI_OUTFIT_26, UI_MARGIN, y + 76);
      g_digit_us[i] = ui_label(scr, UI_OSWALD_64, 0xffffff, "--");
      lv_obj_align(g_digit_us[i], LV_ALIGN_TOP_RIGHT, -UI_MARGIN,
                   y + 30 - ui_ink_top(UI_OSWALD_64));
    }

  obj = bench_button(scr, UI_ORANGE, UI_OSWALD_64, "RUN", bench_cpu_cb);
  lv_obj_set_pos(obj, UI_MARGIN, 1658);
  lv_obj_set_size(obj, UI_CONTENT_W, 150);

  g_cpu_scr = scr;
}

static int bench_start(lv_nuttx_result_t *result)
{
  lv_nuttx_dsc_t info;
  int i;

  for (i = 0; i < BENCH_MATRIX_N * BENCH_MATRIX_N; i++)
    {
      g_mat_a[i / BENCH_MATRIX_N][i % BENCH_MATRIX_N] = i % 7;
      g_mat_b[i / BENCH_MATRIX_N][i % BENCH_MATRIX_N] = i % 5;
    }

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, result);

  if (result->disp == NULL || result->indev == NULL)
    {
      fprintf(stderr, "bench: no display or touchscreen\n");
      return -1;
    }

  ui_hide_perf();

  lv_display_add_event_cb(result->disp, bench_refr_start_cb,
                          LV_EVENT_REFR_START, NULL);
  lv_display_add_event_cb(result->disp, bench_render_start_cb,
                          LV_EVENT_RENDER_START, NULL);
  lv_display_add_event_cb(result->disp, bench_flush_cb,
                          LV_EVENT_FLUSH_START, NULL);
  lv_display_add_event_cb(result->disp, bench_flush_cb,
                          LV_EVENT_FLUSH_FINISH, NULL);
  lv_display_add_event_cb(result->disp, bench_refr_ready_cb,
                          LV_EVENT_REFR_READY, NULL);
  bench_create_cpu();
  bench_create_main();
  lv_screen_load(g_main_scr);
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_result_t result;
  uint32_t idle;

  /* LVGL is one global instance, shared by every task of the flat build */

  if (lv_is_initialized())
    {
      fprintf(stderr, "bench: LVGL is busy, start it from the launcher\n");
      return 1;
    }

  g_mem = malloc(2 * BENCH_MEM_SIZE);
  if (g_mem == NULL)
    {
      fprintf(stderr, "bench: memory alloc failed\n");
      return 1;
    }

  g_running = bench_start(&result) == 0;
  g_auto = argc > 1 && strcmp(argv[1], "run") == 0;
  if (g_running && g_auto)
    {
      bench_run_cb(NULL);
    }

  while (g_running)
    {
      idle = lv_timer_handler();
      usleep(MAX(idle, 1) * 1000);
    }

  lv_nuttx_deinit(&result);
  lv_deinit();
  free(g_mem);
  return 0;
}
