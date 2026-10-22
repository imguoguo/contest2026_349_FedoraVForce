/****************************************************************************
 * app/ui/lvx_ui.c
 *
 * Shared widgets of the LogicPi A1 desktop, see lvx_ui.h.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "lvx_ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UI_TAG_SPACE          4
#define UI_NUM_MAX            16

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void ui_translate_x_cb(void *obj, int32_t v)
{
  lv_obj_set_style_translate_x(obj, v, 0);
}

static void ui_translate_y_cb(void *obj, int32_t v)
{
  lv_obj_set_style_translate_y(obj, v, 0);
}

/* A ui_num is a row of slots, one per character.  A slot clips its
 * labels, so a changed character rolls in from below while the old one
 * leaves at the top.  Digits share the width of the widest one, so the
 * number does not shift while it counts.
 */

static int32_t ui_num_width(const lv_font_t *font, char c)
{
  int32_t w = 0;
  char d;

  if (c < '0' || c > '9')
    {
      return lv_font_get_glyph_width(font, c, 0);
    }

  for (d = '0'; d <= '9'; d++)
    {
      w = LV_MAX(w, lv_font_get_glyph_width(font, d, 0));
    }

  return w;
}

static lv_obj_t *ui_num_char(lv_obj_t *slot, char c)
{
  char text[2] =
    {
      c, '\0'
    };

  lv_obj_t *label = lv_label_create(slot);

  lv_label_set_text(label, text);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(label, lv_pct(100));
  return label;
}

static void ui_num_build(lv_obj_t *num, const char *text)
{
  const lv_font_t *font = lv_obj_get_style_text_font(num, 0);
  int32_t h = lv_font_get_line_height(font);
  lv_obj_t *slot;

  lv_obj_clean(num);

  for (; *text != '\0'; text++)
    {
      slot = lv_obj_create(num);
      lv_obj_remove_style_all(slot);
      lv_obj_remove_flag(slot, LV_OBJ_FLAG_CLICKABLE |
                               LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_size(slot, ui_num_width(font, *text), h);
      ui_num_char(slot, *text);
    }
}

static void ui_num_count_cb(lv_anim_t *a, int32_t v)
{
  char text[UI_NUM_MAX];

  snprintf(text, sizeof(text), "%d%s", (int)v,
           (const char *)lv_anim_get_user_data(a));
  ui_num_set(a->var, text, false);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void ui_hide_perf(void)
{
  lv_obj_add_flag(lv_layer_sys(), LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *ui_box(lv_obj_t *parent, uint32_t color, int32_t radius)
{
  lv_obj_t *obj = lv_obj_create(parent);

  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, radius, 0);
  return obj;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                   const char *text)
{
  lv_obj_t *label = lv_label_create(parent);

  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_label_set_text(label, text);
  return label;
}

int32_t ui_ink_top(const lv_font_t *font)
{
  lv_font_glyph_dsc_t g;

  lv_font_get_glyph_dsc(font, &g, '0', 0);
  return lv_font_get_line_height(font) - font->base_line - g.box_h -
         g.ofs_y;
}

void ui_place(lv_obj_t *obj, const lv_font_t *font, int32_t x, int32_t y)
{
  lv_obj_set_pos(obj, x, y - ui_ink_top(font));
}

lv_obj_t *ui_tag(lv_obj_t *parent, uint32_t color, const char *text)
{
  lv_obj_t *label = ui_label(parent, UI_OUTFIT_26, color, text);

  lv_obj_set_style_text_letter_space(label, UI_TAG_SPACE, 0);
  return label;
}

lv_obj_t *ui_back(lv_obj_t *parent, uint32_t color, lv_event_cb_t cb)
{
  lv_obj_t *btn = ui_box(parent, color, UI_RADIUS);

  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(btn, UI_BACK_SIZE, UI_BACK_SIZE);
  lv_obj_set_style_bg_opa(btn, LV_OPA_70, LV_STATE_PRESSED);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
  lv_obj_center(ui_label(btn, UI_ICON_48, 0xffffff, LV_SYMBOL_LEFT));
  return btn;
}

lv_obj_t *ui_num(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
  lv_obj_t *num = lv_obj_create(parent);

  lv_obj_remove_style_all(num);
  lv_obj_remove_flag(num, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(num, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(num, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_text_font(num, font, 0);
  lv_obj_set_style_text_color(num, lv_color_hex(color), 0);
  return num;
}

void ui_num_set(lv_obj_t *num, const char *text, bool anim)
{
  const lv_font_t *font = lv_obj_get_style_text_font(num, 0);
  uint32_t len = strlen(text);
  lv_obj_t *slot;
  lv_obj_t *label;
  int32_t h;
  uint32_t i;

  if (lv_obj_get_child_count(num) != len)
    {
      ui_num_build(num, text);
      return;
    }

  for (i = 0; i < len; i++)
    {
      slot = lv_obj_get_child(num, i);

      /* Finish a roll that is still running */

      while (lv_obj_get_child_count(slot) > 1)
        {
          lv_obj_delete(lv_obj_get_child(slot, 0));
        }

      label = lv_obj_get_child(slot, 0);
      if (lv_label_get_text(label)[0] == text[i])
        {
          continue;
        }

      /* A slot only rolls between characters of the same width */

      if (ui_num_width(font, lv_label_get_text(label)[0]) !=
          ui_num_width(font, text[i]))
        {
          ui_num_build(num, text);
          return;
        }

      if (!anim)
        {
          lv_label_set_text_fmt(label, "%c", text[i]);
          continue;
        }

      h = lv_obj_get_height(slot);
      ui_anim(label, ui_translate_y_cb, 0, -h, UI_ROLL_TIME, 0,
              lv_obj_delete_anim_completed_cb);
      label = ui_num_char(slot, text[i]);
      lv_obj_set_style_translate_y(label, h, 0);
      ui_anim(label, ui_translate_y_cb, h, 0, UI_ROLL_TIME, 0, NULL);
    }
}

void ui_num_count(lv_obj_t *num, int32_t value, const char *suffix,
                  uint32_t delay)
{
  lv_anim_t a;

  lv_anim_init(&a);
  lv_anim_set_var(&a, num);
  lv_anim_set_custom_exec_cb(&a, ui_num_count_cb);
  lv_anim_set_user_data(&a, (void *)suffix);
  lv_anim_set_values(&a, 0, value);
  lv_anim_set_duration(&a, UI_ENTER_TIME);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

void ui_enter(lv_obj_t *obj, int32_t dx, int32_t dy, uint32_t delay)
{
  if (dx != 0)
    {
      lv_obj_set_style_translate_x(obj, dx, 0);
      ui_anim(obj, ui_translate_x_cb, dx, 0, UI_ENTER_TIME, delay, NULL);
    }

  if (dy != 0)
    {
      lv_obj_set_style_translate_y(obj, dy, 0);
      ui_anim(obj, ui_translate_y_cb, dy, 0, UI_ENTER_TIME, delay, NULL);
    }
}

void ui_anim(void *var, lv_anim_exec_xcb_t exec_cb, int32_t from,
             int32_t to, uint32_t time, uint32_t delay,
             lv_anim_completed_cb_t completed_cb)
{
  lv_anim_t a;

  lv_anim_init(&a);
  lv_anim_set_var(&a, var);
  lv_anim_set_exec_cb(&a, exec_cb);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, time);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_completed_cb(&a, completed_cb);
  lv_anim_start(&a);
}
