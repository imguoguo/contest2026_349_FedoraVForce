/****************************************************************************
 * app/paint/paint_main.c
 *
 * LVGL paint app with on-device handwritten digit recognition
 * VSAI LogicPi A1 (Amlogic S6): MIPI DSI 1080x1920 + GT9XX touch
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "digit.h"
#include "lvx_ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PAINT_BUF_SIZE        (UI_SCREEN_W * UI_SCREEN_H * sizeof(uint16_t))

#define PAINT_BAR_Y           1664
#define PAINT_BAR_H           184
#define PAINT_BTN_SIZE        88

/* Stroke points kept for recognition, closer ones are skipped */

#define PAINT_MAX_POINTS      2048
#define PAINT_POINT_STEP      4

#define PAINT_RECOGNIZE_DELAY 700     /* ms after the pen lifts */

#define PAINT_CARD_H          420
#define PAINT_INPUT_SCALE     6       /* Model input shown 168x168 */
#define PAINT_INPUT_SIZE      (DIGIT_SIZE * PAINT_INPUT_SCALE)
#define PAINT_TOP_N           3
#define PAINT_TOP_BAR_W       207

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Colors, the last one is the paper and works as eraser */

static const uint32_t g_palette[] =
{
  UI_INK, UI_ORANGE, UI_BLUE, UI_YELLOW, UI_PAPER
};

/* Brush radius, 16px wide by default */

static const int32_t g_brush_radius[] =
{
  8, 16, 4
};

static lv_obj_t *g_canvas;
static lv_obj_t *g_swatch;
static lv_obj_t *g_size_dot;
static lv_obj_t *g_eye;
static uint16_t *g_canvas_buf;
static uint16_t g_brush_color;
static bool g_eraser;
static lv_point_t g_brush_prev;
static int g_brush_size;
static bool g_running;

/* Recognition */

static struct digit_point_s g_points[PAINT_MAX_POINTS];
static int g_npoints;
static bool g_recognize;
static lv_timer_t *g_recognize_timer;

static lv_obj_t *g_card;
static lv_obj_t *g_card_digit;
static lv_obj_t *g_card_conf;
static lv_obj_t *g_card_top[PAINT_TOP_N][3];   /* digit, bar, percent */
static lv_obj_t *g_card_input;
static uint16_t *g_input_buf;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static lv_obj_t *paint_button(lv_obj_t *parent, uint32_t color,
                              lv_event_cb_t cb)
{
  lv_obj_t *btn = ui_box(parent, color, UI_RADIUS);

  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(btn, PAINT_BTN_SIZE, PAINT_BTN_SIZE);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
  return btn;
}

static lv_obj_t *paint_icon_button(lv_obj_t *parent, const char *symbol,
                                   lv_event_cb_t cb)
{
  lv_obj_t *btn = paint_button(parent, UI_INK_2, cb);

  lv_obj_set_style_bg_color(btn, lv_color_hex(UI_DIM), LV_STATE_PRESSED);
  lv_obj_center(ui_label(btn, UI_ICON_48, 0xffffff, symbol));
  return btn;
}

/* Percent with one decimal, LVGL's sprintf has no %f */

static void paint_percent(lv_obj_t *label, float p)
{
  int v = (int)(p * 1000 + 0.5f);

  lv_label_set_text_fmt(label, "%d.%d%%", v / 10, v % 10);
}

/****************************************************************************
 * Name: drawing
 *
 * A stroke segment is drawn as a filled capsule, so a fast finger still
 * leaves one continuous line.
 ****************************************************************************/

static void paint_segment(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
  int32_t r = g_brush_radius[g_brush_size];
  int32_t dx = x1 - x0;
  int32_t dy = y1 - y0;
  int32_t len2 = dx * dx + dy * dy;
  lv_area_t area;
  int32_t px;
  int32_t py;
  int32_t t;
  int32_t ex;
  int32_t ey;

  area.x1 = MAX(MIN(x0, x1) - r, 0);
  area.y1 = MAX(MIN(y0, y1) - r, 0);
  area.x2 = MIN(MAX(x0, x1) + r, UI_SCREEN_W - 1);
  area.y2 = MIN(MAX(y0, y1) + r, UI_SCREEN_H - 1);

  for (py = area.y1; py <= area.y2; py++)
    {
      for (px = area.x1; px <= area.x2; px++)
        {
          /* Closest point of the segment, t scaled by len2 */

          t = len2 ? (px - x0) * dx + (py - y0) * dy : 0;
          t = t < 0 ? 0 : t > len2 ? len2 : t;
          ex = len2 ? x0 + dx * t / len2 - px : x0 - px;
          ey = len2 ? y0 + dy * t / len2 - py : y0 - py;

          if (ex * ex + ey * ey <= r * r)
            {
              g_canvas_buf[py * UI_SCREEN_W + px] = g_brush_color;
            }
        }
    }

  lv_obj_invalidate_area(g_canvas, &area);
}

static void paint_record(int16_t x, int16_t y)
{
  struct digit_point_s *last = g_npoints > 0 ? &g_points[g_npoints - 1] :
                               NULL;

  if (g_eraser || g_npoints >= PAINT_MAX_POINTS - 1)
    {
      return;
    }

  if (y != DIGIT_PEN_UP && last != NULL && last->y != DIGIT_PEN_UP &&
      LV_ABS(last->x - x) < PAINT_POINT_STEP &&
      LV_ABS(last->y - y) < PAINT_POINT_STEP)
    {
      return;
    }

  g_points[g_npoints].x = x;
  g_points[g_npoints].y = y;
  g_npoints++;
}

/****************************************************************************
 * Name: recognition
 ****************************************************************************/

static void paint_translate_y_cb(void *obj, int32_t v)
{
  lv_obj_set_style_translate_y(obj, v, 0);
}

static void paint_card_hidden_cb(lv_anim_t *a)
{
  lv_obj_add_flag(a->var, LV_OBJ_FLAG_HIDDEN);
}

static void paint_card_close_cb(lv_event_t *e)
{
  UNUSED(e);
  ui_anim(g_card, paint_translate_y_cb, 0, -(PAINT_CARD_H + UI_MARGIN),
          UI_ENTER_TIME, 0, paint_card_hidden_cb);
}

/* The 28x28 input, white on black like MNIST, also as text on stdout */

static void paint_show_input(const float img[DIGIT_INPUTS])
{
  uint16_t c;
  int x;
  int y;

  for (y = 0; y < PAINT_INPUT_SIZE; y++)
    {
      for (x = 0; x < PAINT_INPUT_SIZE; x++)
        {
          c = img[y / PAINT_INPUT_SCALE * DIGIT_SIZE + x / PAINT_INPUT_SCALE]
              * 255;
          g_input_buf[y * PAINT_INPUT_SIZE + x] =
            lv_color_to_u16(lv_color_make(c, c, c));
        }
    }

  lv_obj_invalidate(g_card_input);

  for (y = 0; y < DIGIT_SIZE; y++)
    {
      for (x = 0; x < DIGIT_SIZE; x++)
        {
          putchar(" .:-=+*#%@"[(int)(img[y * DIGIT_SIZE + x] * 9.99f)]);
        }

      putchar('\n');
    }
}

static void paint_recognize_cb(lv_timer_t *timer)
{
  float img[DIGIT_INPUTS];
  float prob[DIGIT_CLASSES];
  float rest[DIGIT_CLASSES];
  int top[PAINT_TOP_N];
  int best;
  int i;
  int j;

  UNUSED(timer);
  g_recognize_timer = NULL;

  if (g_npoints == 0)
    {
      return;
    }

  digit_preprocess(g_points, g_npoints, img);
  best = digit_classify(img, prob);
  paint_show_input(img);

  /* Top three by probability */

  memcpy(rest, prob, sizeof(rest));
  for (i = 0; i < PAINT_TOP_N; i++)
    {
      top[i] = 0;
      for (j = 1; j < DIGIT_CLASSES; j++)
        {
          top[i] = rest[j] > rest[top[i]] ? j : top[i];
        }

      rest[top[i]] = -1.0f;

      lv_label_set_text_fmt(g_card_top[i][0], "%d", top[i]);
      lv_obj_set_width(g_card_top[i][1],
                       MAX((int32_t)(prob[top[i]] * PAINT_TOP_BAR_W), 4));
      paint_percent(g_card_top[i][2], prob[top[i]]);
    }

  printf("paint: %d (%d%%), then %d, %d\n", best,
         (int)(prob[best] * 100 + 0.5f), top[1], top[2]);

  lv_label_set_text_fmt(g_card_digit, "%d", best);
  lv_label_set_text_fmt(g_card_conf, "%d%%",
                        (int)(prob[best] * 100 + 0.5f));

  if (lv_obj_has_flag(g_card, LV_OBJ_FLAG_HIDDEN))
    {
      lv_obj_remove_flag(g_card, LV_OBJ_FLAG_HIDDEN);
      ui_enter(g_card, 0, -(PAINT_CARD_H + UI_MARGIN), 0);
    }
}

static void paint_create_card(lv_obj_t *scr)
{
  lv_obj_t *obj;
  int32_t y;
  int i;

  g_card = ui_box(scr, UI_INK, UI_RADIUS);
  lv_obj_add_flag(g_card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(g_card, UI_MARGIN, UI_MARGIN);
  lv_obj_set_size(g_card, UI_CONTENT_W, PAINT_CARD_H);
  lv_obj_add_event_cb(g_card, paint_card_close_cb, LV_EVENT_CLICKED, NULL);

  ui_place(ui_tag(g_card, UI_MUTED, "ON-DEVICE AI  /  RESULT"),
           UI_OUTFIT_26, 44, 43);
  g_card_digit = ui_label(g_card, UI_OSWALD_300, UI_ORANGE, "");
  ui_place(g_card_digit, UI_OSWALD_300, 60, 103);
  g_card_conf = ui_label(g_card, UI_OSWALD_64, 0xffffff, "");
  ui_place(g_card_conf, UI_OSWALD_64, 290, 98);
  ui_place(ui_label(g_card, UI_OUTFIT_26, UI_DIM, "tap to dismiss"),
           UI_OUTFIT_26, 44, 368);

  for (i = 0; i < PAINT_TOP_N; i++)
    {
      y = 195 + i * 58;
      g_card_top[i][0] = ui_label(g_card, UI_OUTFIT_34, 0xffffff, "");
      ui_place(g_card_top[i][0], UI_OUTFIT_34, 290, y);

      obj = ui_box(g_card, UI_INK_2, 0);
      lv_obj_set_pos(obj, 340, y + 4);
      lv_obj_set_size(obj, PAINT_TOP_BAR_W, 18);
      g_card_top[i][1] = ui_box(obj, i == 0 ? UI_ORANGE : UI_DIM, 0);
      lv_obj_set_size(g_card_top[i][1], 0, 18);

      g_card_top[i][2] = ui_label(g_card, UI_OUTFIT_26, UI_MUTED, "");
      lv_obj_align(g_card_top[i][2], LV_ALIGN_TOP_RIGHT, -267,
                   y + 2 - ui_ink_top(UI_OUTFIT_26));
    }

  /* What the model saw, scaled up */

  obj = ui_box(g_card, UI_INK_2, 0);
  lv_obj_set_pos(obj, 720, 92);
  lv_obj_set_size(obj, PAINT_INPUT_SIZE + 2, PAINT_INPUT_SIZE + 2);
  g_card_input = lv_canvas_create(obj);
  lv_canvas_set_buffer(g_card_input, g_input_buf, PAINT_INPUT_SIZE,
                       PAINT_INPUT_SIZE, LV_COLOR_FORMAT_RGB565);
  lv_obj_center(g_card_input);
  obj = ui_label(g_card, UI_OUTFIT_26, UI_MUTED, "model input");
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -44, 290);
}

/****************************************************************************
 * Name: tool bar
 ****************************************************************************/

static void paint_canvas_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_point_t p;

  lv_indev_get_point(lv_indev_active(), &p);

  if (code == LV_EVENT_PRESSED)
    {
      g_brush_prev = p;
      if (g_recognize_timer != NULL)
        {
          lv_timer_delete(g_recognize_timer);
          g_recognize_timer = NULL;
        }
    }

  if (code == LV_EVENT_RELEASED)
    {
      paint_record(0, DIGIT_PEN_UP);
      if (g_recognize && g_npoints > 0)
        {
          g_recognize_timer = lv_timer_create(paint_recognize_cb,
                                              PAINT_RECOGNIZE_DELAY, NULL);
          lv_timer_set_repeat_count(g_recognize_timer, 1);
        }

      return;
    }

  paint_segment(g_brush_prev.x, g_brush_prev.y, p.x, p.y);
  paint_record(p.x, p.y);
  g_brush_prev = p;
}

static void paint_home_cb(lv_event_t *e)
{
  UNUSED(e);
  g_running = false;
}

static void paint_clear_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_canvas_fill_bg(g_canvas, lv_color_hex(UI_PAPER), LV_OPA_COVER);
  g_npoints = 0;
}

static void paint_size_cb(lv_event_t *e)
{
  int32_t d;

  UNUSED(e);

  g_brush_size = (g_brush_size + 1) % nitems(g_brush_radius);
  d = 2 * g_brush_radius[g_brush_size];
  lv_obj_set_size(g_size_dot, d, d);
}

static void paint_color_cb(lv_event_t *e)
{
  lv_obj_t *swatch = lv_event_get_target(e);
  lv_color_t c = lv_obj_get_style_bg_color(swatch, 0);

  lv_obj_remove_state(g_swatch, LV_STATE_CHECKED);
  lv_obj_add_state(swatch, LV_STATE_CHECKED);
  g_brush_color = lv_color_to_u16(c);
  g_eraser = lv_color_to_u32(c) == lv_color_to_u32(lv_color_hex(UI_PAPER));
  g_swatch = swatch;
}

static void paint_eye_cb(lv_event_t *e)
{
  UNUSED(e);

  g_recognize = !g_recognize;
  lv_obj_set_style_bg_color(g_eye, lv_color_hex(g_recognize ? UI_ORANGE :
                                                UI_INK_2), 0);
}

static void paint_create(void)
{
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *bar;
  lv_obj_t *obj;
  int i;

  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  g_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(g_canvas, g_canvas_buf, UI_SCREEN_W, UI_SCREEN_H,
                       LV_COLOR_FORMAT_RGB565);
  lv_canvas_fill_bg(g_canvas, lv_color_hex(UI_PAPER), LV_OPA_COVER);
  lv_obj_add_flag(g_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_canvas, paint_canvas_event_cb, LV_EVENT_PRESSED,
                      NULL);
  lv_obj_add_event_cb(g_canvas, paint_canvas_event_cb, LV_EVENT_PRESSING,
                      NULL);
  lv_obj_add_event_cb(g_canvas, paint_canvas_event_cb, LV_EVENT_RELEASED,
                      NULL);

  /* Tool bar: home, colors, eraser, brush size, clear, recognition */

  bar = ui_box(scr, UI_INK, UI_RADIUS);
  lv_obj_add_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(bar, UI_MARGIN, PAINT_BAR_Y);
  lv_obj_set_size(bar, UI_CONTENT_W, PAINT_BAR_H);
  lv_obj_set_style_pad_hor(bar, 24, 0);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  paint_icon_button(bar, LV_SYMBOL_LEFT, paint_home_cb);

  for (i = 0; i < nitems(g_palette); i++)
    {
      obj = paint_button(bar, g_palette[i], paint_color_cb);
      lv_obj_set_style_border_color(obj, lv_color_hex(UI_INK_2), 0);
      lv_obj_set_style_border_width(obj, 2, 0);
      lv_obj_set_style_border_color(obj, lv_color_white(),
                                    LV_STATE_CHECKED);
      lv_obj_set_style_border_width(obj, 8, LV_STATE_CHECKED);
      if (i == 0)
        {
          g_swatch = obj;
        }
    }

  obj = paint_button(bar, UI_INK_2, paint_size_cb);
  g_size_dot = ui_box(obj, 0xffffff, LV_RADIUS_CIRCLE);
  lv_obj_center(g_size_dot);

  paint_icon_button(bar, LV_SYMBOL_TRASH, paint_clear_cb);
  g_eye = paint_icon_button(bar, LV_SYMBOL_EYE_OPEN, paint_eye_cb);

  paint_create_card(scr);

  /* Start with the 16px brush and the first color */

  g_brush_size = nitems(g_brush_radius) - 1;
  paint_size_cb(NULL);
  lv_obj_send_event(g_swatch, LV_EVENT_CLICKED, NULL);
}

static int paint_start(lv_nuttx_result_t *result)
{
  lv_nuttx_dsc_t info;

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, result);

  if (result->disp == NULL || result->indev == NULL)
    {
      fprintf(stderr, "paint: no display or touchscreen\n");
      return -1;
    }

  g_npoints = 0;
  g_recognize = false;
  g_recognize_timer = NULL;
  paint_create();
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
      fprintf(stderr, "paint: LVGL is busy, start it from the launcher\n");
      return 1;
    }

  g_canvas_buf = memalign(64, PAINT_BUF_SIZE);
  g_input_buf = malloc(PAINT_INPUT_SIZE * PAINT_INPUT_SIZE *
                       sizeof(uint16_t));
  if (g_canvas_buf == NULL || g_input_buf == NULL)
    {
      fprintf(stderr, "paint: canvas alloc failed\n");
      free(g_canvas_buf);
      free(g_input_buf);
      return 1;
    }

  g_running = paint_start(&result) == 0;
  while (g_running)
    {
      idle = lv_timer_handler();
      usleep(MAX(idle, 1) * 1000);
    }

  lv_nuttx_deinit(&result);
  lv_deinit();
  free(g_canvas_buf);
  free(g_input_buf);
  return 0;
}
