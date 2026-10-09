/****************************************************************************
 * app/launcher/launcher_digits.c
 *
 * On-device handwritten digit recognition for the paint screen.
 *
 * Touch strokes are 16px wide on a 1080x1920 canvas.  Shrinking those
 * pixels to 28x28 would leave sub-pixel lines the model never saw, so the
 * recorded stroke points are redrawn at MNIST stroke width instead.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <math.h>
#include <string.h>

#include "launcher.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Half stroke width plus antialias edge, and the box the stroke centre
 * lines are scaled into so that the inked digit fits MNIST's 20x20
 */

#define DIGIT_RADIUS    1.7f
#define DIGIT_BOX       (20.0f - 2 * DIGIT_RADIUS)

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Squared distance from (px, py) to segment (ax, ay)-(bx, by) */

static float digit_seg_dist2(float px, float py, float ax, float ay,
                             float bx, float by)
{
  float dx = bx - ax;
  float dy = by - ay;
  float len2 = dx * dx + dy * dy;
  float t = 0.0f;

  if (len2 > 0.0f)
    {
      t = ((px - ax) * dx + (py - ay) * dy) / len2;
      t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }

  dx = ax + t * dx - px;
  dy = ay + t * dy - py;
  return dx * dx + dy * dy;
}

/* Draw the strokes into a 28x28 image with the given scale and offset,
 * returning the ink's centre of mass.
 */

static void digit_draw(const struct launcher_pt_s *pts, int npts, float sc,
                       float ox, float oy, float image[], float *cx,
                       float *cy)
{
  float sum = 0.0f;
  int i;
  int x;
  int y;

  *cx = 0.0f;
  *cy = 0.0f;

  for (y = 0; y < LAUNCHER_DIGIT_SIDE; y++)
    {
      for (x = 0; x < LAUNCHER_DIGIT_SIDE; x++)
        {
          float px = x + 0.5f;
          float py = y + 0.5f;
          float best = 1e9f;
          float v;

          for (i = 0; i < npts; i++)
            {
              const struct launcher_pt_s *a = &pts[i];
              const struct launcher_pt_s *b = a;

              if (a->x == LAUNCHER_PEN_UP)
                {
                  continue;
                }

              if (i + 1 < npts && pts[i + 1].x != LAUNCHER_PEN_UP)
                {
                  b = &pts[i + 1];
                }

              best = fminf(best, digit_seg_dist2(px, py,
                                                 a->x * sc + ox,
                                                 a->y * sc + oy,
                                                 b->x * sc + ox,
                                                 b->y * sc + oy));
            }

          v = DIGIT_RADIUS - sqrtf(best);
          v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
          image[y * LAUNCHER_DIGIT_SIDE + x] = v;
          sum += v;
          *cx += v * x;
          *cy += v * y;
        }
    }

  if (sum > 0.0f)
    {
      *cx /= sum;
      *cy /= sum;
    }
}

/* MNIST-style placement: fit the ink into 20x20, then move it so its
 * centre of mass lands in the middle (redrawn, so nothing is clipped).
 */

static void digit_render(const struct launcher_pt_s *pts, int npts,
                         float image[])
{
  float x0 = 1e9f;
  float y0 = 1e9f;
  float x1 = -1e9f;
  float y1 = -1e9f;
  float sc;
  float ox;
  float oy;
  float cx;
  float cy;
  int i;

  for (i = 0; i < npts; i++)
    {
      if (pts[i].x != LAUNCHER_PEN_UP)
        {
          x0 = fminf(x0, pts[i].x);
          y0 = fminf(y0, pts[i].y);
          x1 = fmaxf(x1, pts[i].x);
          y1 = fmaxf(y1, pts[i].y);
        }
    }

  sc = DIGIT_BOX / fmaxf(fmaxf(x1 - x0, y1 - y0), 1.0f);
  ox = LAUNCHER_DIGIT_SIDE / 2 - (x0 + x1) / 2 * sc;
  oy = LAUNCHER_DIGIT_SIDE / 2 - (y0 + y1) / 2 * sc;

  digit_draw(pts, npts, sc, ox, oy, image, &cx, &cy);
  digit_draw(pts, npts, sc, ox + 13.5f - cx, oy + 13.5f - cy, image,
             &cx, &cy);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int launcher_digit_classify(const struct launcher_pt_s *pts, int npts,
                            float prob[10], float image[])
{
  float hidden[LAUNCHER_DIGIT_HIDDEN];
  float total = 0.0f;
  float peak;
  int best = 0;
  int i;
  int j;

  for (i = 0; i < npts && pts[i].x == LAUNCHER_PEN_UP; i++);
  if (i == npts)
    {
      return -1;
    }

  digit_render(pts, npts, image);

  memcpy(hidden, g_digit_b1, sizeof(hidden));
  for (i = 0; i < LAUNCHER_DIGIT_PIXELS; i++)
    {
      const int8_t *row = &g_digit_w1[i * LAUNCHER_DIGIT_HIDDEN];
      float v = image[i] * g_digit_w1_scale;

      if (v == 0.0f)
        {
          continue;
        }

      for (j = 0; j < LAUNCHER_DIGIT_HIDDEN; j++)
        {
          hidden[j] += v * row[j];
        }
    }

  memcpy(prob, g_digit_b2, sizeof(float) * 10);
  for (j = 0; j < LAUNCHER_DIGIT_HIDDEN; j++)
    {
      float v = fmaxf(hidden[j], 0.0f) * g_digit_w2_scale;

      for (i = 0; i < 10; i++)
        {
          prob[i] += v * g_digit_w2[j * 10 + i];
        }
    }

  /* Softmax */

  peak = prob[0];
  for (i = 1; i < 10; i++)
    {
      peak = fmaxf(peak, prob[i]);
    }

  for (i = 0; i < 10; i++)
    {
      prob[i] = expf(prob[i] - peak);
      total += prob[i];
    }

  for (i = 0; i < 10; i++)
    {
      prob[i] /= total;
      if (prob[i] > prob[best])
        {
          best = i;
        }
    }

  return best;
}
