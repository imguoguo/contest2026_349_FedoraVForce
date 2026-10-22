/****************************************************************************
 * app/paint/digit.h
 *
 * Handwritten digit recognition: strokes -> 28x28 MNIST style image ->
 * 784-128-10 MLP.  Plain C, so tools/digit_test.c runs it on the host.
 ****************************************************************************/

#ifndef __APP_PAINT_DIGIT_H
#define __APP_PAINT_DIGIT_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DIGIT_SIZE            28
#define DIGIT_INPUTS          (DIGIT_SIZE * DIGIT_SIZE)
#define DIGIT_HIDDEN          128
#define DIGIT_CLASSES         10

/* Point that ends a stroke */

#define DIGIT_PEN_UP          INT16_MIN

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct digit_point_s
{
  int16_t x;
  int16_t y;                /* DIGIT_PEN_UP between strokes */
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

extern const float g_digit_s1;
extern const float g_digit_s2;
extern const int8_t g_digit_w1[DIGIT_HIDDEN * DIGIT_INPUTS];
extern const float g_digit_b1[DIGIT_HIDDEN];
extern const int8_t g_digit_w2[DIGIT_CLASSES * DIGIT_HIDDEN];
extern const float g_digit_b2[DIGIT_CLASSES];

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Render strokes into a 0..1 gray image the way MNIST digits look */

void digit_preprocess(const struct digit_point_s *pts, int npts,
                      float img[DIGIT_INPUTS]);

/* Return the most likely digit, prob gets the softmax output */

int digit_classify(const float img[DIGIT_INPUTS],
                   float prob[DIGIT_CLASSES]);

#endif /* __APP_PAINT_DIGIT_H */
