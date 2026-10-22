/****************************************************************************
 * app/paint/tools/digit_test.c
 *
 * Host test for digit.c:
 *   1. MNIST test set through digit_classify(), must match train.py
 *   2. Synthetic touch strokes of 0-9 through digit_preprocess(), the way
 *      paint feeds the model on the board
 *
 *   cc -O2 -I.. -o digit_test digit_test.c ../digit.c ../digit_model.c \
 *      -lz -lm
 *   ./digit_test <mnist dir>
 ****************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "digit.h"

#define SAMPLES     50
#define MAX_POINTS  2048
#define PI          3.14159265f

/* Digits as pen paths in a unit box, -1 ends a stroke, -2 the digit */

struct seg_s
{
  float x;
  float y;
};

static const struct seg_s g_shapes[10][16] =
{
  { {.5, 0}, {.15, .2}, {.08, .55}, {.25, .92}, {.55, 1}, {.85, .8},
    {.92, .4}, {.75, .08}, {.45, .02}, {-2, -2} },
  { {.35, .15}, {.55, 0}, {.55, 1}, {-2, -2} },
  { {.12, .25}, {.35, .02}, {.7, .02}, {.88, .25}, {.75, .5}, {.1, 1},
    {.92, 1}, {-2, -2} },
  { {.12, .1}, {.5, 0}, {.85, .15}, {.8, .4}, {.42, .5}, {.85, .6},
    {.9, .85}, {.5, 1}, {.1, .9}, {-2, -2} },
  { {.6, 0}, {.08, .65}, {.95, .65}, {-1, -1}, {.68, .3}, {.68, 1},
    {-2, -2} },
  { {.85, 0}, {.22, 0}, {.15, .45}, {.55, .38}, {.86, .58}, {.82, .88},
    {.45, 1}, {.1, .9}, {-2, -2} },
  { {.75, 0}, {.35, .3}, {.15, .65}, {.25, .95}, {.6, 1}, {.85, .8},
    {.75, .55}, {.4, .5}, {.18, .65}, {-2, -2} },
  { {.08, 0}, {.92, 0}, {.4, 1}, {-2, -2} },
  { {.75, .12}, {.5, 0}, {.22, .12}, {.25, .35}, {.75, .62}, {.82, .88},
    {.5, 1}, {.18, .88}, {.25, .62}, {.75, .35}, {.75, .12}, {-2, -2} },
  { {.85, .25}, {.55, .45}, {.2, .32}, {.3, .05}, {.7, 0}, {.85, .25},
    {.8, .6}, {.7, 1}, {-2, -2} },
};

static float frand(float lo, float hi)
{
  return lo + (hi - lo) * rand() / (float)RAND_MAX;
}

/* Sample a digit the way the touch panel reports a moving finger:
 * random size, aspect, slant and tremble, a point every 6-20px.
 */

static int synth(int digit, struct digit_point_s *pts)
{
  const struct seg_s *s = g_shapes[digit];
  float h = frand(300, 900);
  float w = h * frand(.5f, .8f);
  float slant = frand(-.25f, .25f);
  float ox = frand(100, 1080 - 100 - w);
  float oy = frand(200, 1700 - h);
  float x;
  float y;
  float len;
  float t;
  float step;
  int n = 0;
  int i;

  for (i = 0; s[i].x > -2; i++)
    {
      if (s[i].x < 0)
        {
          pts[n++].y = DIGIT_PEN_UP;
          continue;
        }

      if (s[i + 1].x < 0)
        {
          pts[n].x = ox + (s[i].x + slant * (1 - s[i].y)) * w;
          pts[n].y = oy + s[i].y * h;
          n++;
          continue;
        }

      len = hypotf((s[i + 1].x - s[i].x) * w, (s[i + 1].y - s[i].y) * h);
      for (t = 0; t < len && n < MAX_POINTS - 2; t += step)
        {
          step = frand(6, 20);
          x = s[i].x + (s[i + 1].x - s[i].x) * t / len;
          y = s[i].y + (s[i + 1].y - s[i].y) * t / len;
          pts[n].x = ox + (x + slant * (1 - y)) * w + frand(-4, 4);
          pts[n].y = oy + y * h + frand(-4, 4);
          n++;
        }
    }

  pts[n++].y = DIGIT_PEN_UP;
  return n;
}

static void show(const float img[DIGIT_INPUTS])
{
  int x;
  int y;

  for (y = 0; y < DIGIT_SIZE; y++)
    {
      for (x = 0; x < DIGIT_SIZE; x++)
        {
          putchar(" .:-=+*#%@"[(int)(img[y * DIGIT_SIZE + x] * 9.99f)]);
        }

      putchar('\n');
    }
}

static unsigned char *load(const char *dir, const char *name, int offset,
                           int size)
{
  unsigned char *buf = malloc(offset + size);
  char path[512];
  gzFile f;

  snprintf(path, sizeof(path), "%s/%s", dir, name);
  f = gzopen(path, "rb");
  if (f == NULL || gzread(f, buf, offset + size) != offset + size)
    {
      fprintf(stderr, "cannot read %s\n", path);
      exit(1);
    }

  gzclose(f);
  return buf;
}

int main(int argc, char *argv[])
{
  static struct digit_point_s pts[MAX_POINTS];
  float img[DIGIT_INPUTS];
  float prob[DIGIT_CLASSES];
  unsigned char *images;
  unsigned char *labels;
  int correct = 0;
  int total = 0;
  int digit;
  int i;
  int j;

  if (argc < 2)
    {
      fprintf(stderr, "usage: %s <mnist dir>\n", argv[0]);
      return 1;
    }

  images = load(argv[1], "t10k-images-idx3-ubyte.gz", 16,
                10000 * DIGIT_INPUTS);
  labels = load(argv[1], "t10k-labels-idx1-ubyte.gz", 8, 10000);

  for (i = 0; i < 10000; i++)
    {
      for (j = 0; j < DIGIT_INPUTS; j++)
        {
          img[j] = images[16 + i * DIGIT_INPUTS + j] / 255.0f;
        }

      correct += digit_classify(img, prob) == labels[8 + i];
    }

  printf("MNIST test set: %.2f%%\n\n", correct / 100.0f);

  srand(349);
  for (digit = 0; digit < 10; digit++)
    {
      correct = 0;
      for (i = 0; i < SAMPLES; i++)
        {
          digit_preprocess(pts, synth(digit, pts), img);
          j = digit_classify(img, prob);
          correct += j == digit;
          if (i == 0)
            {
              printf("digit %d -> %d (%.1f%%)\n", digit, j, prob[j] * 100);
              show(img);
            }
        }

      printf("digit %d: %d/%d\n\n", digit, correct, SAMPLES);
      total += correct;
    }

  printf("synthetic strokes: %d/%d\n", total, 10 * SAMPLES);
  return 0;
}
