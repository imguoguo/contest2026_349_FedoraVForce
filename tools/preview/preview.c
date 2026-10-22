/****************************************************************************
 * tools/preview/preview.c
 *
 * Offscreen 1080x1920 RGB565 display in direct mode, the same render path
 * as lv_nuttx_fbdev on the board, plus the scene API of preview.h.
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include <malloc.h>
#include <nuttx/arch.h>
#include <nuttx/lib/builtin.h>

#include "preview.h"

#define W 1080
#define H 1920

static uint16_t g_fb[W * H];
static uint32_t g_ms;
static lv_point_t g_point;
static bool g_pressed;

/* Apps the launcher finds as builtins */

static int preview_app_main(int argc, char *argv[])
{
  return 0;
}

static const struct builtin_s g_builtins[] =
{
  { "paint",   100, 16384, preview_app_main },
  { "bench",   100, 16384, preview_app_main },
  { "monitor", 100, 16384, preview_app_main },
  { "touch",   100, 16384, preview_app_main },
  { "lvdemo",  100, 16384, preview_app_main },
};

static uint32_t preview_tick(void)
{
  return g_ms;
}

static void preview_flush(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px)
{
  lv_display_flush_ready(disp);
}

static void preview_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  data->point = g_point;
  data->state = g_pressed ? LV_INDEV_STATE_PRESSED :
                            LV_INDEV_STATE_RELEASED;
}

static void png_chunk(FILE *f, const char *type, const uint8_t *data,
                      uint32_t len)
{
  uint8_t be[4] =
    {
      len >> 24, len >> 16, len >> 8, len
    };

  uint32_t crc = crc32(crc32(0, (const uint8_t *)type, 4), data, len);

  fwrite(be, 1, 4, f);
  fwrite(type, 1, 4, f);
  fwrite(data, 1, len, f);
  be[0] = crc >> 24;
  be[1] = crc >> 16;
  be[2] = crc >> 8;
  be[3] = crc;
  fwrite(be, 1, 4, f);
}

/****************************************************************************
 * NuttX stand-ins
 ****************************************************************************/

void lv_nuttx_dsc_init(lv_nuttx_dsc_t *dsc)
{
  memset(dsc, 0, sizeof(*dsc));
}

void lv_nuttx_init(const lv_nuttx_dsc_t *dsc, lv_nuttx_result_t *result)
{
  memset(result, 0, sizeof(*result));
  lv_tick_set_cb(preview_tick);

  result->disp = lv_display_create(W, H);
  lv_display_set_color_format(result->disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(result->disp, g_fb, NULL, sizeof(g_fb),
                         LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(result->disp, preview_flush);

  result->indev = lv_indev_create();
  lv_indev_set_type(result->indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(result->indev, preview_read);
}

void lv_nuttx_deinit(lv_nuttx_result_t *result)
{
  lv_indev_delete(result->indev);
  lv_display_delete(result->disp);
}

int preview_clock_gettime(clockid_t id, struct timespec *ts)
{
  ts->tv_sec = PREVIEW_UPTIME + g_ms / 1000;
  ts->tv_nsec = g_ms % 1000 * 1000000;
  return 0;
}

clock_t up_perf_gettime(void)
{
  return g_ms * 1000;
}

unsigned long up_perf_getfreq(void)
{
  return 1000000;
}

struct mallinfo mallinfo(void)
{
  struct mallinfo mi =
    {
      .arena = PREVIEW_HEAP_TOTAL,
      .fordblks = PREVIEW_HEAP_FREE,
      .mxordblk = PREVIEW_HEAP_FREE / 2,
    };

  return mi;
}

void *memalign(size_t align, size_t size)
{
  void *p = NULL;

  return posix_memalign(&p, align, size) == 0 ? p : NULL;
}

int builtin_isavail(const char *name)
{
  int i;

  for (i = 0; i < (int)nitems(g_builtins); i++)
    {
      if (strcmp(g_builtins[i].name, name) == 0)
        {
          return i;
        }
    }

  return -1;
}

const char *builtin_getname(int index)
{
  return g_builtins[index].name;
}

const struct builtin_s *builtin_for_index(int index)
{
  return &g_builtins[index];
}

int task_create(const char *name, int priority, int stack_size,
                main_t entry, char * const argv[])
{
  return -1;
}

/****************************************************************************
 * Scene API
 ****************************************************************************/

uint32_t preview_now(void)
{
  return g_ms;
}

void preview_step(uint32_t ms)
{
  while (ms-- > 0)
    {
      g_ms++;
      lv_timer_handler();
    }
}

void preview_touch(int32_t x, int32_t y, bool pressed)
{
  g_point.x = x;
  g_point.y = y;
  g_pressed = pressed;
}

/* Move a pressed finger in a straight line, one point per 10ms */

void preview_drag(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                  uint32_t ms)
{
  uint32_t t;

  for (t = 0; t <= ms; t += 10)
    {
      preview_touch(x0 + (x1 - x0) * (int32_t)t / (int32_t)ms,
                    y0 + (y1 - y0) * (int32_t)t / (int32_t)ms, true);
      preview_step(10);
    }
}

/* Render whatever is pending and save the frame buffer as out/<name> */

void preview_shot(const char *name)
{
  static uint8_t raw[H * (1 + 3 * W)];
  static uint8_t zbuf[H * (1 + 3 * W) + 1024];
  uLongf zlen = sizeof(zbuf);
  uint8_t ihdr[13] =
    {
      W >> 24, W >> 16, W >> 8, W & 0xff,
      H >> 24, H >> 16, H >> 8, H & 0xff, 8, 2, 0, 0, 0
    };

  char path[256];
  uint8_t *p = raw;
  uint16_t c;
  FILE *f;
  int x;
  int y;

  lv_refr_now(NULL);

  for (y = 0; y < H; y++)
    {
      *p++ = 0;
      for (x = 0; x < W; x++)
        {
          c = g_fb[y * W + x];
          *p++ = (c >> 11) * 255 / 31;
          *p++ = (c >> 5 & 0x3f) * 255 / 63;
          *p++ = (c & 0x1f) * 255 / 31;
        }
    }

  compress2(zbuf, &zlen, raw, sizeof(raw), 6);

  snprintf(path, sizeof(path), "out/%s", name);
  f = fopen(path, "wb");
  if (f == NULL)
    {
      perror(path);
      exit(1);
    }

  fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
  png_chunk(f, "IHDR", ihdr, sizeof(ihdr));
  png_chunk(f, "IDAT", zbuf, zlen);
  png_chunk(f, "IEND", NULL, 0);
  fclose(f);
  printf("%s\n", path);
}
