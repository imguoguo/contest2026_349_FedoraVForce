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
 * - Its text is in a 16px font, made for a small screen, which it sets
 *   again on every start.  Our fonts are set for LV_STATE_USER_1, which
 *   wins over its default state styles.  The game over box gets bigger.
 * - The hammer swings by a transform_rotation style, which LVGL 9.1 draws
 *   through a transformed layer.  Now and then that layer's draw task is
 *   never taken by the SW draw unit and the refresh spins forever (seen on
 *   the board and in tools/preview).  The hammer image is rotated instead,
 *   which needs no layer.
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
#include "lvx_ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LVDEMO_START_W        300
#define LVDEMO_START_H        90
#define LVDEMO_BG             0x2b4a12   /* Dark grass above and below */
#define LVDEMO_SWING          300        /* 30 degrees */
#define LVDEMO_MSGBOX_W       720
#define LVDEMO_MSGBOX_H       360

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void lvdemo_font(lv_obj_t *obj, const lv_font_t *font)
{
  lv_obj_set_style_text_font(obj, font, LV_STATE_USER_1);
  lv_obj_add_state(obj, LV_STATE_USER_1);
}

static void lvdemo_swing_cb(lv_event_t *e)
{
  lv_image_set_rotation(hammer_cursor,
                        lv_event_get_code(e) == LV_EVENT_PRESSED ?
                        LVDEMO_SWING : 0);
}

static void lvdemo_swing(lv_obj_t *obj)
{
  lv_obj_add_event_cb(obj, lvdemo_swing_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(obj, lvdemo_swing_cb, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(obj, lvdemo_swing_cb, LV_EVENT_PRESS_LOST, NULL);
}

/* The game over box is made at the end of each game, 300 x 120, its
 * buttons after it.  LV_EVENT_CHILD_CREATED bubbles up to the screen for
 * all of them.  Montserrat has the close symbol, Outfit has not.
 */

static void lvdemo_child_cb(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_param(e);

  if (obj == NULL)
    {
      return;
    }

  if (lv_obj_check_type(obj, &lv_msgbox_class))
    {
      lv_obj_set_style_min_width(obj, LVDEMO_MSGBOX_W, 0);
      lv_obj_set_style_min_height(obj, LVDEMO_MSGBOX_H, 0);
      lv_obj_set_style_text_font(obj, UI_ICON_24, 0);
    }
  else if (lv_obj_check_type(obj, &lv_msgbox_footer_button_class))
    {
      lv_obj_set_style_min_width(obj, LVDEMO_START_W, 0);
      lv_obj_set_style_min_height(obj, LVDEMO_START_H, 0);
      lvdemo_font(obj, UI_ICON_24);
    }
}

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
  lv_obj_set_style_transform_rotation(hammer_cursor, 0, LV_STATE_PRESSED);
  lv_obj_remove_local_style_prop(hammer_cursor, LV_STYLE_TRANSITION,
                                 LV_STATE_DEFAULT);
  lv_obj_remove_local_style_prop(hammer_cursor, LV_STYLE_TRANSITION,
                                 LV_STATE_PRESSED);
  lv_image_set_pivot(hammer_cursor, hammer_image.header.w / 2,
                     hammer_image.header.h / 2);
  lv_obj_add_event_cb(scr, lvdemo_child_cb, LV_EVENT_CHILD_CREATED, NULL);
  lvdemo_swing(game_screen);
  for (i = 0; i < MOLE_COUNT; i++)
    {
      lvdemo_swing(moles[i]);
    }

  for (i = 0; i < lv_obj_get_child_count(game_screen); i++)
    {
      obj = lv_obj_get_child(game_screen, i);
      if (lv_obj_check_type(obj, &lv_button_class))
        {
          lv_obj_set_size(obj, LVDEMO_START_W, LVDEMO_START_H);
          lvdemo_font(lv_obj_get_child(obj, 0), UI_OUTFIT_34);
        }
      else if (lv_obj_check_type(obj, &lv_label_class))
        {
          lvdemo_font(obj, UI_OUTFIT_34);
        }
    }
}
