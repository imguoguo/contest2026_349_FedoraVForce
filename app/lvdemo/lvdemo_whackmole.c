/****************************************************************************
 * app/lvdemo/lvdemo_whackmole.c
 *
 * Whack-a-mole of packages/demos/Whackmole, built here instead of as its
 * own app, whose main loop never returns.  Its source is not changed:
 *
 * - Its timers and objects are kept in statics that outlive LVGL in the
 *   flat build; a second start would delete them again.  They are cleared
 *   before each start.
 * - It is made for a 4:3 landscape screen and stretches the background
 *   and holes to the whole screen, which distorts them on the portrait
 *   screen.  The game is put into a 4:3 area across the middle instead,
 *   the holes are placed again by its table for that area, and the START
 *   button gets bigger.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "Whackmole.c"
#include "pic/MiSans_Normal.c"
#include "pic/grassland.c"
#include "pic/hammer_image.c"
#include "pic/mole_image.c"

#include "lvdemo.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LVDEMO_START_W        300
#define LVDEMO_START_H        90
#define LVDEMO_BG             0x2b4a12   /* Dark grass above and below */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void lvdemo_whackmole_create(void)
{
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *obj;
  int32_t w;
  int32_t h;
  uint32_t i;

  game_screen = NULL;
  game_screen_bg = NULL;
  hit_effect = NULL;
  hammer_cursor = NULL;
  game_timer = NULL;
  mole_timer = NULL;
  score = 0;
  game_time = GAME_TIME;

  whackmole_app_create();

  /* 4:3 across the middle, the background is 320 x 240 */

  w = lv_display_get_horizontal_resolution(NULL);
  h = w * grassland.header.h / grassland.header.w;
  lv_obj_set_style_bg_color(scr, lv_color_hex(LVDEMO_BG), 0);
  lv_obj_set_size(game_screen_bg, w, h);
  lv_obj_set_size(game_screen, w, h);

  for (i = 0; i < MOLE_COUNT; i++)
    {
      lv_obj_set_pos(lv_obj_get_parent(moles[i]),
                     hole_positions[i].x_pct * w / 100,
                     hole_positions[i].y_pct * h / 100);
    }

  /* The hammer follows the finger in screen coordinates */

  lv_obj_set_parent(hammer_cursor, scr);

  for (i = 0; i < lv_obj_get_child_count(game_screen); i++)
    {
      obj = lv_obj_get_child(game_screen, i);
      if (lv_obj_check_type(obj, &lv_button_class))
        {
          lv_obj_set_size(obj, LVDEMO_START_W, LVDEMO_START_H);
        }
    }
}
