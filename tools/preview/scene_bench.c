/****************************************************************************
 * tools/preview/scene_bench.c
 *
 * Bench before a run, during scenes 1 and 3 and with the results.  The
 * CPU tests spin on the clock, which stands still here, so they are not
 * run; the digit samples go through the plain C inference to check them.
 ****************************************************************************/

#define main bench_main
#include "../../app/bench/bench_main.c"
#undef main

static void tap(int32_t x, int32_t y)
{
  preview_touch(x, y, true);
  preview_step(60);
  preview_touch(x, y, false);
}

int main(void)
{
  lv_nuttx_result_t result;
  uint32_t us;

  bench_start(&result);
  preview_step(500);
  preview_shot("5a-bench-idle.png");

  tap(540, 1730);
  preview_step(1000);
  preview_shot("5b-bench-scene1.png");
  preview_step(2 * BENCH_SCENE_TIME);
  preview_shot("5c-bench-scene3.png");
  preview_step(3 * BENCH_SCENE_TIME);
  preview_shot("5-bench.png");

  tap(300, 1460);
  preview_step(500);
  preview_shot("5d-bench-cpu.png");

  printf("digit samples, plain C: %d / %d right\n",
         bench_digit(&g_digits[nitems(g_digits) - 1], &us), DIGIT_SAMPLES);

  lv_nuttx_deinit(&result);
  lv_deinit();
  return 0;
}
