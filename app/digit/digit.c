/****************************************************************************
 * app/digit/digit.c
 *
 * Handwritten digit recognition, see digit.h.
 *
 * Screen strokes are 16px wide on a 1080px canvas.  Shrinking the pixels
 * to 28x28 leaves lines far thinner than anything in MNIST, so the stroke
 * points are scaled instead and the lines are drawn again with MNIST
 * width.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <math.h>
#include <string.h>

#include "digit.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The longer side of the stroke skeleton, plus the pen gives about 20px */

#define DIGIT_BOX             17.6f

/* Pen: solid up to 1.2px from the center, fading out at 1.7px */

#define DIGIT_PEN_SOLID       1.2f
#define DIGIT_PEN_EDGE        1.7f

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Squared distance from (px, py) to the segment (ax, ay) - (bx, by) */

static float digit_dist2(float px, float py, float ax, float ay,
                         float bx, float by)
{
  float dx = bx - ax;
  float dy = by - ay;
  float len2 = dx * dx + dy * dy;
  float t = 0.0f;

  if (len2 > 0.0f)
    {
      t = ((px - ax) * dx + (py - ay) * dy) / len2;
      t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    }

  dx = ax + t * dx - px;
  dy = ay + t * dy - py;
  return dx * dx + dy * dy;
}

/* Draw the strokes mapped by grid = (p - c) * scale + off */

static void digit_render(const struct digit_point_s *pts, int npts,
                         float cx, float cy, float scale, float offx,
                         float offy, float img[DIGIT_INPUTS])
{
  float px;
  float py;
  float d;
  int x;
  int y;
  int i;

  for (y = 0; y < DIGIT_SIZE; y++)
    {
      for (x = 0; x < DIGIT_SIZE; x++)
        {
          px = x + 0.5f;
          py = y + 0.5f;
          d = DIGIT_PEN_EDGE * DIGIT_PEN_EDGE;

          for (i = 0; i < npts; i++)
            {
              const struct digit_point_s *a = &pts[i];
              const struct digit_point_s *b = &pts[i];

              if (a->y == DIGIT_PEN_UP)
                {
                  continue;
                }

              /* Single points draw a dot */

              if (i + 1 < npts && pts[i + 1].y != DIGIT_PEN_UP)
                {
                  b = &pts[i + 1];
                }

              d = fminf(d, digit_dist2(px, py,
                                       (a->x - cx) * scale + offx,
                                       (a->y - cy) * scale + offy,
                                       (b->x - cx) * scale + offx,
                                       (b->y - cy) * scale + offy));
            }

          d = (DIGIT_PEN_EDGE - sqrtf(d)) /
              (DIGIT_PEN_EDGE - DIGIT_PEN_SOLID);
          img[y * DIGIT_SIZE + x] = d < 0.0f ? 0.0f : d > 1.0f ? 1.0f : d;
        }
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void digit_preprocess(const struct digit_point_s *pts, int npts,
                      float img[DIGIT_INPUTS])
{
  int16_t x0 = INT16_MAX;
  int16_t y0 = INT16_MAX;
  int16_t x1 = INT16_MIN;
  int16_t y1 = INT16_MIN;
  float scale = 0.0f;
  float sum = 0.0f;
  float mx = 0.0f;
  float my = 0.0f;
  int i;

  for (i = 0; i < npts; i++)
    {
      if (pts[i].y != DIGIT_PEN_UP)
        {
          x0 = pts[i].x < x0 ? pts[i].x : x0;
          x1 = pts[i].x > x1 ? pts[i].x : x1;
          y0 = pts[i].y < y0 ? pts[i].y : y0;
          y1 = pts[i].y > y1 ? pts[i].y : y1;
        }
    }

  memset(img, 0, DIGIT_INPUTS * sizeof(float));
  if (x0 > x1)
    {
      return;
    }

  if (x1 - x0 > 0 || y1 - y0 > 0)
    {
      scale = DIGIT_BOX / (x1 - x0 > y1 - y0 ? x1 - x0 : y1 - y0);
    }

  /* Draw around the box center, then again with the center of mass moved
   * to the middle of the grid as MNIST does.
   */

  digit_render(pts, npts, (x0 + x1) / 2.0f, (y0 + y1) / 2.0f, scale,
               DIGIT_SIZE / 2.0f, DIGIT_SIZE / 2.0f, img);

  for (i = 0; i < DIGIT_INPUTS; i++)
    {
      sum += img[i];
      mx += img[i] * (i % DIGIT_SIZE + 0.5f);
      my += img[i] * (i / DIGIT_SIZE + 0.5f);
    }

  if (sum > 0.0f)
    {
      digit_render(pts, npts, (x0 + x1) / 2.0f, (y0 + y1) / 2.0f, scale,
                   DIGIT_SIZE - mx / sum, DIGIT_SIZE - my / sum, img);
    }
}

int digit_classify(const float img[DIGIT_INPUTS],
                   float prob[DIGIT_CLASSES])
{
  float hidden[DIGIT_HIDDEN];
  const int8_t *w;
  float sum;
  float max;
  int best = 0;
  int i;
  int j;

  for (i = 0; i < DIGIT_HIDDEN; i++)
    {
      w = &g_digit_w1[i * DIGIT_INPUTS];
      sum = 0.0f;

      for (j = 0; j < DIGIT_INPUTS; j++)
        {
          sum += w[j] * img[j];
        }

      sum = sum * g_digit_s1 + g_digit_b1[i];
      hidden[i] = sum > 0.0f ? sum : 0.0f;
    }

  for (i = 0; i < DIGIT_CLASSES; i++)
    {
      w = &g_digit_w2[i * DIGIT_HIDDEN];
      sum = 0.0f;

      for (j = 0; j < DIGIT_HIDDEN; j++)
        {
          sum += w[j] * hidden[j];
        }

      prob[i] = sum * g_digit_s2 + g_digit_b2[i];
      best = prob[i] > prob[best] ? i : best;
    }

  /* Softmax */

  max = prob[best];
  sum = 0.0f;

  for (i = 0; i < DIGIT_CLASSES; i++)
    {
      prob[i] = expf(prob[i] - max);
      sum += prob[i];
    }

  for (i = 0; i < DIGIT_CLASSES; i++)
    {
      prob[i] /= sum;
    }

  return best;
}
