/****************************************************************************
 * tools/preview/stub/nuttx/input/touchscreen.h
 *
 * The touch sample of NuttX, same layout.  The ioctl is not handled by
 * the preview, an app falls back as if the driver did not have it.
 ****************************************************************************/

#ifndef __TOOLS_PREVIEW_STUB_NUTTX_INPUT_TOUCHSCREEN_H
#define __TOOLS_PREVIEW_STUB_NUTTX_INPUT_TOUCHSCREEN_H

#include <stdint.h>

#define TSIOC_GETMAXPOINTS   0x7f0f

#define TOUCH_DOWN           (1 << 0)
#define TOUCH_MOVE           (1 << 1)
#define TOUCH_UP             (1 << 2)
#define TOUCH_ID_VALID       (1 << 3)
#define TOUCH_POS_VALID      (1 << 4)

struct __attribute__((packed)) touch_point_s
{
  uint8_t  id;
  uint8_t  flags;
  int16_t  x;
  int16_t  y;
  int16_t  h;
  int16_t  w;
  uint16_t gesture;
  uint16_t pressure;
  uint16_t dummy;
  uint64_t timestamp;
};

struct __attribute__((packed)) touch_sample_s
{
  int32_t npoints;
  int32_t dummy;
  struct touch_point_s point[1];
};

#define SIZEOF_TOUCH_SAMPLE_S(n) \
  (sizeof(struct touch_sample_s) + ((n) - 1) * sizeof(struct touch_point_s))

#endif /* __TOOLS_PREVIEW_STUB_NUTTX_INPUT_TOUCHSCREEN_H */
