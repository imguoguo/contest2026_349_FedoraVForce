/****************************************************************************
 * app/paint/digit_tflm.cc
 *
 * Digit recognition on TFLite Micro, the C API is in digit.h.
 *
 * The interpreter lives in static storage and is built on first use.  In
 * the flat build the statics of an app keep their value from one run to
 * the next, so it is built once per boot and nothing leaks.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <new>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "digit.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Input, hidden and output tensors plus the interpreter's own records */

#define DIGIT_TFLM_ARENA      (16 * 1024)

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef tflite::MicroMutableOpResolver<2> digit_resolver_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

alignas(16) static uint8_t g_arena[DIGIT_TFLM_ARENA];
alignas(digit_resolver_t)
static uint8_t g_resolver[sizeof(digit_resolver_t)];
alignas(tflite::MicroInterpreter)
static uint8_t g_interp_buf[sizeof(tflite::MicroInterpreter)];

static tflite::MicroInterpreter *g_interp;
static bool g_failed;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static tflite::MicroInterpreter *digit_tflm_get(void)
{
  digit_resolver_t *resolver;

  if (g_interp != nullptr || g_failed)
    {
      return g_interp;
    }

  /* The model only has fully connected (with ReLU fused) and softmax */

  resolver = new (g_resolver) digit_resolver_t();
  resolver->AddFullyConnected();
  resolver->AddSoftmax();

  g_interp = new (g_interp_buf)
    tflite::MicroInterpreter(tflite::GetModel(g_digit_tflm_model),
                             *resolver, g_arena, sizeof(g_arena));

  if (g_interp->AllocateTensors() != kTfLiteOk ||
      g_interp->input(0)->type != kTfLiteInt8 ||
      g_interp->output(0)->type != kTfLiteInt8)
    {
      g_interp = nullptr;
      g_failed = true;
    }

  return g_interp;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

extern "C" int digit_tflm_classify(const float img[DIGIT_INPUTS],
                                   float prob[DIGIT_CLASSES])
{
  tflite::MicroInterpreter *interp = digit_tflm_get();
  TfLiteTensor *in;
  TfLiteTensor *out;
  int best = 0;
  int q;
  int i;

  if (interp == nullptr)
    {
      return -1;
    }

  /* Quantize the 0..1 image the way the converter calibrated it */

  in = interp->input(0);
  for (i = 0; i < DIGIT_INPUTS; i++)
    {
      q = (int)(img[i] / in->params.scale + 0.5f) + in->params.zero_point;
      in->data.int8[i] = q < -128 ? -128 : q > 127 ? 127 : q;
    }

  if (interp->Invoke() != kTfLiteOk)
    {
      return -1;
    }

  out = interp->output(0);
  for (i = 0; i < DIGIT_CLASSES; i++)
    {
      prob[i] = (out->data.int8[i] - out->params.zero_point) *
                out->params.scale;
      best = prob[i] > prob[best] ? i : best;
    }

  return best;
}
