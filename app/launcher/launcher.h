/****************************************************************************
 * app/launcher/launcher.h
 *
 * Contest 2026 team 349 (FedoraVForce) LVGL launcher demo
 ****************************************************************************/

#ifndef __APP_LAUNCHER_LAUNCHER_H
#define __APP_LAUNCHER_LAUNCHER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LAUNCHER_DIGIT_HIDDEN   128
#define LAUNCHER_DIGIT_SIDE     28
#define LAUNCHER_DIGIT_PIXELS   (LAUNCHER_DIGIT_SIDE * LAUNCHER_DIGIT_SIDE)

/* Stroke separator in the recorded point list */

#define LAUNCHER_PEN_UP         INT16_MIN

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct launcher_pt_s
{
  int16_t x;
  int16_t y;
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

extern const float g_digit_w1_scale;
extern const float g_digit_w2_scale;
extern const int8_t g_digit_w1[];
extern const float g_digit_b1[];
extern const int8_t g_digit_w2[];
extern const float g_digit_b2[];

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Render the recorded strokes into a 28x28 MNIST-style image (0..1, white
 * ink on black, centre of mass at the middle) and classify it.
 * pts holds points with LAUNCHER_PEN_UP between strokes.
 * Returns the most likely digit, or -1 when there is no ink.
 */

int launcher_digit_classify(const struct launcher_pt_s *pts, int npts,
                            float prob[10], float image[]);

#endif /* __APP_LAUNCHER_LAUNCHER_H */
