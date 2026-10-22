/****************************************************************************
 * tools/preview/scene_paint.c
 *
 * Empty canvas, a hand drawn 7 with the result card sliding in and
 * settled, as in docs/launcher-v2/images/4*.png.
 ****************************************************************************/

#define main paint_main
#include "../../app/paint/paint_main.c"
#undef main

static void tap(int32_t x, int32_t y)
{
  preview_touch(x, y, true);
  preview_step(60);
  preview_touch(x, y, false);
  preview_step(30);
}

static void stroke(const lv_point_t *p, int n)
{
  int i;

  preview_touch(p[0].x, p[0].y, true);
  preview_step(30);
  for (i = 1; i < n; i++)
    {
      preview_drag(p[i - 1].x, p[i - 1].y, p[i].x, p[i].y, 150);
    }

  preview_touch(p[n - 1].x, p[n - 1].y, false);
  preview_step(30);
}

int main(void)
{
  static const lv_point_t seven[] =
  {
    { 378, 520 }, { 722, 520 }, { 470, 1300 }
  };

  lv_nuttx_result_t result;

  g_canvas_buf = memalign(64, PAINT_BUF_SIZE);
  g_input_buf = malloc(PAINT_INPUT_SIZE * PAINT_INPUT_SIZE *
                       sizeof(uint16_t));
  paint_start(&result);
  preview_step(300);
  tap(924, 1756);                       /* Recognition on */
  preview_shot("4a-paint-empty.png");

  stroke(seven, nitems(seven));
  preview_step(PAINT_RECOGNIZE_DELAY + 150);
  preview_shot("4b-paint-result-moving.png");
  preview_step(600);
  preview_shot("4-paint.png");

  lv_nuttx_deinit(&result);
  lv_deinit();
  return 0;
}
