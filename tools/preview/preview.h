/****************************************************************************
 * tools/preview/preview.h
 *
 * Host stand-ins for the NuttX pieces the apps use, and the API scenes use
 * to drive an app: virtual time, scripted touch and PNG screenshots.
 * Force included (-include) into every app source.
 ****************************************************************************/

#ifndef __TOOLS_PREVIEW_PREVIEW_H
#define __TOOLS_PREVIEW_PREVIEW_H

/* Apps read the uptime through clock_gettime(), give them virtual time */

#define clock_gettime         preview_clock_gettime

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <lvgl/lvgl.h>

#define FAR
#define UNUSED(x)             ((void)(x))
#ifndef nitems
#  define nitems(a)           (sizeof(a) / sizeof((a)[0]))
#endif

/* Uptime and heap shown in docs/launcher-v2/images */

#define PREVIEW_UPTIME        (5 * 3600 + 28 * 60 + 27)
#define PREVIEW_HEAP_TOTAL    (32768 * 1024)
#define PREVIEW_HEAP_FREE     (23552 * 1024)

typedef struct
{
  const char *fb_path;
  const char *input_path;
  const char *utouch_path;
  const char *mouse_path;
  const char *trace_path;
} lv_nuttx_dsc_t;

typedef struct
{
  lv_display_t *disp;
  lv_indev_t *indev;
  lv_indev_t *utouch_indev;
  lv_indev_t *mouse_indev;
} lv_nuttx_result_t;

void lv_nuttx_dsc_init(lv_nuttx_dsc_t *dsc);
void lv_nuttx_init(const lv_nuttx_dsc_t *dsc, lv_nuttx_result_t *result);
void lv_nuttx_deinit(lv_nuttx_result_t *result);

int preview_clock_gettime(clockid_t id, struct timespec *ts);

/* Scene API */

uint32_t preview_now(void);
void preview_step(uint32_t ms);
void preview_touch(int32_t x, int32_t y, bool pressed);
void preview_touch_points(int n, const lv_point_t *pts);
void preview_drag(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                  uint32_t ms);
void preview_shot(const char *name);

#endif /* __TOOLS_PREVIEW_PREVIEW_H */
