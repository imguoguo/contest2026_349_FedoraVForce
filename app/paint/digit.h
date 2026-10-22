/****************************************************************************
 * app/paint/digit.h
 *
 * Handwritten digit recognition: strokes -> 28x28 MNIST style image ->
 * 784-128-10 MLP.  The MLP runs either on TFLite Micro (digit_tflm.cc) or
 * on the plain C inference kept for comparison (digit.c), which
 * tools/digit_test.c also runs on the host.
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
#define DIGIT_SAMPLES         500

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

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* Plain C model: int8 weights, one scale per layer */

extern const float g_digit_s1;
extern const float g_digit_s2;
extern const int8_t g_digit_w1[DIGIT_HIDDEN * DIGIT_INPUTS];
extern const float g_digit_b1[DIGIT_HIDDEN];
extern const int8_t g_digit_w2[DIGIT_CLASSES * DIGIT_HIDDEN];
extern const float g_digit_b2[DIGIT_CLASSES];

/* Fully int8 TFLite model */

extern const uint8_t g_digit_tflm_model[];

/* MNIST test samples for comparing the two */

extern const uint8_t g_digit_sample_images[DIGIT_SAMPLES][DIGIT_INPUTS];
extern const uint8_t g_digit_sample_labels[DIGIT_SAMPLES];

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Render strokes into a 0..1 gray image the way MNIST digits look */

void digit_preprocess(const struct digit_point_s *pts, int npts,
                      float img[DIGIT_INPUTS]);

/* Return the most likely digit, prob gets the softmax output */

int digit_classify(const float img[DIGIT_INPUTS],
                   float prob[DIGIT_CLASSES]);

/* Same on TFLite Micro, returns -1 if the interpreter cannot be set up */

int digit_tflm_classify(const float img[DIGIT_INPUTS],
                        float prob[DIGIT_CLASSES]);

#ifdef __cplusplus
}
#endif

#endif /* __APP_PAINT_DIGIT_H */
