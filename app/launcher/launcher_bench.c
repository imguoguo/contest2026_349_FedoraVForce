/****************************************************************************
 * app/launcher/launcher_bench.c
 *
 * Benchmark screen: a short LVGL render test measured on the real display
 * path, then CoreMark (when it is built in) on one core.
 *
 * Each render scene runs full screen for a fixed time while every display
 * refresh is counted and timed.  The frame rate is capped by LVGL's refresh
 * period; the per-frame time shows the actual rendering cost.
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
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "launcher_ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BENCH_WARMUP_MS       300
#define BENCH_SCENE_MS        2000
#define BENCH_RING            460

#if defined(CONFIG_BENCHMARK_COREMARK) && defined(CONFIG_PIPES)
#  define BENCH_HAVE_COREMARK 1
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bench_scene_s
{
  const char *name;
  void (*build)(lv_obj_t *stage);
  lv_obj_t *bar;
  lv_obj_t *fps;
  lv_obj_t *ms;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void bench_build_rects(lv_obj_t *stage);
static void bench_build_rounded(lv_obj_t *stage);
static void bench_build_text(lv_obj_t *stage);
static void bench_build_arcs(lv_obj_t *stage);
static void bench_build_slide(lv_obj_t *stage);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct bench_scene_s g_scenes[] =
{
  { "Rectangles",        bench_build_rects   },
  { "Rounded + border",  bench_build_rounded },
  { "Text",              bench_build_text    },
  { "Arcs",              bench_build_arcs    },
  { "Full-screen slide", bench_build_slide   },
};

static const uint32_t g_colors[] =
{
  COLOR_ORANGE, COLOR_BLUE, COLOR_YELLOW, COLOR_PAPER
};

static lv_obj_t *g_scr;
static lv_obj_t *g_ring;
static lv_obj_t *g_avg;
static lv_obj_t *g_cm;
static lv_obj_t *g_cm_unit;
static lv_obj_t *g_btn;
static lv_obj_t *g_btn_label;
static lv_obj_t *g_stage;

static int g_scene;
static bool g_measuring;
static uint32_t g_frames;
static uint32_t g_frame_us;
static uint32_t g_start_tick;
static uint32_t g_fps_sum;

#ifdef BENCH_HAVE_COREMARK
extern int coremark_main(int argc, char *argv[]);

static volatile bool g_cm_done;
static char g_cm_out[1024];
static int g_cm_len;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void bench_anim_x_cb(void *obj, int32_t v)
{
  lv_obj_set_style_translate_x(obj, v, 0);
}

static void bench_anim_y_cb(void *obj, int32_t v)
{
  lv_obj_set_style_translate_y(obj, v, 0);
}

static void bench_anim_arc_cb(void *obj, int32_t v)
{
  lv_arc_set_value(obj, v);
}

static void bench_loop(lv_obj_t *obj, lv_anim_exec_xcb_t cb, int32_t from,
                       int32_t to, uint32_t ms)
{
  lv_anim_t a;

  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_exec_cb(&a, cb);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_playback_duration(&a, ms);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);
}

/****************************************************************************
 * Name: render scenes
 ****************************************************************************/

static void bench_build_boxes(lv_obj_t *stage, int32_t radius,
                              int32_t border)
{
  int i;

  for (i = 0; i < 16; i++)
    {
      lv_obj_t *box = launcher_box(stage, 160, 160,
                                   g_colors[i % nitems(g_colors)], radius);

      lv_obj_set_style_border_width(box, border, 0);
      lv_obj_set_style_border_color(box, lv_color_hex(COLOR_PAPER), 0);
      lv_obj_set_pos(box, 40 + (i % 2) * 120, 220 + i * 100);
      bench_loop(box, bench_anim_x_cb, 0, 660, 700 + i * 37);
    }
}

static void bench_build_rects(lv_obj_t *stage)
{
  bench_build_boxes(stage, 0, 0);
}

static void bench_build_rounded(lv_obj_t *stage)
{
  bench_build_boxes(stage, 48, 6);
}

static void bench_build_text(lv_obj_t *stage)
{
  int i;

  for (i = 0; i < 14; i++)
    {
      lv_obj_t *label = launcher_label(stage, "openvela on LogicPi",
                                       &launcher_font_outfit52,
                                       g_colors[i % nitems(g_colors)]);

      lv_obj_set_pos(label, 40, 230 + i * 118);
      bench_loop(label, bench_anim_x_cb, 0, 480, 800 + i * 41);
    }
}

static void bench_build_arcs(lv_obj_t *stage)
{
  int i;

  for (i = 0; i < 9; i++)
    {
      lv_obj_t *arc = lv_arc_create(stage);

      lv_obj_set_size(arc, 300, 300);
      lv_obj_set_pos(arc, 30 + (i % 3) * 340, 260 + (i / 3) * 440);
      lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
      lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_arc_width(arc, 30, LV_PART_MAIN);
      lv_obj_set_style_arc_width(arc, 30, LV_PART_INDICATOR);
      lv_obj_set_style_arc_color(arc, lv_color_hex(COLOR_INK_2),
                                 LV_PART_MAIN);
      lv_obj_set_style_arc_color(arc,
                                 lv_color_hex(g_colors[i % 3]),
                                 LV_PART_INDICATOR);
      bench_loop(arc, bench_anim_arc_cb, 0, 100, 600 + i * 53);
    }
}

static void bench_build_slide(lv_obj_t *stage)
{
  lv_obj_t *page = launcher_box(stage, LAUNCHER_SCREEN_W,
                                LAUNCHER_SCREEN_H, COLOR_BG, 0);
  lv_obj_t *obj;
  int i;

  for (i = 0; i < 6; i++)
    {
      obj = launcher_box(page, LAUNCHER_CONTENT_W, 220,
                         g_colors[i % nitems(g_colors)], LAUNCHER_RADIUS);
      lv_obj_set_pos(obj, LAUNCHER_MARGIN, 220 + i * 260);
      launcher_label(obj, "SLIDE", &launcher_font_oswald120, COLOR_INK);
    }

  bench_loop(page, bench_anim_y_cb, 0, -LAUNCHER_SCREEN_H / 2, 900);
}

/****************************************************************************
 * Name: run control
 ****************************************************************************/

static void bench_start_scene(void);
static void bench_start_coremark(void);

static void bench_finish_render(void)
{
  int32_t avg = g_fps_sum / nitems(g_scenes);

  lv_arc_set_value(g_ring, nitems(g_scenes));
  launcher_count_up(g_avg, "%d", avg, 0);
  bench_start_coremark();
}

static void bench_end_scene_cb(lv_timer_t *timer)
{
  struct bench_scene_s *s = &g_scenes[g_scene];
  uint32_t elapsed = lv_tick_elaps(g_start_tick);
  uint32_t fps;
  uint32_t us;

  UNUSED(timer);

  g_measuring = false;
  fps = elapsed ? g_frames * 1000 / elapsed : 0;
  us = g_frames ? g_frame_us / g_frames : 0;
  g_fps_sum += fps;

  lv_obj_delete(g_stage);
  g_stage = NULL;

  lv_bar_set_value(s->bar, LV_MIN(100, fps * LV_DEF_REFR_PERIOD / 10),
                   LV_ANIM_ON);
  launcher_count_up(s->fps, "%d", fps, 0);
  lv_label_set_text_fmt(s->ms, "%u.%u ms / frame", (unsigned)us / 1000,
                        (unsigned)(us / 100) % 10);
  lv_arc_set_value(g_ring, g_scene + 1);

  if (++g_scene < (int)nitems(g_scenes))
    {
      bench_start_scene();
    }
  else
    {
      bench_finish_render();
    }
}

static void bench_measure_cb(lv_timer_t *timer)
{
  lv_timer_t *end;

  UNUSED(timer);

  g_frames = 0;
  g_frame_us = 0;
  g_start_tick = lv_tick_get();
  g_measuring = true;

  end = lv_timer_create(bench_end_scene_cb, BENCH_SCENE_MS, NULL);
  lv_timer_set_repeat_count(end, 1);
}

static void bench_start_scene(void)
{
  struct bench_scene_s *s = &g_scenes[g_scene];
  lv_timer_t *timer;
  lv_obj_t *obj;

  g_stage = launcher_box(g_scr, LAUNCHER_SCREEN_W, LAUNCHER_SCREEN_H,
                         COLOR_INK, 0);
  lv_obj_add_flag(g_stage, LV_OBJ_FLAG_CLICKABLE);
  s->build(g_stage);

  obj = launcher_label(g_stage, "", &launcher_font_outfit34, COLOR_PAPER);
  lv_label_set_text_fmt(obj, "SCENE %d / %d  -  %s", g_scene + 1,
                        (int)nitems(g_scenes), s->name);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN, 80);

  timer = lv_timer_create(bench_measure_cb, BENCH_WARMUP_MS, NULL);
  lv_timer_set_repeat_count(timer, 1);
}

static void bench_set_button(const char *text, bool busy)
{
  lv_label_set_text(g_btn_label, text);
  lv_obj_set_style_bg_color(g_btn, lv_color_hex(busy ? COLOR_INK_2 :
                                                COLOR_ORANGE), 0);
  lv_obj_set_style_text_color(g_btn_label, lv_color_hex(busy ? COLOR_MUTED :
                                                        COLOR_PAPER), 0);
  if (busy)
    {
      lv_obj_add_state(g_btn, LV_STATE_DISABLED);
    }
  else
    {
      lv_obj_remove_state(g_btn, LV_STATE_DISABLED);
    }
}

#ifdef BENCH_HAVE_COREMARK

/* CoreMark prints its report to stdout; read it back through a pipe */

static void *bench_cm_reader(void *arg)
{
  int fd = (int)(intptr_t)arg;
  char sink[64];
  ssize_t n;

  while ((n = read(fd, g_cm_len < (int)sizeof(g_cm_out) - 1 ?
                   g_cm_out + g_cm_len : sink,
                   g_cm_len < (int)sizeof(g_cm_out) - 1 ?
                   sizeof(g_cm_out) - 1 - g_cm_len : sizeof(sink))) > 0)
    {
      if (g_cm_len < (int)sizeof(g_cm_out) - 1)
        {
          g_cm_len += n;
        }
    }

  g_cm_out[g_cm_len] = '\0';
  close(fd);
  return NULL;
}

static void *bench_cm_thread(void *arg)
{
  char *argv[] =
    {
      "coremark", NULL
    };

  pthread_t reader;
  int fds[2];
  int saved;

  UNUSED(arg);

  if (pipe(fds) < 0)
    {
      g_cm_done = true;
      return NULL;
    }

  if (pthread_create(&reader, NULL, bench_cm_reader,
                     (void *)(intptr_t)fds[0]) != 0)
    {
      close(fds[0]);
      close(fds[1]);
      g_cm_done = true;
      return NULL;
    }

  fflush(stdout);
  saved = dup(STDOUT_FILENO);
  if (saved >= 0)
    {
      dup2(fds[1], STDOUT_FILENO);
    }

  close(fds[1]);

  if (saved >= 0)
    {
      coremark_main(1, argv);
      fflush(stdout);
      dup2(saved, STDOUT_FILENO);
      close(saved);
    }

  pthread_join(reader, NULL);
  g_cm_done = true;
  return NULL;
}

static void bench_cm_poll_cb(lv_timer_t *timer)
{
  const char *p;

  if (!g_cm_done)
    {
      return;
    }

  lv_timer_delete(timer);
  lv_obj_set_style_arc_color(g_ring, lv_color_hex(COLOR_ORANGE),
                             LV_PART_INDICATOR);

  p = strstr(g_cm_out, "Iterations/Sec");
  p = p != NULL ? strchr(p, ':') : NULL;
  if (p != NULL)
    {
      launcher_count_up(g_cm, "%d", (int32_t)strtod(p + 1, NULL), 0);
      lv_label_set_text(g_cm_unit, "iterations / s, 1 core");
    }
  else
    {
      lv_label_set_text(g_cm, "ERR");
      lv_label_set_text(g_cm_unit, "no CoreMark report");
    }

  bench_set_button("RUN AGAIN", false);
}

static void bench_start_coremark(void)
{
  struct sched_param param;
  pthread_attr_t attr;
  pthread_t thread;

  /* CoreMark times itself with the wall clock, so the UI stays still while
   * it runs: draw the "running" state once, then let CoreMark have the CPU.
   * It runs below the UI priority so Back still responds.
   */

  lv_obj_set_style_arc_color(g_ring, lv_color_hex(COLOR_DIM),
                             LV_PART_INDICATOR);
  lv_label_set_text(g_cm_unit, "running about 15 s, screen held still");
  bench_set_button("COREMARK RUNNING", true);
  lv_refr_now(NULL);

  g_cm_done = false;
  g_cm_len = 0;
  g_cm_out[0] = '\0';

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 16384);
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  param.sched_priority = SCHED_PRIORITY_DEFAULT - 20;
  pthread_attr_setschedparam(&attr, &param);
  if (pthread_create(&thread, &attr, bench_cm_thread, NULL) != 0)
    {
      g_cm_done = true;
    }
  else
    {
      pthread_detach(thread);
    }

  pthread_attr_destroy(&attr);
  lv_timer_create(bench_cm_poll_cb, 250, NULL);
}

#else

static void bench_start_coremark(void)
{
  lv_label_set_text(g_cm, "N/A");
  lv_label_set_text(g_cm_unit, "not built in: needs BENCHMARK_COREMARK "
                    "and PIPES");
  bench_set_button("RUN AGAIN", false);
}

#endif

static void bench_run_cb(lv_event_t *e)
{
  int i;

  UNUSED(e);

  for (i = 0; i < (int)nitems(g_scenes); i++)
    {
      lv_bar_set_value(g_scenes[i].bar, 0, LV_ANIM_OFF);
      lv_label_set_text(g_scenes[i].fps, "--");
      lv_label_set_text(g_scenes[i].ms, "");
    }

  lv_label_set_text(g_avg, "--");
  lv_label_set_text(g_cm, "----");
  lv_arc_set_value(g_ring, 0);
  bench_set_button("RUNNING", true);

  g_fps_sum = 0;
  g_scene = 0;
  bench_start_scene();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void launcher_bench_frame(uint32_t render_us)
{
  if (g_measuring)
    {
      g_frames++;
      g_frame_us += render_us;
    }
}

lv_obj_t *launcher_bench_create(lv_event_cb_t back_cb)
{
  lv_obj_t *list;
  lv_obj_t *obj;
  int i;

  g_scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(g_scr, lv_color_hex(COLOR_INK), 0);
  lv_obj_clear_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);

  obj = launcher_square_btn(g_scr, LV_SYMBOL_LEFT, 112, COLOR_INK_2,
                            COLOR_PAPER, back_cb);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN, 56);

  obj = launcher_label(g_scr, "BENCH", &launcher_font_oswald120,
                       COLOR_PAPER);
  lv_obj_set_pos(obj, 220, 36);

  /* Progress ring with the average frame rate inside */

  g_ring = lv_arc_create(g_scr);
  lv_obj_set_size(g_ring, BENCH_RING, BENCH_RING);
  lv_obj_set_pos(g_ring, LAUNCHER_MARGIN, 220);
  lv_arc_set_bg_angles(g_ring, 0, 360);
  lv_arc_set_rotation(g_ring, 270);
  lv_arc_set_range(g_ring, 0, nitems(g_scenes));
  lv_arc_set_value(g_ring, 0);
  lv_obj_remove_style(g_ring, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(g_ring, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(g_ring, 40, LV_PART_MAIN);
  lv_obj_set_style_arc_width(g_ring, 40, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(g_ring, false, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(g_ring, lv_color_hex(COLOR_INK_2),
                             LV_PART_MAIN);
  lv_obj_set_style_arc_color(g_ring, lv_color_hex(COLOR_ORANGE),
                             LV_PART_INDICATOR);

  g_avg = launcher_label(g_ring, "--", &launcher_font_oswald120,
                         COLOR_PAPER);
  lv_obj_align(g_avg, LV_ALIGN_CENTER, 0, -16);
  obj = launcher_caps(g_ring, "AVG FPS", COLOR_MUTED);
  lv_obj_align(obj, LV_ALIGN_CENTER, 0, 70);

  obj = launcher_caps(g_scr, "COREMARK", COLOR_MUTED);
  lv_obj_set_pos(obj, 600, 260);

  g_cm = launcher_label(g_scr, "----", &launcher_font_oswald120,
                        COLOR_PAPER);
  lv_obj_set_pos(g_cm, 596, 300);

  g_cm_unit = launcher_label(g_scr, "iterations / s, 1 core",
                             &launcher_font_outfit26, COLOR_MUTED);
  lv_obj_set_width(g_cm_unit, 410);
  lv_obj_set_pos(g_cm_unit, 600, 440);

  obj = launcher_label(g_scr, "", &launcher_font_outfit26, COLOR_DIM);
  lv_label_set_text_fmt(obj, "Frame rate capped at %d by the %d ms "
                        "refresh period", 1000 / LV_DEF_REFR_PERIOD,
                        LV_DEF_REFR_PERIOD);
  lv_obj_set_pos(obj, 600, 560);
  lv_obj_set_width(obj, 410);

  /* Scene rows */

  list = launcher_box(g_scr, LAUNCHER_CONTENT_W, LV_SIZE_CONTENT, COLOR_INK,
                      0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_pos(list, LAUNCHER_MARGIN, 740);

  for (i = 0; i < (int)nitems(g_scenes); i++)
    {
      struct bench_scene_s *s = &g_scenes[i];
      lv_obj_t *row = launcher_box(list, LAUNCHER_CONTENT_W, 120, COLOR_INK,
                                   0);

      lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
      lv_obj_set_style_border_width(row, 2, 0);
      lv_obj_set_style_border_color(row, lv_color_hex(COLOR_INK_2), 0);

      obj = launcher_label(row, s->name, &launcher_font_outfit34,
                           COLOR_PAPER);
      lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 0, 22);

      s->ms = launcher_label(row, "", &launcher_font_outfit26, COLOR_MUTED);
      lv_obj_align(s->ms, LV_ALIGN_BOTTOM_LEFT, 0, -18);

      s->bar = lv_bar_create(row);
      lv_obj_set_size(s->bar, 220, 14);
      lv_obj_set_style_radius(s->bar, 0, 0);
      lv_obj_set_style_radius(s->bar, 0, LV_PART_INDICATOR);
      lv_obj_set_style_bg_color(s->bar, lv_color_hex(COLOR_INK_2), 0);
      lv_obj_set_style_bg_color(s->bar, lv_color_hex(COLOR_ORANGE),
                                LV_PART_INDICATOR);
      lv_obj_set_style_anim_duration(s->bar, 500, 0);
      lv_obj_align(s->bar, LV_ALIGN_RIGHT_MID, -150, 0);

      s->fps = launcher_label(row, "--", &launcher_font_oswald64,
                              COLOR_PAPER);
      lv_obj_align(s->fps, LV_ALIGN_RIGHT_MID, 0, 0);
    }

  g_btn = launcher_box(g_scr, LAUNCHER_CONTENT_W, 150, COLOR_ORANGE,
                       LAUNCHER_RADIUS);
  lv_obj_add_flag(g_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_transform_width(g_btn, -10, LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(g_btn, -10, LV_STATE_PRESSED);
  lv_obj_add_event_cb(g_btn, bench_run_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_align(g_btn, LV_ALIGN_BOTTOM_MID, 0, -LAUNCHER_MARGIN - 40);

  g_btn_label = launcher_label(g_btn, "RUN BENCHMARK",
                               &launcher_font_oswald64, COLOR_PAPER);
  lv_obj_center(g_btn_label);

  return g_scr;
}
