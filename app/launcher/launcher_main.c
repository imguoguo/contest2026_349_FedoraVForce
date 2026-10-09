/****************************************************************************
 * app/launcher/launcher_main.c
 *
 * Contest 2026 team 349 (FedoraVForce) LVGL launcher demo
 * VSAI LogicPi A1 (Amlogic S6): MIPI DSI 1080x1920 + GT9XX touch
 *
 * Light "editorial" look: condensed numerals and flat colour blocks.
 * Animations are kept to small regions (rolling digits, count-ups, short
 * slide-ins) because the board renders with one CPU core in software.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "launcher.h"
#include "launcher_ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UNLOCK_DRAG_DIST      240
#define PAINT_BRUSH_RADIUS    8
#define PAINT_STROKE_STEP     4
#define PAINT_BG              0xfafaf7
#define PAINT_IDLE_MS         700
#define PAINT_MAX_POINTS      4096
#define PREVIEW_SCALE         6
#define PREVIEW_SIDE          (LAUNCHER_DIGIT_SIDE * PREVIEW_SCALE)
#define CANVAS_BUF_SIZE       (LAUNCHER_SCREEN_W * LAUNCHER_SCREEN_H * \
                               sizeof(uint16_t))
#define ROLL_MAX              5
#define TOUCH_DEVPATH         "/dev/input0"

/* The paint code indexes the canvas buffer as width * 2 bytes per row */

#if LV_DRAW_BUF_STRIDE_ALIGN != 1
#  error "launcher paint expects LV_DRAW_BUF_STRIDE_ALIGN == 1"
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* A row of digits where each changed digit slides up into place */

struct roll_s
{
  lv_obj_t *cell[ROLL_MAX];
  int n;
  int32_t h;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static lv_obj_t *g_lock_scr;
static lv_obj_t *g_home_scr;
static lv_obj_t *g_about_scr;
static lv_obj_t *g_paint_scr;
static lv_obj_t *g_bench_scr;

static struct roll_s g_lock_hh;
static struct roll_s g_lock_mm;
static struct roll_s g_lock_ss;
static struct roll_s g_home_hhmm;
static struct roll_s g_home_ss;
static struct roll_s g_result_digit;

static lv_obj_t *g_unlock_block;
static lv_point_t g_drag_start;
static int32_t g_drag_moved;

static lv_obj_t *g_home_fps;
static lv_obj_t *g_home_blocks[8];
static int g_home_nblocks;
static lv_obj_t *g_val_heap;
static lv_obj_t *g_val_fps;
static lv_obj_t *g_val_uptime;

static lv_obj_t *g_about_rule;
static lv_obj_t *g_about_rows[5];
static lv_obj_t *g_about_heap;
static lv_obj_t *g_about_bar;
static lv_obj_t *g_about_fps;

static lv_obj_t *g_canvas;
static uint16_t *g_canvas_buf;
static uint16_t g_brush_color;
static lv_obj_t *g_brush_btn;
static lv_point_t g_brush_prev;
static lv_obj_t *g_paint_hint;
static lv_timer_t *g_paint_timer;
static struct launcher_pt_s g_pts[PAINT_MAX_POINTS];
static int g_npts;

static lv_obj_t *g_result;
static lv_obj_t *g_result_conf;
static lv_obj_t *g_result_rank[3];
static lv_obj_t *g_result_bar[3];
static lv_obj_t *g_result_pct[3];
static lv_obj_t *g_preview;
static uint16_t g_preview_buf[PREVIEW_SIDE * PREVIEW_SIDE]
  aligned_data(LV_DRAW_BUF_ALIGN);
static bool g_recognized;
static lv_obj_t *g_unlock_arrow;

static const uint32_t g_palette[] =
{
  0x101317, 0xff5b1f, 0x2d5bff, 0xffc21a
};

static uint32_t g_refr_count;
static uint32_t g_fps;
static uint32_t g_heap_free_mb;
static struct timespec g_refr_start;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t launcher_uptime_sec(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)ts.tv_sec;
}

static void launcher_switch_scr(lv_obj_t *scr, lv_screen_load_anim_t anim)
{
  lv_screen_load_anim(scr, anim, 250, 0, false);
}

static void launcher_back_event_cb(lv_event_t *e)
{
  UNUSED(e);
  launcher_switch_scr(g_home_scr, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
}

static lv_obj_t *launcher_screen(uint32_t color)
{
  lv_obj_t *scr = lv_obj_create(NULL);

  lv_obj_set_style_bg_color(scr, lv_color_hex(color), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

static void launcher_top_bar(lv_obj_t *scr, lv_obj_t **right,
                             const char *text)
{
  lv_obj_t *obj;

  obj = launcher_caps(scr, "FEDORA-V FORCE  /  349", COLOR_DIM);
  lv_obj_align(obj, LV_ALIGN_TOP_LEFT, LAUNCHER_MARGIN, 64);

  obj = launcher_caps(scr, text, COLOR_INK);
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -LAUNCHER_MARGIN, 64);
  if (right != NULL)
    {
      *right = obj;
    }
}

static void launcher_anim_y_cb(void *obj, int32_t v)
{
  lv_obj_set_style_translate_y(obj, v, 0);
}

static void launcher_anim_x_cb(void *obj, int32_t v)
{
  lv_obj_set_style_translate_x(obj, v, 0);
}

static void launcher_anim_width_cb(void *obj, int32_t v)
{
  lv_obj_set_width(obj, v);
}

/* Shrink slightly while pressed: changes only the drawn area, no layer */

static void launcher_press_feedback(lv_obj_t *obj)
{
  lv_obj_set_style_transform_width(obj, -10, LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(obj, -10, LV_STATE_PRESSED);
}

/****************************************************************************
 * Name: rolling digits
 ****************************************************************************/

static void roll_exec_cb(void *cell, int32_t v)
{
  lv_obj_t *cur = lv_obj_get_child(cell, 0);
  lv_obj_t *nxt = lv_obj_get_child(cell, 1);
  int32_t h = lv_obj_get_height(cell);

  lv_obj_set_y(cur, -v);
  lv_obj_set_y(nxt, h - v);
}

static void roll_ready_cb(lv_anim_t *a)
{
  lv_obj_t *cell = a->var;
  lv_obj_t *cur = lv_obj_get_child(cell, 0);
  lv_obj_t *nxt = lv_obj_get_child(cell, 1);

  lv_label_set_text(cur, lv_label_get_text(nxt));
  lv_obj_set_y(cur, 0);
  lv_obj_set_y(nxt, lv_obj_get_height(cell));
}

static lv_obj_t *roll_create(struct roll_s *r, lv_obj_t *parent,
                             const char *pattern, const lv_font_t *font,
                             uint32_t color)
{
  lv_obj_t *row = launcher_box(parent, LV_SIZE_CONTENT, LV_SIZE_CONTENT,
                               0, 0);
  int32_t w = 0;
  char ch;
  int i;

  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);

  for (ch = '0'; ch <= '9'; ch++)
    {
      w = LV_MAX(w, (int32_t)lv_font_get_glyph_width(font, ch, 0));
    }

  r->n = 0;
  r->h = lv_font_get_line_height(font);

  for (i = 0; pattern[i] != '\0'; i++)
    {
      lv_obj_t *cell;
      lv_obj_t *obj;

      if (pattern[i] != '0')
        {
          char sep[2] =
            {
              pattern[i], '\0'
            };

          launcher_label(row, sep, font, color);
          continue;
        }

      cell = launcher_box(row, w, r->h, 0, 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);

      obj = launcher_label(cell, "0", font, color);
      lv_obj_set_width(obj, w);
      lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);

      obj = launcher_label(cell, "0", font, color);
      lv_obj_set_width(obj, w);
      lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_y(obj, r->h);

      r->cell[r->n++] = cell;
    }

  return row;
}

static void roll_set(struct roll_s *r, const char *digits)
{
  lv_anim_t a;
  int i;

  for (i = 0; i < r->n && digits[i] != '\0'; i++)
    {
      lv_obj_t *cell = r->cell[i];
      lv_obj_t *cur = lv_obj_get_child(cell, 0);
      lv_obj_t *nxt = lv_obj_get_child(cell, 1);
      bool rolling = lv_anim_get(cell, roll_exec_cb) != NULL;
      char ch[2] =
        {
          digits[i], '\0'
        };

      if (rolling ? strcmp(lv_label_get_text(nxt), ch) == 0 :
                    strcmp(lv_label_get_text(cur), ch) == 0)
        {
          continue;
        }

      /* Finish an unfinished roll at once before starting the next */

      if (rolling)
        {
          lv_anim_delete(cell, roll_exec_cb);
          lv_label_set_text(cur, lv_label_get_text(nxt));
          lv_obj_set_y(cur, 0);
          lv_obj_set_y(nxt, r->h);
        }

      lv_label_set_text(nxt, ch);

      lv_anim_init(&a);
      lv_anim_set_var(&a, cell);
      lv_anim_set_exec_cb(&a, roll_exec_cb);
      lv_anim_set_values(&a, 0, r->h);
      lv_anim_set_duration(&a, 320);
      lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
      lv_anim_set_ready_cb(&a, roll_ready_cb);
      lv_anim_start(&a);
    }
}

/****************************************************************************
 * Name: lock screen
 ****************************************************************************/

static void launcher_unlock(void)
{
  /* Nothing returns to the lock screen, so stop its looping hint */

  lv_anim_delete(g_unlock_arrow, NULL);
  launcher_switch_scr(g_home_scr, LV_SCR_LOAD_ANIM_MOVE_TOP);
}

/* Swipe up anywhere outside the orange block */

static void launcher_lock_swipe_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_active();
  lv_point_t p;

  if (indev == NULL)
    {
      return;
    }

  lv_indev_get_point(indev, &p);

  if (code == LV_EVENT_PRESSED)
    {
      g_drag_start = p;
    }
  else if (code == LV_EVENT_PRESSING &&
           g_drag_start.y - p.y > UNLOCK_DRAG_DIST)
    {
      lv_indev_wait_release(indev);
      launcher_unlock();
    }
}

/* The orange block follows the finger; release past the line or tap */

static void launcher_unlock_drag_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_active();
  lv_anim_t a;
  lv_point_t p;

  if (indev == NULL)
    {
      return;
    }

  lv_indev_get_point(indev, &p);

  if (code == LV_EVENT_PRESSED)
    {
      g_drag_start = p;
      g_drag_moved = 0;
      lv_anim_delete(g_unlock_block, launcher_anim_y_cb);
    }
  else if (code == LV_EVENT_PRESSING)
    {
      g_drag_moved = LV_MAX(0, g_drag_start.y - p.y);
      lv_obj_set_style_translate_y(g_unlock_block, -g_drag_moved, 0);
    }
  else if (g_drag_moved > UNLOCK_DRAG_DIST || g_drag_moved < 10)
    {
      lv_obj_set_style_translate_y(g_unlock_block, 0, 0);
      launcher_unlock();
    }
  else
    {
      lv_anim_init(&a);
      lv_anim_set_var(&a, g_unlock_block);
      lv_anim_set_exec_cb(&a, launcher_anim_y_cb);
      lv_anim_set_values(&a, -g_drag_moved, 0);
      lv_anim_set_duration(&a, 260);
      lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
      lv_anim_start(&a);
    }
}

static void launcher_create_lock_scr(void)
{
  lv_obj_t *scr = launcher_screen(COLOR_BG);
  lv_obj_t *obj;
  lv_anim_t a;

  lv_obj_add_event_cb(scr, launcher_lock_swipe_event_cb, LV_EVENT_PRESSED,
                      NULL);
  lv_obj_add_event_cb(scr, launcher_lock_swipe_event_cb, LV_EVENT_PRESSING,
                      NULL);

  launcher_top_bar(scr, NULL, "OPENVELA");

  /* Hours stacked over minutes, seconds in the margin */

  obj = roll_create(&g_lock_hh, scr, "00", &launcher_font_clock300,
                    COLOR_INK);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN - 8, 200);

  obj = roll_create(&g_lock_mm, scr, "00", &launcher_font_clock300,
                    COLOR_INK);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN - 8, 490);

  obj = launcher_caps(scr, "UPTIME", COLOR_DIM);
  lv_obj_set_pos(obj, 428, 560);

  obj = roll_create(&g_lock_ss, scr, "00", &launcher_font_oswald120,
                    COLOR_ORANGE);
  lv_obj_set_pos(obj, 420, 610);

  obj = launcher_box(scr, LAUNCHER_CONTENT_W, 6, COLOR_INK, 0);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN, 880);

  obj = launcher_label(scr, "openvela on\nLogicPi A1",
                       &launcher_font_outfit52, COLOR_INK);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN, 930);

  obj = launcher_label(scr, "NuttX\nAmlogic S6\n4x Cortex-A510",
                       &launcher_font_outfit26, COLOR_DIM);
  lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_line_space(obj, 8, 0);
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -LAUNCHER_MARGIN, 936);

  /* Unlock block */

  g_unlock_block = launcher_box(scr, LAUNCHER_SCREEN_W, 600, COLOR_ORANGE,
                                0);
  lv_obj_add_flag(g_unlock_block, LV_OBJ_FLAG_CLICKABLE |
                                  LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_align(g_unlock_block, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(g_unlock_block, launcher_unlock_drag_cb,
                      LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(g_unlock_block, launcher_unlock_drag_cb,
                      LV_EVENT_PRESSING, NULL);
  lv_obj_add_event_cb(g_unlock_block, launcher_unlock_drag_cb,
                      LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(g_unlock_block, launcher_unlock_drag_cb,
                      LV_EVENT_PRESS_LOST, NULL);

  obj = launcher_label(g_unlock_block, "SWIPE UP", &launcher_font_oswald120,
                       COLOR_PAPER);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN, 120);

  obj = launcher_label(g_unlock_block, "or tap here to unlock",
                       &launcher_font_outfit34, COLOR_PAPER);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN, 280);

  g_unlock_arrow = launcher_label(g_unlock_block, LV_SYMBOL_UP,
                                  &lv_font_montserrat_48, COLOR_PAPER);
  lv_obj_align(g_unlock_arrow, LV_ALIGN_TOP_RIGHT, -LAUNCHER_MARGIN - 20,
               160);

  lv_anim_init(&a);
  lv_anim_set_var(&a, g_unlock_arrow);
  lv_anim_set_exec_cb(&a, launcher_anim_y_cb);
  lv_anim_set_values(&a, 0, -40);
  lv_anim_set_duration(&a, 800);
  lv_anim_set_playback_duration(&a, 800);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);

  g_lock_scr = scr;
}

/****************************************************************************
 * Name: about screen
 ****************************************************************************/

static lv_obj_t *launcher_about_row(lv_obj_t *list, const char *key,
                                    const char *val)
{
  lv_obj_t *row = launcher_box(list, LAUNCHER_CONTENT_W, 104, COLOR_BG, 0);
  lv_obj_t *obj;

  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(row, 2, 0);
  lv_obj_set_style_border_color(row, lv_color_hex(COLOR_RULE), 0);

  obj = launcher_caps(row, key, COLOR_DIM);
  lv_obj_align(obj, LV_ALIGN_LEFT_MID, 0, 0);

  obj = launcher_label(row, val, &launcher_font_outfit34, COLOR_INK);
  lv_obj_align(obj, LV_ALIGN_RIGHT_MID, 0, 0);
  return row;
}

static void launcher_about_enter_cb(lv_event_t *e)
{
  struct mallinfo mi = mallinfo();
  lv_anim_t a;
  int i;

  UNUSED(e);

  lv_anim_init(&a);
  lv_anim_set_var(&a, g_about_rule);
  lv_anim_set_exec_cb(&a, launcher_anim_width_cb);
  lv_anim_set_values(&a, 0, LAUNCHER_CONTENT_W);
  lv_anim_set_duration(&a, 500);
  lv_anim_set_delay(&a, 150);
  lv_anim_set_early_apply(&a, true);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);

  for (i = 0; i < (int)nitems(g_about_rows); i++)
    {
      launcher_slide_in(g_about_rows[i], -60, 0, 120 + i * 60);
    }

  lv_bar_set_value(g_about_bar, 0, LV_ANIM_OFF);
  lv_bar_set_value(g_about_bar, mi.arena >= 100 ?
                   100 - mi.fordblks / (mi.arena / 100) : 0, LV_ANIM_ON);
  launcher_count_up(g_about_fps, "%d", g_fps, 400);
}

static void launcher_create_about_scr(void)
{
  lv_obj_t *scr = launcher_screen(COLOR_BG);
  lv_obj_t *list;
  lv_obj_t *card;
  lv_obj_t *obj;

  lv_obj_add_event_cb(scr, launcher_about_enter_cb,
                      LV_EVENT_SCREEN_LOAD_START, NULL);

  obj = launcher_square_btn(scr, LV_SYMBOL_LEFT, 112, COLOR_INK,
                            COLOR_PAPER, launcher_back_event_cb);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN, 56);

  obj = launcher_caps(scr, "PORT DETAILS", COLOR_DIM);
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -LAUNCHER_MARGIN, 96);

  obj = launcher_label(scr, "ABOUT", &launcher_font_oswald120, COLOR_INK);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN - 4, 200);

  obj = launcher_label(scr, "VSAI LogicPi A1", &launcher_font_outfit52,
                       COLOR_INK);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN, 350);

  obj = launcher_label(scr, "openvela / NuttX, ported by team 349",
                       &launcher_font_outfit26, COLOR_DIM);
  lv_obj_set_pos(obj, LAUNCHER_MARGIN, 424);

  g_about_rule = launcher_box(scr, LAUNCHER_CONTENT_W, 6, COLOR_INK, 0);
  lv_obj_set_pos(g_about_rule, LAUNCHER_MARGIN, 500);

  list = launcher_box(scr, LAUNCHER_CONTENT_W, LV_SIZE_CONTENT, COLOR_BG, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_pos(list, LAUNCHER_MARGIN, 512);

  g_about_rows[0] = launcher_about_row(list, "SOC", "Amlogic S6 (ARM64)");
  g_about_rows[1] = launcher_about_row(list, "INTERRUPT", "GICv3");
  g_about_rows[2] = launcher_about_row(list, "DISPLAY",
                                       "MIPI DSI 1080x1920");
  g_about_rows[3] = launcher_about_row(list, "FORMAT", "RGB565");
  g_about_rows[4] = launcher_about_row(list, "TOUCH", "GT9XX over I2C");

  /* Live block */

  card = launcher_box(scr, LAUNCHER_CONTENT_W, 360, COLOR_BLUE,
                      LAUNCHER_RADIUS);
  lv_obj_set_pos(card, LAUNCHER_MARGIN, 1110);

  obj = launcher_caps(card, "LIVE", COLOR_PAPER);
  lv_obj_set_pos(obj, 48, 48);

  obj = launcher_caps(card, "FRAMES / S", COLOR_PAPER);
  lv_obj_set_pos(obj, 48, 100);

  g_about_fps = launcher_label(card, "0", &launcher_font_oswald120,
                               COLOR_PAPER);
  lv_obj_align(g_about_fps, LV_ALIGN_TOP_RIGHT, -48, 20);

  g_about_heap = launcher_label(card, "heap -", &launcher_font_outfit34,
                                COLOR_PAPER);
  lv_obj_align(g_about_heap, LV_ALIGN_BOTTOM_LEFT, 48, -100);

  g_about_bar = lv_bar_create(card);
  lv_obj_set_size(g_about_bar, LAUNCHER_CONTENT_W - 96, 20);
  lv_obj_set_style_radius(g_about_bar, 0, 0);
  lv_obj_set_style_radius(g_about_bar, 0, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(g_about_bar, lv_color_hex(COLOR_BLUE_DIM), 0);
  lv_obj_set_style_bg_color(g_about_bar, lv_color_hex(COLOR_PAPER),
                            LV_PART_INDICATOR);
  lv_obj_set_style_anim_duration(g_about_bar, 900, 0);
  lv_obj_align(g_about_bar, LV_ALIGN_BOTTOM_LEFT, 48, -52);

  card = launcher_box(scr, LAUNCHER_CONTENT_W, 136, COLOR_INK,
                      LAUNCHER_RADIUS);
  launcher_press_feedback(card);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, launcher_back_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_align(card, LV_ALIGN_BOTTOM_MID, 0, -LAUNCHER_MARGIN - 40);
  lv_obj_center(launcher_label(card, LV_SYMBOL_LEFT "  BACK",
                               &launcher_font_oswald64, COLOR_PAPER));

  g_about_scr = scr;
}

/****************************************************************************
 * Name: paint screen
 ****************************************************************************/

static void launcher_paint_dot(int32_t x, int32_t y)
{
  int32_t dx;
  int32_t dy;

  for (dy = -PAINT_BRUSH_RADIUS; dy <= PAINT_BRUSH_RADIUS; dy++)
    {
      for (dx = -PAINT_BRUSH_RADIUS; dx <= PAINT_BRUSH_RADIUS; dx++)
        {
          if (dx * dx + dy * dy <=
              PAINT_BRUSH_RADIUS * PAINT_BRUSH_RADIUS &&
              x + dx >= 0 && x + dx < LAUNCHER_SCREEN_W &&
              y + dy >= 0 && y + dy < LAUNCHER_SCREEN_H)
            {
              g_canvas_buf[(y + dy) * LAUNCHER_SCREEN_W + x + dx] =
                g_brush_color;
            }
        }
    }
}

static void launcher_paint_stroke(int32_t x0, int32_t y0,
                                  int32_t x1, int32_t y1)
{
  int32_t dx = x1 - x0;
  int32_t dy = y1 - y0;
  int32_t steps = LV_MAX(LV_ABS(dx), LV_ABS(dy));
  int32_t i;

  if (steps == 0)
    {
      launcher_paint_dot(x1, y1);
      return;
    }

  for (i = 0; i <= steps; i += PAINT_STROKE_STEP)
    {
      launcher_paint_dot(x0 + dx * i / steps, y0 + dy * i / steps);
    }
}

static void launcher_paint_invalidate(int32_t x0, int32_t y0,
                                      int32_t x1, int32_t y1)
{
  lv_area_t area =
    {
      LV_MIN(x0, x1) - PAINT_BRUSH_RADIUS - 2,
      LV_MIN(y0, y1) - PAINT_BRUSH_RADIUS - 2,
      LV_MAX(x0, x1) + PAINT_BRUSH_RADIUS + 2,
      LV_MAX(y0, y1) + PAINT_BRUSH_RADIUS + 2
    };

  lv_obj_invalidate_area(g_canvas, &area);
}

static void launcher_paint_record(int16_t x, int16_t y)
{
  if (g_npts < PAINT_MAX_POINTS)
    {
      g_pts[g_npts].x = x;
      g_pts[g_npts].y = y;
      g_npts++;
    }
}

static void launcher_result_hide(void)
{
  lv_anim_t a;

  if (lv_obj_get_style_translate_y(g_result, 0) == -560)
    {
      return;
    }

  lv_anim_init(&a);
  lv_anim_set_var(&a, g_result);
  lv_anim_set_exec_cb(&a, launcher_anim_y_cb);
  lv_anim_set_values(&a, lv_obj_get_style_translate_y(g_result, 0), -560);
  lv_anim_set_duration(&a, 220);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
  lv_anim_start(&a);
}

static void launcher_result_tap_cb(lv_event_t *e)
{
  UNUSED(e);
  launcher_result_hide();
}

static void launcher_recognize(void)
{
  float image[LAUNCHER_DIGIT_PIXELS];
  float prob[10];
  int order[3];
  char digit[2];
  lv_anim_t a;
  int best;
  int i;
  int j;
  int x;
  int y;

  if (g_paint_timer != NULL)
    {
      lv_timer_delete(g_paint_timer);
      g_paint_timer = NULL;
    }

  best = launcher_digit_classify(g_pts, g_npts, prob, image);
  if (best < 0)
    {
      return;
    }

  g_recognized = true;

  /* Top three classes */

  for (i = 0; i < 3; i++)
    {
      order[i] = -1;
      for (j = 0; j < 10; j++)
        {
          if ((i == 0 || j != order[0]) && (i < 2 || j != order[1]) &&
              (order[i] < 0 || prob[j] > prob[order[i]]))
            {
              order[i] = j;
            }
        }
    }

  digit[0] = '0' + best;
  digit[1] = '\0';
  roll_set(&g_result_digit, digit);
  lv_label_set_text_fmt(g_result_conf, "%d%%",
                        (int)(prob[best] * 100 + 0.5f));

  for (i = 0; i < 3; i++)
    {
      lv_label_set_text_fmt(g_result_rank[i], "%d", order[i]);
      lv_label_set_text_fmt(g_result_pct[i], "%d.%d%%",
                            (int)(prob[order[i]] * 100),
                            (int)(prob[order[i]] * 1000) % 10);
      lv_bar_set_value(g_result_bar[i], 0, LV_ANIM_OFF);
      lv_bar_set_value(g_result_bar[i],
                       LV_MAX(2, (int32_t)(prob[order[i]] * 100)),
                       LV_ANIM_ON);
    }

  /* What the model saw, magnified */

  for (y = 0; y < PREVIEW_SIDE; y++)
    {
      for (x = 0; x < PREVIEW_SIDE; x++)
        {
          uint8_t v = (uint8_t)(image[(y / PREVIEW_SCALE) *
                                      LAUNCHER_DIGIT_SIDE +
                                      x / PREVIEW_SCALE] * 255);

          g_preview_buf[y * PREVIEW_SIDE + x] =
            lv_color_to_u16(lv_color_make(v, v, v));
        }
    }

  lv_obj_invalidate(g_preview);

  lv_anim_init(&a);
  lv_anim_set_var(&a, g_result);
  lv_anim_set_exec_cb(&a, launcher_anim_y_cb);
  lv_anim_set_values(&a, lv_obj_get_style_translate_y(g_result, 0), 0);
  lv_anim_set_duration(&a, 380);
  lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
  lv_anim_start(&a);
}

static void launcher_paint_timer_cb(lv_timer_t *timer)
{
  UNUSED(timer);
  g_paint_timer = NULL;
  launcher_recognize();
}

static void launcher_canvas_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_active();
  lv_point_t p;

  if (indev == NULL)
    {
      return;
    }

  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
    {
      launcher_paint_record(LAUNCHER_PEN_UP, 0);
      g_paint_timer = lv_timer_create(launcher_paint_timer_cb,
                                      PAINT_IDLE_MS, NULL);
      lv_timer_set_repeat_count(g_paint_timer, 1);
      return;
    }

  lv_indev_get_point(indev, &p);

  if (code == LV_EVENT_PRESSED)
    {
      if (g_paint_timer != NULL)
        {
          lv_timer_delete(g_paint_timer);
          g_paint_timer = NULL;
        }

      /* One digit at a time: a new stroke after a result starts over */

      if (g_recognized)
        {
          lv_canvas_fill_bg(g_canvas, lv_color_hex(PAINT_BG), LV_OPA_COVER);
          g_npts = 0;
          g_recognized = false;
        }

      g_brush_prev = p;
      lv_obj_add_flag(g_paint_hint, LV_OBJ_FLAG_HIDDEN);
      launcher_result_hide();
    }

  launcher_paint_stroke(g_brush_prev.x, g_brush_prev.y, p.x, p.y);
  launcher_paint_invalidate(g_brush_prev.x, g_brush_prev.y, p.x, p.y);
  launcher_paint_record(p.x, p.y);
  g_brush_prev = p;
}

static void launcher_paint_clear_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_canvas_fill_bg(g_canvas, lv_color_hex(PAINT_BG), LV_OPA_COVER);
  lv_obj_clear_flag(g_paint_hint, LV_OBJ_FLAG_HIDDEN);
  launcher_result_hide();
  g_npts = 0;
  g_recognized = false;
}

static void launcher_paint_read_cb(lv_event_t *e)
{
  UNUSED(e);
  launcher_recognize();
}

static void launcher_paint_select(lv_obj_t *btn)
{
  const lv_color_t *color = lv_obj_get_user_data(btn);

  if (g_brush_btn != NULL)
    {
      lv_obj_set_style_border_color(g_brush_btn,
                                    lv_color_hex(COLOR_INK_2), 0);
      lv_obj_set_style_border_width(g_brush_btn, 3, 0);
      lv_obj_set_style_translate_y(g_brush_btn, 0, 0);
    }

  lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_PAPER), 0);
  lv_obj_set_style_border_width(btn, 8, 0);
  lv_obj_set_style_translate_y(btn, -14, 0);
  g_brush_btn = btn;
  g_brush_color = lv_color_to_u16(*color);
}

static void launcher_paint_color_cb(lv_event_t *e)
{
  launcher_paint_select(lv_event_get_target(e));
}

static lv_obj_t *launcher_paint_color_btn(lv_obj_t *parent, int idx)
{
  static lv_color_t button_colors[nitems(g_palette)];
  lv_obj_t *btn;

  button_colors[idx] = lv_color_hex(g_palette[idx]);
  btn = launcher_box(parent, 100, 100, g_palette[idx], 8);
  lv_obj_set_style_border_width(btn, 3, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_INK_2), 0);
  lv_obj_set_user_data(btn, &button_colors[idx]);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btn, launcher_paint_color_cb, LV_EVENT_CLICKED, NULL);
  return btn;
}

static void launcher_create_result_card(lv_obj_t *scr)
{
  lv_obj_t *col;
  lv_obj_t *obj;
  int i;

  g_result = launcher_box(scr, LAUNCHER_CONTENT_W, 420, COLOR_INK,
                          LAUNCHER_RADIUS);
  lv_obj_set_pos(g_result, LAUNCHER_MARGIN, LAUNCHER_MARGIN);
  lv_obj_set_style_translate_y(g_result, -560, 0);
  lv_obj_add_flag(g_result, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_result, launcher_result_tap_cb, LV_EVENT_CLICKED,
                      NULL);

  obj = launcher_caps(g_result, "ON-DEVICE AI  /  RESULT", COLOR_MUTED);
  lv_obj_set_pos(obj, 44, 40);

  obj = roll_create(&g_result_digit, g_result, "0",
                    &launcher_font_clock300, COLOR_ORANGE);
  lv_obj_set_pos(obj, 40, 100);

  g_result_conf = launcher_label(g_result, "", &launcher_font_oswald64,
                                 COLOR_PAPER);
  lv_obj_set_pos(g_result_conf, 290, 92);

  col = launcher_box(g_result, 380, LV_SIZE_CONTENT, COLOR_INK, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, 14, 0);
  lv_obj_set_pos(col, 290, 190);

  for (i = 0; i < 3; i++)
    {
      lv_obj_t *row = launcher_box(col, 380, 44, COLOR_INK, 0);

      g_result_rank[i] = launcher_label(row, "-", &launcher_font_outfit34,
                                        COLOR_PAPER);
      lv_obj_align(g_result_rank[i], LV_ALIGN_LEFT_MID, 0, 0);

      g_result_bar[i] = lv_bar_create(row);
      lv_obj_set_size(g_result_bar[i], 210, 18);
      lv_obj_set_style_radius(g_result_bar[i], 0, 0);
      lv_obj_set_style_radius(g_result_bar[i], 0, LV_PART_INDICATOR);
      lv_obj_set_style_bg_color(g_result_bar[i],
                                lv_color_hex(COLOR_INK_2), 0);
      lv_obj_set_style_bg_color(g_result_bar[i],
                                lv_color_hex(i == 0 ? COLOR_ORANGE :
                                             COLOR_DIM),
                                LV_PART_INDICATOR);
      lv_obj_set_style_anim_duration(g_result_bar[i], 600, 0);
      lv_obj_align(g_result_bar[i], LV_ALIGN_LEFT_MID, 50, 0);

      g_result_pct[i] = launcher_label(row, "", &launcher_font_outfit26,
                                       COLOR_MUTED);
      lv_obj_align(g_result_pct[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }

  g_preview = lv_canvas_create(g_result);
  lv_canvas_set_buffer(g_preview, g_preview_buf, PREVIEW_SIDE,
                       PREVIEW_SIDE, LV_COLOR_FORMAT_RGB565);
  lv_canvas_fill_bg(g_preview, lv_color_black(), LV_OPA_COVER);
  lv_obj_set_style_border_width(g_preview, 2, 0);
  lv_obj_set_style_border_color(g_preview, lv_color_hex(COLOR_INK_2), 0);
  lv_obj_align(g_preview, LV_ALIGN_TOP_RIGHT, -44, 92);

  obj = launcher_label(g_result, "model input",
                       &launcher_font_outfit26, COLOR_MUTED);
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -44, 290);

  obj = launcher_label(g_result, "tap to dismiss", &launcher_font_outfit26,
                       COLOR_DIM);
  lv_obj_align(obj, LV_ALIGN_BOTTOM_LEFT, 44, -28);
}

static bool launcher_create_paint_scr(void)
{
  lv_obj_t *scr;
  lv_obj_t *dock;
  lv_obj_t *first;
  int i;

  g_canvas_buf = memalign(64, CANVAS_BUF_SIZE);
  if (g_canvas_buf == NULL)
    {
      fprintf(stderr, "launcher: canvas alloc failed\n");
      return false;
    }

  scr = launcher_screen(COLOR_BG);

  g_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(g_canvas, g_canvas_buf, LAUNCHER_SCREEN_W,
                       LAUNCHER_SCREEN_H, LV_COLOR_FORMAT_RGB565);
  lv_canvas_fill_bg(g_canvas, lv_color_hex(PAINT_BG), LV_OPA_COVER);
  lv_obj_add_flag(g_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_canvas, launcher_canvas_event_cb, LV_EVENT_PRESSED,
                      NULL);
  lv_obj_add_event_cb(g_canvas, launcher_canvas_event_cb, LV_EVENT_PRESSING,
                      NULL);
  lv_obj_add_event_cb(g_canvas, launcher_canvas_event_cb, LV_EVENT_RELEASED,
                      NULL);
  lv_obj_add_event_cb(g_canvas, launcher_canvas_event_cb,
                      LV_EVENT_PRESS_LOST, NULL);

  g_paint_hint = launcher_box(scr, LAUNCHER_SCREEN_W, LV_SIZE_CONTENT,
                              0, 0);
  lv_obj_set_style_bg_opa(g_paint_hint, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(g_paint_hint, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(g_paint_hint, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_pos(g_paint_hint, 0, 820);
  launcher_label(g_paint_hint, "WRITE A DIGIT", &launcher_font_oswald120,
                 0xd6d9dc);
  launcher_label(g_paint_hint, "0 - 9, one at a time",
                 &launcher_font_outfit34, 0xaab1b8);

  launcher_create_result_card(scr);

  /* Tool dock: back, four colours, clear, recognise */

  dock = launcher_box(scr, LAUNCHER_CONTENT_W, 184, COLOR_INK,
                      LAUNCHER_RADIUS);
  lv_obj_set_flex_flow(dock, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dock, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(dock, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(dock, LV_ALIGN_BOTTOM_MID, 0, -LAUNCHER_MARGIN);

  launcher_square_btn(dock, LV_SYMBOL_LEFT, 100, COLOR_INK_2, COLOR_PAPER,
                      launcher_back_event_cb);

  first = launcher_paint_color_btn(dock, 0);
  for (i = 1; i < (int)nitems(g_palette); i++)
    {
      launcher_paint_color_btn(dock, i);
    }

  launcher_square_btn(dock, LV_SYMBOL_TRASH, 100, COLOR_INK_2, COLOR_PAPER,
                      launcher_paint_clear_cb);
  launcher_square_btn(dock, LV_SYMBOL_EYE_OPEN, 100, COLOR_ORANGE,
                      COLOR_PAPER, launcher_paint_read_cb);

  launcher_paint_select(first);
  g_paint_scr = scr;
  return true;
}

/****************************************************************************
 * Name: home screen
 ****************************************************************************/

static void launcher_home_btn_event_cb(lv_event_t *e)
{
  lv_obj_t *target = lv_event_get_user_data(e);

  launcher_switch_scr(target, LV_SCR_LOAD_ANIM_MOVE_TOP);
}

static lv_obj_t *launcher_home_block(lv_obj_t *parent, int32_t w,
                                     int32_t h, uint32_t bg)
{
  lv_obj_t *block = launcher_box(parent, w, h, bg, LAUNCHER_RADIUS);

  g_home_blocks[g_home_nblocks++] = block;
  return block;
}

static lv_obj_t *launcher_stat_block(lv_obj_t *parent, uint32_t bg,
                                     uint32_t fg, const char *name)
{
  lv_obj_t *block = launcher_home_block(parent, LAUNCHER_HALF_W, 260, bg);
  lv_obj_t *value;

  lv_obj_set_pos(launcher_caps(block, name, fg), 44, 44);

  value = launcher_label(block, "-", &launcher_font_oswald120, fg);
  lv_obj_align(value, LV_ALIGN_BOTTOM_LEFT, 40, -20);
  return value;
}

static lv_obj_t *launcher_app_block(lv_obj_t *parent, int32_t h,
                                    const char *icon, const char *name,
                                    const char *hint, uint32_t bg,
                                    uint32_t fg, uint32_t icon_fg,
                                    lv_obj_t *target)
{
  lv_obj_t *block = launcher_home_block(parent, LAUNCHER_HALF_W, h, bg);
  lv_obj_t *obj;

  launcher_press_feedback(block);
  lv_obj_add_flag(block, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(block, launcher_home_btn_event_cb, LV_EVENT_CLICKED,
                      target);

  obj = launcher_label(block, icon, &lv_font_montserrat_48, icon_fg);
  lv_obj_set_pos(obj, 40, 44);

  obj = launcher_label(block, LV_SYMBOL_RIGHT, &lv_font_montserrat_40, fg);
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -40, 50);

  obj = launcher_label(block, name, &launcher_font_oswald120, fg);
  lv_obj_align(obj, LV_ALIGN_BOTTOM_LEFT, 36, -72);

  obj = launcher_label(block, hint, &launcher_font_outfit26, fg);
  lv_obj_align(obj, LV_ALIGN_BOTTOM_LEFT, 42, -34);
  return block;
}

static void launcher_home_enter_cb(lv_event_t *e)
{
  int i;

  UNUSED(e);

  for (i = 0; i < g_home_nblocks; i++)
    {
      launcher_slide_in(g_home_blocks[i], 0, 90, 120 + i * 60);
    }

  launcher_count_up(g_val_heap, "%dM", g_heap_free_mb, 250);
  launcher_count_up(g_val_fps, "%d", g_fps, 250);
}

static void launcher_create_home_scr(void)
{
  lv_obj_t *scr = launcher_screen(COLOR_BG);
  lv_obj_t *grid;
  lv_obj_t *block;
  lv_obj_t *col;
  lv_obj_t *obj;

  lv_obj_add_event_cb(scr, launcher_home_enter_cb,
                      LV_EVENT_SCREEN_LOAD_START, NULL);

  launcher_top_bar(scr, &g_home_fps, "- FPS");

  grid = launcher_box(scr, LAUNCHER_CONTENT_W, LV_SIZE_CONTENT, COLOR_BG, 0);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(grid, 24, 0);
  lv_obj_set_style_pad_column(grid, 24, 0);
  lv_obj_set_pos(grid, LAUNCHER_MARGIN, 140);

  /* Clock block */

  block = launcher_home_block(grid, LAUNCHER_CONTENT_W, 440, COLOR_INK);
  lv_obj_set_pos(launcher_caps(block, "UPTIME CLOCK", COLOR_MUTED), 44, 44);

  obj = launcher_label(block, LV_SYMBOL_SETTINGS "  NuttX / openvela",
                       &launcher_font_outfit26, COLOR_MUTED);
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -44, 44);

  obj = roll_create(&g_home_hhmm, block, "00:00", &launcher_font_clock300,
                    COLOR_PAPER);
  lv_obj_align(obj, LV_ALIGN_BOTTOM_LEFT, 32, -24);

  obj = roll_create(&g_home_ss, block, "00", &launcher_font_oswald120,
                    COLOR_ORANGE);
  lv_obj_align(obj, LV_ALIGN_BOTTOM_RIGHT, -44, -170);

  g_val_heap = launcher_stat_block(grid, COLOR_YELLOW, COLOR_INK,
                                   "HEAP FREE");
  g_val_fps = launcher_stat_block(grid, COLOR_BLUE, COLOR_PAPER,
                                  "FRAMES / S");

  block = launcher_home_block(grid, LAUNCHER_CONTENT_W, 150, COLOR_PAPER);
  lv_obj_align(launcher_caps(block, "UPTIME", COLOR_DIM), LV_ALIGN_LEFT_MID,
               44, 0);
  g_val_uptime = launcher_label(block, "-", &launcher_font_oswald64,
                                COLOR_INK);
  lv_obj_align(g_val_uptime, LV_ALIGN_RIGHT_MID, -44, 0);

  /* Apps: tall paint on the left, about and bench stacked on the right */

  block = launcher_app_block(grid, 680, LV_SYMBOL_EDIT, "PAINT",
                             "Write a digit, the board reads it",
                             COLOR_ORANGE, COLOR_PAPER, COLOR_PAPER,
                             g_paint_scr);

  obj = launcher_box(block, LV_SIZE_CONTENT, LV_SIZE_CONTENT, COLOR_PAPER,
                     8);
  lv_obj_set_style_pad_hor(obj, 18, 0);
  lv_obj_set_style_pad_ver(obj, 6, 0);
  lv_obj_set_pos(obj, 40, 130);
  obj = launcher_label(obj, "AI DIGITS", &launcher_font_outfit26,
                       COLOR_ORANGE);
  lv_obj_set_style_text_letter_space(obj, 3, 0);

  col = launcher_box(grid, LAUNCHER_HALF_W, 680, COLOR_BG, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, 24, 0);

  launcher_app_block(col, 328, LV_SYMBOL_LIST, "ABOUT", "Port details",
                     COLOR_PAPER, COLOR_INK, COLOR_INK, g_about_scr);
  launcher_app_block(col, 328, LV_SYMBOL_CHARGE, "BENCH",
                     "LVGL render + CoreMark", COLOR_INK, COLOR_PAPER,
                     COLOR_ORANGE, g_bench_scr);

  g_home_scr = scr;
}

/****************************************************************************
 * Name: periodic update
 ****************************************************************************/

static void launcher_periodic_cb(lv_timer_t *timer)
{
  struct mallinfo mi = mallinfo();
  uint32_t sec = launcher_uptime_sec();
  char buf[8];

  UNUSED(timer);

  g_fps = g_refr_count;
  g_refr_count = 0;
  g_heap_free_mb = mi.fordblks >> 20;

  snprintf(buf, sizeof(buf), "%02u", (unsigned)(sec / 3600) % 100);
  roll_set(&g_lock_hh, buf);
  snprintf(buf, sizeof(buf), "%02u", (unsigned)(sec / 60) % 60);
  roll_set(&g_lock_mm, buf);
  snprintf(buf, sizeof(buf), "%02u", (unsigned)sec % 60);
  roll_set(&g_lock_ss, buf);
  roll_set(&g_home_ss, buf);
  snprintf(buf, sizeof(buf), "%02u%02u", (unsigned)(sec / 3600) % 100,
           (unsigned)(sec / 60) % 60);
  roll_set(&g_home_hhmm, buf);

  lv_label_set_text_fmt(g_home_fps, "%u FPS", (unsigned)g_fps);

  /* Leave numbers alone while their count-up is still running */

  if (lv_anim_get(g_val_heap, NULL) == NULL)
    {
      lv_label_set_text_fmt(g_val_heap, "%uM", (unsigned)g_heap_free_mb);
    }

  if (lv_anim_get(g_val_fps, NULL) == NULL)
    {
      lv_label_set_text_fmt(g_val_fps, "%u", (unsigned)g_fps);
    }

  if (lv_anim_get(g_about_fps, NULL) == NULL)
    {
      lv_label_set_text_fmt(g_about_fps, "%u", (unsigned)g_fps);
    }

  if (sec < 3600)
    {
      lv_label_set_text_fmt(g_val_uptime, "%um %02us",
                            (unsigned)sec / 60, (unsigned)sec % 60);
    }
  else
    {
      lv_label_set_text_fmt(g_val_uptime, "%uh %02um",
                            (unsigned)sec / 3600,
                            (unsigned)(sec / 60) % 60);
    }

  lv_label_set_text_fmt(g_about_heap, "Heap %uKB free of %uKB",
                        (unsigned)mi.fordblks / 1024,
                        (unsigned)mi.arena / 1024);
}

/* Count frames, and time each refresh for the benchmark screen */

static void launcher_refr_event_cb(lv_event_t *e)
{
  struct timespec now;

  if (lv_event_get_code(e) == LV_EVENT_REFR_START)
    {
      g_refr_count++;
      clock_gettime(CLOCK_MONOTONIC, &g_refr_start);
      return;
    }

  clock_gettime(CLOCK_MONOTONIC, &now);
  launcher_bench_frame((now.tv_sec - g_refr_start.tv_sec) * 1000000 +
                       (now.tv_nsec - g_refr_start.tv_nsec) / 1000);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

lv_obj_t *launcher_label(lv_obj_t *parent, const char *text,
                         const lv_font_t *font, uint32_t color)
{
  lv_obj_t *label = lv_label_create(parent);

  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_label_set_text(label, text);
  return label;
}

lv_obj_t *launcher_caps(lv_obj_t *parent, const char *text, uint32_t color)
{
  lv_obj_t *label = launcher_label(parent, text, &launcher_font_outfit26,
                                   color);

  lv_obj_set_style_text_letter_space(label, 5, 0);
  return label;
}

lv_obj_t *launcher_box(lv_obj_t *parent, int32_t w, int32_t h,
                       uint32_t color, int32_t radius)
{
  lv_obj_t *obj = lv_obj_create(parent);

  lv_obj_remove_style_all(obj);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, radius, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

lv_obj_t *launcher_square_btn(lv_obj_t *parent, const char *symbol,
                              int32_t size, uint32_t bg, uint32_t fg,
                              lv_event_cb_t cb)
{
  lv_obj_t *btn = launcher_box(parent, size, size, bg, LAUNCHER_RADIUS);

  launcher_press_feedback(btn);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
  lv_obj_center(launcher_label(btn, symbol, &lv_font_montserrat_40, fg));
  return btn;
}

static void launcher_count_exec_cb(lv_anim_t *a, int32_t v)
{
  lv_label_set_text_fmt(a->var, lv_anim_get_user_data(a), (int)v);
}

void launcher_count_up(lv_obj_t *label, const char *fmt, int32_t to,
                       uint32_t delay)
{
  lv_anim_t a;

  /* This LVGL does not replace custom-callback animations by itself */

  lv_anim_delete(label, NULL);

  lv_anim_init(&a);
  lv_anim_set_var(&a, label);
  lv_anim_set_custom_exec_cb(&a, launcher_count_exec_cb);
  lv_anim_set_user_data(&a, (void *)fmt);
  lv_anim_set_values(&a, 0, to);
  lv_anim_set_duration(&a, 700);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_label_set_text_fmt(label, fmt, 0);
  lv_anim_start(&a);
}

void launcher_slide_in(lv_obj_t *obj, int32_t dx, int32_t dy,
                       uint32_t delay)
{
  lv_anim_t a;

  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_duration(&a, 320);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_early_apply(&a, true);

  if (dx != 0)
    {
      lv_anim_set_exec_cb(&a, launcher_anim_x_cb);
      lv_anim_set_values(&a, dx, 0);
      lv_anim_start(&a);
    }

  if (dy != 0)
    {
      lv_anim_set_exec_cb(&a, launcher_anim_y_cb);
      lv_anim_set_values(&a, dy, 0);
      lv_anim_start(&a);
    }
}

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

  UNUSED(argc);
  UNUSED(argv);

  lv_init();

  lv_nuttx_dsc_init(&info);
  info.input_path = TOUCH_DEVPATH;
  lv_nuttx_init(&info, &result);

  if (result.disp == NULL)
    {
      fprintf(stderr, "launcher: no display found\n");
      return 1;
    }

  if (result.indev == NULL)
    {
      fprintf(stderr, "launcher: no touchscreen found\n");
      return 1;
    }

  lv_display_add_event_cb(result.disp, launcher_refr_event_cb,
                          LV_EVENT_REFR_START, NULL);
  lv_display_add_event_cb(result.disp, launcher_refr_event_cb,
                          LV_EVENT_REFR_READY, NULL);

  launcher_create_about_scr();
  if (!launcher_create_paint_scr())
    {
      return 1;
    }

  g_bench_scr = launcher_bench_create(launcher_back_event_cb);
  launcher_create_home_scr();
  launcher_create_lock_scr();

  lv_screen_load(g_lock_scr);
  launcher_periodic_cb(NULL);
  lv_timer_create(launcher_periodic_cb, 1000, NULL);

  while (1)
    {
      uint32_t idle = lv_timer_handler();

      idle = LV_CLAMP(1, idle, 100);
      usleep(idle * 1000);
    }

  return 0;
}
