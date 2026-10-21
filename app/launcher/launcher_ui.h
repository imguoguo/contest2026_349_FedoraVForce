/****************************************************************************
 * app/launcher/launcher_ui.h
 *
 * Shared look of the launcher: palette, fonts and small widget helpers.
 ****************************************************************************/

#ifndef __APP_LAUNCHER_LAUNCHER_UI_H
#define __APP_LAUNCHER_LAUNCHER_UI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <lvgl/lvgl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LAUNCHER_SCREEN_W     1080
#define LAUNCHER_SCREEN_H     1920
#define LAUNCHER_MARGIN       72
#define LAUNCHER_CONTENT_W    (LAUNCHER_SCREEN_W - 2 * LAUNCHER_MARGIN)
#define LAUNCHER_HALF_W       ((LAUNCHER_CONTENT_W - 24) / 2)
#define LAUNCHER_RADIUS       12

#define COLOR_BG              0xe9ecef
#define COLOR_PAPER           0xf8f9fa
#define COLOR_RULE            0xc8cdd3
#define COLOR_INK             0x101317
#define COLOR_INK_2           0x262b33
#define COLOR_DIM             0x5a6470
#define COLOR_MUTED           0x9aa3ad
#define COLOR_ORANGE          0xff5b1f
#define COLOR_BLUE            0x2d5bff
#define COLOR_BLUE_DIM        0x5a7bff
#define COLOR_YELLOW          0xffd43b

/****************************************************************************
 * Public Data
 ****************************************************************************/

LV_FONT_DECLARE(launcher_font_clock300);
LV_FONT_DECLARE(launcher_font_oswald120);
LV_FONT_DECLARE(launcher_font_oswald64);
LV_FONT_DECLARE(launcher_font_outfit26);
LV_FONT_DECLARE(launcher_font_outfit34);
LV_FONT_DECLARE(launcher_font_outfit52);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

lv_obj_t *launcher_label(lv_obj_t *parent, const char *text,
                         const lv_font_t *font, uint32_t color);
lv_obj_t *launcher_caps(lv_obj_t *parent, const char *text, uint32_t color);
lv_obj_t *launcher_box(lv_obj_t *parent, int32_t w, int32_t h,
                       uint32_t color, int32_t radius);
lv_obj_t *launcher_square_btn(lv_obj_t *parent, const char *symbol,
                              int32_t size, uint32_t bg, uint32_t fg,
                              lv_event_cb_t cb);
void launcher_count_up(lv_obj_t *label, const char *fmt, int32_t to,
                       uint32_t delay);
void launcher_slide_in(lv_obj_t *obj, int32_t dx, int32_t dy,
                       uint32_t delay);

/* Benchmark screen (launcher_bench.c) */

lv_obj_t *launcher_bench_create(lv_event_cb_t back_cb);
void launcher_bench_frame(uint32_t render_us);

#endif /* __APP_LAUNCHER_LAUNCHER_UI_H */
