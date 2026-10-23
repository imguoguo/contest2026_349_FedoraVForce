/****************************************************************************
 * app/touch/touch_main.c
 *
 * Touch test
 *
 * A trail for every finger that fades out after it lifts, live
 * coordinates, the report rate of the touch controller, the number of
 * fingers and a calibration check: tap five targets and see how far off
 * the reported points are.
 *
 * touch reads the touchscreen itself instead of the LVGL driver, which
 * only passes on one point: every sample of the controller is counted and
 * every finger in it drawn, and LVGL gets the first finger for the
 * buttons.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <sys/param.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/input/touchscreen.h>

#include <lvgl/lvgl.h>

#include "lvx_ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TOUCH_AREA_Y          420     /* Trails below the stats */
#define TOUCH_AREA_BOTTOM     1620    /* and above the button */
#define TOUCH_STAT_Y          263
#define TOUCH_STAT_W          ((UI_CONTENT_W - 3 * UI_GAP) / 4)
#define TOUCH_LINE_W          14
#define TOUCH_LINE_POINTS     32      /* Per line object, then a new one */
#define TOUCH_DOT_SIZE        96
#define TOUCH_FADE_DELAY      300
#define TOUCH_FADE_TIME       900

#define TOUCH_MAX_POINTS      10      /* Room in a sample */
#define TOUCH_IDS             16      /* GT9XX track IDs are 0 to 15 */

#define TOUCH_TARGET_INSET    120
#define TOUCH_TARGET_SIZE     120
#define TOUCH_TARGETS         5
#define TOUCH_CAL_ROW_Y       820
#define TOUCH_CAL_ROW_H       120

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct touch_line_s
{
  lv_point_precise_t points[TOUCH_LINE_POINTS];
  uint32_t npoints;
  int id;                   /* Finger */
};

struct touch_finger_s
{
  lv_obj_t *line;           /* Line being drawn, NULL when up */
  lv_obj_t *dot;
  bool down;
  bool drawing;             /* Press started in the trail area */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* One color per finger */

static const uint32_t g_colors[] =
{
  UI_ORANGE, UI_BLUE, UI_YELLOW, UI_PAPER, 0x3ecf8e
};

static int g_fd;
static uint64_t g_sample[(SIZEOF_TOUCH_SAMPLE_S(TOUCH_MAX_POINTS) + 7) / 8];
static struct touch_finger_s g_fingers[TOUCH_IDS];
static lv_point_t g_point;          /* First finger, given to LVGL */
static int g_down;                  /* Fingers on the panel */

static lv_obj_t *g_scr;
static lv_obj_t *g_stat[4];         /* X, Y, rate, points */
static lv_obj_t *g_cal;             /* Calibration overlay */
static lv_obj_t *g_cal_target;
static lv_obj_t *g_cal_text;

static uint32_t g_reports;
static int g_target;
static lv_point_t g_offset[TOUCH_TARGETS];
static bool g_running;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static lv_point_t touch_target(int i)
{
  lv_point_t p;

  if (i == TOUCH_TARGETS - 1)
    {
      p.x = UI_SCREEN_W / 2;
      p.y = UI_SCREEN_H / 2;
    }
  else
    {
      p.x = i % 2 ? UI_SCREEN_W - TOUCH_TARGET_INSET : TOUCH_TARGET_INSET;
      p.y = i / 2 ? UI_SCREEN_H - TOUCH_TARGET_INSET : TOUCH_TARGET_INSET;
    }

  return p;
}

static uint32_t touch_color(int id)
{
  return g_colors[id % nitems(g_colors)];
}

/****************************************************************************
 * Name: trails
 ****************************************************************************/

static void touch_line_delete_cb(lv_event_t *e)
{
  lv_free(lv_event_get_user_data(e));
}

static lv_obj_t *touch_line_create(int id)
{
  struct touch_line_s *pts = lv_malloc(sizeof(struct touch_line_s));
  lv_obj_t *line;

  if (pts == NULL)
    {
      return NULL;
    }

  pts->npoints = 0;
  pts->id = id;
  line = lv_line_create(g_scr);
  lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_line_width(line, TOUCH_LINE_W, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
  lv_obj_set_style_line_color(line, lv_color_hex(touch_color(id)), 0);
  lv_obj_set_user_data(line, pts);
  lv_obj_add_event_cb(line, touch_line_delete_cb, LV_EVENT_DELETE, pts);
  lv_obj_move_to_index(line, lv_obj_get_index(g_fingers[0].dot));
  return line;
}

static void touch_line_add(struct touch_finger_s *f, int id, int32_t x,
                           int32_t y)
{
  struct touch_line_s *pts;
  lv_point_precise_t last;

  if (f->line == NULL)
    {
      return;
    }

  /* A full line ends here, the next one goes on from its last point */

  pts = lv_obj_get_user_data(f->line);
  if (pts->npoints == TOUCH_LINE_POINTS)
    {
      last = pts->points[TOUCH_LINE_POINTS - 1];
      f->line = touch_line_create(id);
      if (f->line == NULL)
        {
          return;
        }

      pts = lv_obj_get_user_data(f->line);
      pts->points[pts->npoints++] = last;
    }

  pts->points[pts->npoints].x = x;
  pts->points[pts->npoints].y = y;
  pts->npoints++;
  lv_line_set_points(f->line, pts->points, pts->npoints);
}

static void touch_fade_cb(void *obj, int32_t v)
{
  lv_obj_set_style_opa(obj, v, 0);
}

static void touch_faded_cb(lv_anim_t *a)
{
  lv_obj_delete_async(a->var);
}

/* Fade out every line of the stroke of finger id that just ended */

static void touch_stroke_end(int id)
{
  struct touch_line_s *pts;
  lv_obj_t *obj;
  uint32_t i;

  for (i = 0; i < lv_obj_get_child_count(g_scr); i++)
    {
      obj = lv_obj_get_child(g_scr, i);
      if (!lv_obj_check_type(obj, &lv_line_class) ||
          lv_obj_has_flag(obj, LV_OBJ_FLAG_USER_1))
        {
          continue;
        }

      pts = lv_obj_get_user_data(obj);
      if (pts->id == id)
        {
          lv_obj_add_flag(obj, LV_OBJ_FLAG_USER_1);
          ui_anim(obj, touch_fade_cb, LV_OPA_COVER, LV_OPA_TRANSP,
                  TOUCH_FADE_TIME, TOUCH_FADE_DELAY, touch_faded_cb);
        }
    }

  g_fingers[id].line = NULL;
}

static void touch_finger(int id, int32_t x, int32_t y, bool down)
{
  struct touch_finger_s *f = &g_fingers[id];

  if (g_cal == NULL && down && !f->down)
    {
      f->drawing = y >= TOUCH_AREA_Y && y < TOUCH_AREA_BOTTOM;
      if (f->drawing)
        {
          f->line = touch_line_create(id);
        }
    }

  f->down = down;

  if (f->drawing && down)
    {
      touch_line_add(f, id, x, y);
      lv_obj_set_pos(f->dot, x - TOUCH_DOT_SIZE / 2, y - TOUCH_DOT_SIZE / 2);
      lv_obj_remove_flag(f->dot, LV_OBJ_FLAG_HIDDEN);
    }
  else if (f->drawing)
    {
      lv_obj_add_flag(f->dot, LV_OBJ_FLAG_HIDDEN);
      touch_stroke_end(id);
      f->drawing = false;
    }
}

/* Replaces the read callback of the LVGL touchscreen.  One sample per
 * call, with continue_reading while there are more, so that LVGL sees
 * every press and release.
 */

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
  FAR struct touch_sample_s *sample = (FAR struct touch_sample_s *)g_sample;
  FAR struct touch_point_s *p;
  int32_t x;
  int32_t y;
  bool down;
  int n = 0;
  int i;

  UNUSED(indev);

  if (read(g_fd, g_sample, sizeof(g_sample)) >=
      (ssize_t)SIZEOF_TOUCH_SAMPLE_S(1))
    {
      g_reports++;
      for (i = 0; i < MIN(sample->npoints, TOUCH_MAX_POINTS); i++)
        {
          p = &sample->point[i];
          down = (p->flags & (TOUCH_DOWN | TOUCH_MOVE)) != 0;
          x = LV_CLAMP(0, p->x, UI_SCREEN_W - 1);
          y = LV_CLAMP(0, p->y, UI_SCREEN_H - 1);
          if (p->id < TOUCH_IDS)
            {
              touch_finger(p->id, x, y, down);
            }

          if (down && n++ == 0)
            {
              g_point.x = x;
              g_point.y = y;
            }
        }

      g_down = n;
      if (n > 0)
        {
          lv_label_set_text_fmt(g_stat[0], "%d", (int)g_point.x);
          lv_label_set_text_fmt(g_stat[1], "%d", (int)g_point.y);
        }

      lv_label_set_text_fmt(g_stat[3], "%d", n);
      data->continue_reading = true;
    }

  data->point = g_point;
  data->state = g_down > 0 ? LV_INDEV_STATE_PRESSED :
                             LV_INDEV_STATE_RELEASED;
}

static void touch_rate_cb(lv_timer_t *timer)
{
  UNUSED(timer);
  lv_label_set_text_fmt(g_stat[2], "%u", (unsigned int)g_reports);
  g_reports = 0;
}

/****************************************************************************
 * Name: calibration check
 ****************************************************************************/

static void touch_cal_show_target(void)
{
  lv_point_t p = touch_target(g_target);

  lv_obj_set_pos(g_cal_target, p.x - TOUCH_TARGET_SIZE / 2,
                 p.y - TOUCH_TARGET_SIZE / 2);
  lv_label_set_text_fmt(g_cal_text, "Tap the center of the target  "
                        "%d / %d", g_target + 1, TOUCH_TARGETS);
}

static void touch_cal_result(void)
{
  static const char * const names[TOUCH_TARGETS] =
  {
    "Top left", "Top right", "Bottom left", "Bottom right", "Center"
  };

  lv_obj_t *obj;
  int32_t sum_x = 0;
  int32_t sum_y = 0;
  int32_t y;
  float sum = 0.0f;
  float max = 0.0f;
  float err;
  int i;

  lv_obj_add_flag(g_cal_target, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(g_cal_text, "Tap anywhere to close");
  lv_obj_align(g_cal_text, LV_ALIGN_TOP_MID, 0, 1760);

  /* One row per target: where it was missed and by how much */

  for (i = 0; i < TOUCH_TARGETS; i++)
    {
      err = sqrtf(g_offset[i].x * g_offset[i].x +
                  g_offset[i].y * g_offset[i].y);
      sum += err;
      max = MAX(max, err);
      sum_x += g_offset[i].x;
      sum_y += g_offset[i].y;

      y = TOUCH_CAL_ROW_Y + i * TOUCH_CAL_ROW_H;
      obj = ui_box(g_cal, UI_INK_2, 0);
      lv_obj_set_pos(obj, UI_MARGIN, y + TOUCH_CAL_ROW_H - 2);
      lv_obj_set_size(obj, UI_CONTENT_W, 2);
      ui_place(ui_label(g_cal, UI_OUTFIT_34, 0xffffff, names[i]),
               UI_OUTFIT_34, UI_MARGIN, y + 26);
      obj = ui_label(g_cal, UI_OUTFIT_26, UI_MUTED, "");
      lv_label_set_text_fmt(obj, "x %+d, y %+d", (int)g_offset[i].x,
                            (int)g_offset[i].y);
      ui_place(obj, UI_OUTFIT_26, UI_MARGIN, y + 76);
      obj = ui_label(g_cal, UI_OSWALD_64, 0xffffff, "");
      lv_label_set_text_fmt(obj, "%d PX", (int)(err + 0.5f));
      lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -UI_MARGIN,
                   y + 30 - ui_ink_top(UI_OSWALD_64));
    }

  ui_place(ui_tag(g_cal, UI_MUTED, "MEAN ERROR, PX"), UI_OUTFIT_26,
           UI_MARGIN, 263);
  obj = ui_label(g_cal, UI_OSWALD_300, UI_ORANGE, "");
  lv_label_set_text_fmt(obj, "%d", (int)(sum / TOUCH_TARGETS + 0.5f));
  ui_place(obj, UI_OSWALD_300, UI_MARGIN, 330);
  obj = ui_label(g_cal, UI_OUTFIT_34, UI_MUTED, "");
  lv_label_set_text_fmt(obj, "Largest %d px\nMean offset x %+d, y %+d px",
                        (int)(max + 0.5f), (int)(sum_x / TOUCH_TARGETS),
                        (int)(sum_y / TOUCH_TARGETS));
  ui_place(obj, UI_OUTFIT_34, 520, 450);

  printf("touch: mean error %d px, largest %d px, offset %+d, %+d\n",
         (int)(sum / TOUCH_TARGETS + 0.5f), (int)(max + 0.5f),
         (int)(sum_x / TOUCH_TARGETS), (int)(sum_y / TOUCH_TARGETS));
}

static void touch_cal_press_cb(lv_event_t *e)
{
  lv_point_t p;
  lv_point_t t;

  UNUSED(e);

  /* The press after the result closes it */

  if (g_target >= TOUCH_TARGETS)
    {
      lv_obj_delete_async(g_cal);
      g_cal = NULL;
      return;
    }

  lv_indev_get_point(lv_indev_active(), &p);
  t = touch_target(g_target);
  g_offset[g_target].x = p.x - t.x;
  g_offset[g_target].y = p.y - t.y;
  printf("touch: target %d at %d, %d, tapped %d, %d\n", g_target + 1,
         (int)t.x, (int)t.y, (int)p.x, (int)p.y);

  if (++g_target < TOUCH_TARGETS)
    {
      touch_cal_show_target();
    }
  else
    {
      touch_cal_result();
    }
}

/* Cross hair in a ring */

static lv_obj_t *touch_cal_target(lv_obj_t *parent)
{
  lv_obj_t *target = ui_box(parent, 0, LV_RADIUS_CIRCLE);
  lv_obj_t *obj;

  lv_obj_set_size(target, TOUCH_TARGET_SIZE, TOUCH_TARGET_SIZE);
  lv_obj_set_style_bg_opa(target, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(target, 4, 0);
  lv_obj_set_style_border_color(target, lv_color_hex(UI_ORANGE), 0);

  obj = ui_box(target, 0xffffff, 0);
  lv_obj_set_size(obj, TOUCH_TARGET_SIZE, 2);
  lv_obj_center(obj);
  obj = ui_box(target, 0xffffff, 0);
  lv_obj_set_size(obj, 2, TOUCH_TARGET_SIZE);
  lv_obj_center(obj);
  return target;
}

static void touch_cal_open_cb(lv_event_t *e)
{
  UNUSED(e);

  g_cal = ui_box(g_scr, UI_INK, 0);
  lv_obj_set_size(g_cal, UI_SCREEN_W, UI_SCREEN_H);
  lv_obj_add_flag(g_cal, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_cal, touch_cal_press_cb, LV_EVENT_PRESSED, NULL);

  g_cal_text = ui_label(g_cal, UI_OUTFIT_34, 0xffffff, "");
  lv_obj_set_width(g_cal_text, UI_CONTENT_W);
  lv_obj_set_style_text_align(g_cal_text, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(g_cal_text, LV_ALIGN_CENTER, 0, 300);

  g_cal_target = touch_cal_target(g_cal);
  g_target = 0;
  touch_cal_show_target();
}

/****************************************************************************
 * Name: page
 ****************************************************************************/

static void touch_home_cb(lv_event_t *e)
{
  UNUSED(e);
  g_running = false;
}

static void touch_create(void)
{
  static const char * const names[] =
  {
    "X", "Y", "REPORTS / S", "FINGERS"
  };

  lv_obj_t *obj;
  uint8_t max;
  int32_t x;
  int i;

  g_scr = lv_obj_create(NULL);
  lv_obj_remove_style_all(g_scr);
  lv_obj_remove_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(g_scr, lv_color_hex(UI_INK), 0);
  lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);

  lv_obj_set_pos(ui_back(g_scr, UI_INK_2, touch_home_cb), UI_MARGIN, 56);
  ui_place(ui_label(g_scr, UI_OSWALD_120, 0xffffff, "TOUCH"), UI_OSWALD_120,
           222, 50);

  /* Live values in four columns */

  for (i = 0; i < nitems(g_stat); i++)
    {
      x = UI_MARGIN + i * (TOUCH_STAT_W + UI_GAP);
      ui_place(ui_tag(g_scr, UI_MUTED, names[i]), UI_OUTFIT_26, x,
               TOUCH_STAT_Y);
      g_stat[i] = ui_label(g_scr, UI_OSWALD_64, 0xffffff, "--");
      ui_place(g_stat[i], UI_OSWALD_64, x, TOUCH_STAT_Y + 52);
    }

  lv_label_set_text(g_stat[3], "0");

  /* The trail area */

  obj = ui_box(g_scr, UI_INK_2, 0);
  lv_obj_set_pos(obj, UI_MARGIN, TOUCH_AREA_Y - 2);
  lv_obj_set_size(obj, UI_CONTENT_W, 2);
  obj = ui_label(g_scr, UI_OUTFIT_26, UI_DIM, "Draw anywhere here");
  if (ioctl(g_fd, TSIOC_GETMAXPOINTS, (unsigned long)(uintptr_t)&max) >= 0)
    {
      lv_label_set_text_fmt(obj, "Draw here with up to %d fingers", max);
    }

  lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, TOUCH_AREA_Y + 40);

  for (i = 0; i < TOUCH_IDS; i++)
    {
      obj = ui_box(g_scr, 0, LV_RADIUS_CIRCLE);
      lv_obj_set_size(obj, TOUCH_DOT_SIZE, TOUCH_DOT_SIZE);
      lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(obj, 6, 0);
      lv_obj_set_style_border_color(obj, lv_color_hex(touch_color(i)), 0);
      lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
      g_fingers[i].dot = obj;
    }

  obj = ui_box(g_scr, UI_ORANGE, UI_RADIUS);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(UI_ORANGE),
                                                 LV_OPA_20),
                            LV_STATE_PRESSED);
  lv_obj_add_event_cb(obj, touch_cal_open_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(obj, UI_MARGIN, 1658);
  lv_obj_set_size(obj, UI_CONTENT_W, 150);
  lv_obj_center(ui_label(obj, UI_OSWALD_64, 0xffffff, "CALIBRATION CHECK"));

  lv_screen_load(g_scr);
}

static int touch_start(lv_nuttx_result_t *result)
{
  lv_nuttx_dsc_t info;

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, result);

  if (result->disp == NULL || result->indev == NULL)
    {
      fprintf(stderr, "touch: no display or touchscreen\n");
      return -1;
    }

  ui_hide_perf();

  g_cal = NULL;
  g_reports = 0;
  g_down = 0;
  memset(g_fingers, 0, sizeof(g_fingers));

  /* The NuttX touchscreen of LVGL keeps the file descriptor first in its
   * driver data
   */

  g_fd = *(FAR int *)lv_indev_get_driver_data(result->indev);
  lv_indev_set_read_cb(result->indev, touch_read_cb);

  touch_create();
  lv_timer_create(touch_rate_cb, 1000, NULL);
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_result_t result;
  uint32_t idle;

  UNUSED(argc);
  UNUSED(argv);

  /* LVGL is one global instance, shared by every task of the flat build */

  if (lv_is_initialized())
    {
      fprintf(stderr, "touch: LVGL is busy, start it from the launcher\n");
      return 1;
    }

  g_running = touch_start(&result) == 0;
  while (g_running)
    {
      idle = lv_timer_handler();
      usleep(MAX(idle, 1) * 1000);
    }

  lv_nuttx_deinit(&result);
  lv_deinit();
  return 0;
}
