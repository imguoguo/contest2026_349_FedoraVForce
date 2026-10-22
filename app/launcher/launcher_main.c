/****************************************************************************
 * app/launcher/launcher_main.c
 *
 * LVGL launcher
 * VSAI LogicPi A1 (Amlogic S6): MIPI DSI 1080x1920 + GT9XX touch
 *
 * Lock screen, dashboard home, About and the all apps page.  Apps are NSH
 * builtins listed in CONFIG_LVX_LAUNCHER_APPS, the home screen shows the
 * ones in CONFIG_LVX_LAUNCHER_PINNED.  LVGL is a single global instance,
 * so the launcher releases it before starting an app and initializes it
 * again after the app exits.
 *
 * With PIPES, "echo <id> [argument] > /dev/launcher" in NSH starts an app
 * too, e.g. "echo bench run > /dev/launcher".
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/lib/builtin.h>

#include <lvgl/lvgl.h>

#include "lvx_ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LAUNCHER_TOP_Y        67      /* Team name and FPS row */
#define LAUNCHER_PAD          44
#define LAUNCHER_TILES_Y      1062
#define LAUNCHER_TILE_H       328
#define LAUNCHER_TILE_PAD     40
#define LAUNCHER_TITLE_BASE   97      /* Title baseline above tile bottom */
#define LAUNCHER_STRIP_H      130
#define LAUNCHER_HALF_W       ((UI_CONTENT_W - UI_GAP) / 2)
#define LAUNCHER_ROW_H        104     /* About key value rows */
#define LAUNCHER_CARD_H       240     /* Cards on the all apps page */
#define LAUNCHER_CARD_BASE    90      /* Card title baseline above bottom */
#define LAUNCHER_MAX_APPS     24

#define LAUNCHER_UNLOCK_Y     1320    /* Top of the orange unlock block */
#define LAUNCHER_UNLOCK_DIST  240
#define LAUNCHER_TAP_DIST     20
#define LAUNCHER_BOUNCE_TIME  260
#define LAUNCHER_FLOAT_DIST   40
#define LAUNCHER_FLOAT_TIME   800
#define LAUNCHER_LIFT_TIME    350

/* "echo bench run > /dev/launcher" starts an entry from the console */

#define LAUNCHER_FIFO         "/dev/launcher"
#define LAUNCHER_FIFO_POLL    200
#define LAUNCHER_FIFO_ARGS    4

/* The FPS counters show IDLE after a second without touch in which only
 * the clock rolled, about one roll worth of frames.
 */

#define LAUNCHER_IDLE_TIME    1000
#define LAUNCHER_IDLE_FRAMES  (UI_ROLL_TIME / LV_DEF_REFR_PERIOD + 2)

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum launcher_group_e
{
  LAUNCHER_TEAM,
  LAUNCHER_OPENVELA,
  LAUNCHER_LVGL,
  LAUNCHER_GROUPS
};

/* An entry of CONFIG_LVX_LAUNCHER_APPS: what it starts and how its tile
 * looks
 */

struct launcher_app_s
{
  const char *id;           /* Name in the config, NULL for About */
  const char *prog;         /* Builtin to start, NULL for the id itself */
  const char *arg;          /* Argument for it, may be NULL */
  enum launcher_group_e group;
  const char *title;
  const char *desc;
  const char *tag;          /* Small label, may be NULL */
  const char *icon;
  uint32_t bg;
  uint32_t fg;
  uint32_t icon_color;
};

/* An app that is built in */

struct launcher_entry_s
{
  struct launcher_app_s app;
  char title[16];           /* For apps not in g_apps */
  int index;                /* Builtin index */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void launcher_about_open_cb(lv_event_t *e);
static void launcher_apps_open_cb(lv_event_t *e);
static void launcher_lock_cb(lv_event_t *e);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct launcher_app_s g_apps[] =
{
  {
    "paint", NULL, NULL, LAUNCHER_TEAM,
    "PAINT", "Write a digit, the board reads it", "AI DIGITS",
    LV_SYMBOL_EDIT, UI_ORANGE, 0xffffff, 0xffffff
  },
  {
    "bench", NULL, NULL, LAUNCHER_TEAM,
    "BENCH", "LVGL render + CoreMark", NULL,
    LV_SYMBOL_CHARGE, UI_INK, 0xffffff, UI_ORANGE
  },
  {
    "monitor", NULL, NULL, LAUNCHER_TEAM,
    "MONITOR", "4 cores, tasks and memory", "SMP",
    LV_SYMBOL_BARS, UI_BLUE, 0xffffff, 0xffffff
  },
  {
    "touch", NULL, NULL, LAUNCHER_TEAM,
    "TOUCH", "Trails, report rate, calibration", NULL,
    LV_SYMBOL_GPS, UI_YELLOW, UI_INK, UI_INK
  },
  {
    "snake", "lvdemo", "snake", LAUNCHER_OPENVELA,
    "SNAKE", "Snake game from openvela", NULL,
    LV_SYMBOL_SHUFFLE, UI_INK_2, 0xffffff, UI_YELLOW
  },
  {
    "whackmole", "lvdemo", "whackmole", LAUNCHER_OPENVELA,
    "MOLES", "Whack-a-mole from openvela", NULL,
    LV_SYMBOL_PLAY, UI_INK_2, 0xffffff, UI_ORANGE
  },
  {
    "lvgl_benchmark", "lvdemo", "benchmark", LAUNCHER_LVGL,
    "LVGL BENCH", "The official LVGL benchmark", NULL,
    LV_SYMBOL_CHARGE, UI_PAPER, UI_INK, UI_ORANGE
  },
  {
    "lvgl_widgets", "lvdemo", "widgets", LAUNCHER_LVGL,
    "WIDGETS", "The standard LVGL widgets", NULL,
    LV_SYMBOL_LIST, UI_PAPER, UI_INK, UI_BLUE
  },
};

static const struct launcher_app_s g_about =
{
  NULL, NULL, NULL, LAUNCHER_TEAM,
  "ABOUT", "Port details", NULL,
  LV_SYMBOL_LIST, UI_PAPER, UI_INK, UI_INK
};

static const char * const g_group_names[LAUNCHER_GROUPS] =
{
  "TEAM 349", "OPENVELA", "LVGL"
};

/* CONFIG_LVX_LAUNCHER_APPS split in place, the entries point into it */

static char g_app_list[] = CONFIG_LVX_LAUNCHER_APPS;
static struct launcher_entry_s g_entries[LAUNCHER_MAX_APPS];
static int g_nentries = -1;

static const char * const g_about_rows[][2] =
{
  { "SOC", "Amlogic S6 (ARM64)" },
  { "CPU", "4x Cortex-A510, SMP" },
  { "INTERRUPT", "GICv3" },
  { "DISPLAY", "MIPI DSI 1080x1920" },
  { "FORMAT", "RGB565" },
  { "TOUCH", "GT9XX over I2C" },
};

static lv_nuttx_result_t g_result;

static lv_obj_t *g_lock;
static lv_obj_t *g_unlock;
static lv_obj_t *g_home_scr;
static lv_obj_t *g_about_scr;
static lv_obj_t *g_apps_scr;
static lv_obj_t *g_about_rule;
static lv_obj_t *g_about_list;

/* Live values */

static lv_obj_t *g_lock_hour;
static lv_obj_t *g_lock_min;
static lv_obj_t *g_lock_sec;
static lv_obj_t *g_home_clock;
static lv_obj_t *g_home_sec;
static lv_obj_t *g_home_heap;
static lv_obj_t *g_home_fps;
static lv_obj_t *g_home_uptime;
static lv_obj_t *g_top_fps;
static lv_obj_t *g_about_fps;
static lv_obj_t *g_about_heap;
static lv_obj_t *g_about_bar;

static uint32_t g_frames;
static int g_fps;                   /* -1 while idle */
static int32_t g_drag_start;
static bool g_drag_unlock;
static int g_launch;                /* Entry to start, -1 for none */
static bool g_from_apps;            /* Started from the all apps page */
static int g_fifo = -1;
static char g_fifo_line[64];
static FAR char *g_fifo_args[LAUNCHER_FIFO_ARGS];  /* Replace app.arg */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Text whose top right corner is at (x, y) */

static lv_obj_t *launcher_right(lv_obj_t *obj, const lv_font_t *font,
                                int32_t x, int32_t y)
{
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -x, y - ui_ink_top(font));
  return obj;
}

static lv_obj_t *launcher_card(lv_obj_t *parent, uint32_t color, int32_t x,
                               int32_t y, int32_t w, int32_t h)
{
  lv_obj_t *card = ui_box(parent, color, UI_RADIUS);

  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  return card;
}

static lv_obj_t *launcher_screen(void)
{
  lv_obj_t *scr = lv_obj_create(NULL);

  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_scroll_dir(scr, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  return scr;
}

/* "FEDORA-V FORCE / 349" on the left, returns the right label */

static lv_obj_t *launcher_topbar(lv_obj_t *parent, uint32_t color,
                                 const char *right)
{
  ui_place(ui_tag(parent, UI_DIM, "FEDORA-V FORCE  /  349"), UI_OUTFIT_26,
           UI_MARGIN, LAUNCHER_TOP_Y);
  return launcher_right(ui_tag(parent, color, right), UI_OUTFIT_26,
                        UI_MARGIN, LAUNCHER_TOP_Y);
}

static void launcher_width_cb(void *obj, int32_t v)
{
  lv_obj_set_width(obj, v);
}

static void launcher_translate_y_cb(void *obj, int32_t v)
{
  lv_obj_set_style_translate_y(obj, v, 0);
}

/* Width of the used part of the About heap bar */

static int32_t launcher_heap_used(const struct mallinfo *mi)
{
  return ((mi->arena - mi->fordblks) >> 10) * (UI_CONTENT_W - 2 * 48) /
         (mi->arena >> 10);
}

/****************************************************************************
 * Name: frame rate
 *
 * A frame is a refresh that rendered something, LV_EVENT_RENDER_READY is
 * not sent when nothing was invalidated.
 ****************************************************************************/

static void launcher_render_cb(lv_event_t *e)
{
  UNUSED(e);
  g_frames++;
}

static void launcher_fps_text(char *buf, size_t size, const char *suffix)
{
  if (g_fps < 0)
    {
      snprintf(buf, size, "IDLE");
    }
  else
    {
      snprintf(buf, size, "%d%s", g_fps, suffix);
    }
}

/****************************************************************************
 * Name: tiles
 ****************************************************************************/

/* Resolve CONFIG_LVX_LAUNCHER_APPS once.  An id not in g_apps is a builtin
 * of that name with a dark tile.  Ids that are not built in are skipped.
 */

static void launcher_load_entries(void)
{
  struct launcher_entry_s *e;
  FAR char *save;
  FAR char *id;
  int i;

  g_nentries = 0;
  for (id = strtok_r(g_app_list, " ", &save);
       id != NULL && g_nentries < LAUNCHER_MAX_APPS;
       id = strtok_r(NULL, " ", &save))
    {
      e = &g_entries[g_nentries];

      for (i = 0; i < nitems(g_apps) && strcmp(g_apps[i].id, id) != 0;
           i++);

      if (i < nitems(g_apps))
        {
          e->app = g_apps[i];
        }
      else
        {
          for (i = 0; i < sizeof(e->title) - 1 && id[i] != '\0'; i++)
            {
              e->title[i] = toupper(id[i]);
            }

          e->title[i] = '\0';
          memset(&e->app, 0, sizeof(e->app));
          e->app.id = id;
          e->app.group = LAUNCHER_OPENVELA;
          e->app.title = e->title;
          e->app.desc = "";
          e->app.icon = LV_SYMBOL_PLAY;
          e->app.bg = UI_INK_2;
          e->app.fg = 0xffffff;
          e->app.icon_color = 0xffffff;
        }

      e->index = builtin_isavail(e->app.prog ? e->app.prog : id);
      if (e->index >= 0)
        {
          g_nentries++;
        }
    }
}

static int launcher_find_entry(const char *id)
{
  int i;

  for (i = 0; i < g_nentries; i++)
    {
      if (strcmp(g_entries[i].app.id, id) == 0)
        {
          return i;
        }
    }

  return -1;
}

/* Icon and arrow on top, title and one line of description at the
 * bottom.  The tag goes below the icon on a tall tile, beside it otherwise.
 * Home tiles have Oswald 120 titles, all apps cards Oswald 64; a title too
 * wide for the tile gets a smaller font.
 */

static void launcher_fill_tile(lv_obj_t *tile,
                               const struct launcher_app_s *app, bool tall,
                               bool card)
{
  const lv_font_t *font = card ? UI_OSWALD_64 : UI_OSWALD_120;
  int32_t base = card ? LAUNCHER_CARD_BASE : LAUNCHER_TITLE_BASE;
  lv_obj_t *obj;

  lv_obj_set_style_bg_color(tile, lv_color_hex(app->bg), 0);
  lv_obj_set_style_bg_color(tile, lv_color_darken(lv_color_hex(app->bg),
                                                  LV_OPA_20),
                            LV_STATE_PRESSED);
  lv_obj_set_style_pad_all(tile, LAUNCHER_TILE_PAD, 0);

  obj = ui_label(tile, UI_ICON_48, app->icon_color, app->icon);
  lv_obj_set_pos(obj, 0, 6);
  obj = ui_label(tile, UI_ICON_48, app->fg, LV_SYMBOL_RIGHT);
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, 0, 6);

  if (app->tag != NULL)
    {
      obj = ui_tag(tile, app->bg, app->tag);
      lv_obj_set_style_bg_color(obj, lv_color_hex(app->fg), 0);
      lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(obj, 6, 0);
      lv_obj_set_style_pad_hor(obj, 18, 0);
      lv_obj_set_style_pad_ver(obj, 2, 0);
      lv_obj_set_pos(obj, tall ? 0 : 80, tall ? 90 : 12);
    }

  if (lv_text_get_width(app->title, strlen(app->title), font, 0) >
      lv_obj_get_style_width(tile, 0) - 2 * LAUNCHER_TILE_PAD)
    {
      font = card ? UI_OUTFIT_34 : UI_OSWALD_64;
    }

  /* The title stands on a baseline above the description */

  obj = ui_label(tile, font, app->fg, app->title);
  lv_obj_align(obj, LV_ALIGN_BOTTOM_LEFT, 0,
               LAUNCHER_TILE_PAD - base + font->base_line);
  obj = ui_label(tile, UI_OUTFIT_26, app->fg, app->desc);
  lv_obj_align(obj, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

static lv_obj_t *launcher_tile(lv_obj_t *parent)
{
  lv_obj_t *tile = ui_box(parent, 0, UI_RADIUS);

  lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
  return tile;
}

static void launcher_app_cb(lv_event_t *e)
{
  g_from_apps = lv_screen_active() == g_apps_scr;
  g_launch = (int)(intptr_t)lv_event_get_user_data(e);
}

/* A line "<id> [arguments]" written to LAUNCHER_FIFO starts that entry as
 * if its tile was tapped, so the apps can be run without touching the
 * screen.  Arguments given replace the one of the entry.  The pipe stays
 * open while the app runs, a line written then is read when the launcher
 * is back.
 */

static void launcher_fifo_cb(lv_timer_t *timer)
{
  FAR char *save;
  FAR char *id;
  ssize_t n;
  int i;
  int j;

  UNUSED(timer);

  n = read(g_fifo, g_fifo_line, sizeof(g_fifo_line) - 1);
  if (n <= 0)
    {
      return;
    }

  g_fifo_line[n] = '\0';
  id = strtok_r(g_fifo_line, " \r\n", &save);
  i = id != NULL ? launcher_find_entry(id) : -1;
  if (i < 0)
    {
      fprintf(stderr, "launcher: no app %s\n", id != NULL ? id : "");
      return;
    }

  g_fifo_args[0] = strtok_r(NULL, " \r\n", &save);
  for (j = 1; j < LAUNCHER_FIFO_ARGS - 1 && g_fifo_args[j - 1] != NULL;
       j++)
    {
      g_fifo_args[j] = strtok_r(NULL, " \r\n", &save);
    }

  g_fifo_args[j] = NULL;
  g_from_apps = false;
  g_launch = i;
}

/* With three or more items the first is a tall tile on the left and the
 * next two stack beside it.  The rest go two per row, a last odd one
 * takes the whole row.  Returns the number of rows used.
 */

static int launcher_place_tile(lv_obj_t *tile, int i, int n)
{
  int32_t step = LAUNCHER_TILE_H + UI_GAP;
  int32_t right = UI_MARGIN + LAUNCHER_HALF_W + UI_GAP;
  int k = n >= 3 ? i - 3 : i;
  int row = (n >= 3 ? 2 : 0) + k / 2;

  if (n >= 3 && i == 0)
    {
      lv_obj_set_pos(tile, UI_MARGIN, LAUNCHER_TILES_Y);
      lv_obj_set_size(tile, LAUNCHER_HALF_W, LAUNCHER_TILE_H + step);
      return 2;
    }

  if (n >= 3 && i < 3)
    {
      lv_obj_set_pos(tile, right, LAUNCHER_TILES_Y + (i - 1) * step);
      lv_obj_set_size(tile, LAUNCHER_HALF_W, LAUNCHER_TILE_H);
      return 2;
    }

  lv_obj_set_pos(tile, k % 2 ? right : UI_MARGIN,
                 LAUNCHER_TILES_Y + row * step);
  lv_obj_set_size(tile, k % 2 == 0 && i == n - 1 ? UI_CONTENT_W :
                  LAUNCHER_HALF_W, LAUNCHER_TILE_H);
  return row + 1;
}

/* Pinned apps with About second and ALL APPS last, then the lock strip */

static int32_t launcher_create_tiles(lv_obj_t *parent)
{
  static struct launcher_app_s all =
  {
    NULL, NULL, NULL, LAUNCHER_TEAM,
    "ALL APPS", "Team 349, openvela and LVGL", NULL,
    LV_SYMBOL_DIRECTORY, UI_PAPER, UI_INK, UI_ORANGE
  };

  static char count[16];
  char pinned[] = CONFIG_LVX_LAUNCHER_PINNED;
  int items[8];             /* Entry, -1 About, -2 all apps */
  FAR char *save;
  FAR char *id;
  lv_obj_t *tile;
  int32_t y;
  int rows = 0;
  int n = 0;
  int i;

  for (id = strtok_r(pinned, " ", &save); id != NULL && n < 6;
       id = strtok_r(NULL, " ", &save))
    {
      items[n] = launcher_find_entry(id);
      if (items[n] >= 0)
        {
          n++;
        }

      if (n == 1)
        {
          items[n++] = -1;
        }
    }

  if (n == 0)
    {
      items[n++] = -1;
    }

  items[n++] = -2;
  snprintf(count, sizeof(count), "%d APPS", g_nentries);
  all.tag = count;

  for (i = 0; i < n; i++)
    {
      tile = launcher_tile(parent);
      rows = launcher_place_tile(tile, i, n);
      launcher_fill_tile(tile, items[i] >= 0 ? &g_entries[items[i]].app :
                         items[i] == -1 ? &g_about : &all,
                         i == 0 && n >= 3, false);

      if (items[i] >= 0)
        {
          lv_obj_add_event_cb(tile, launcher_app_cb, LV_EVENT_CLICKED,
                              (void *)(intptr_t)items[i]);
        }
      else
        {
          lv_obj_add_event_cb(tile, items[i] == -1 ?
                              launcher_about_open_cb :
                              launcher_apps_open_cb,
                              LV_EVENT_CLICKED, NULL);
        }
    }

  y = LAUNCHER_TILES_Y + rows * (LAUNCHER_TILE_H + UI_GAP);

  tile = ui_box(parent, UI_PAPER, UI_RADIUS);
  lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(tile, lv_color_hex(UI_RULE), LV_STATE_PRESSED);
  lv_obj_set_style_pad_hor(tile, LAUNCHER_TILE_PAD, 0);
  lv_obj_set_pos(tile, UI_MARGIN, y);
  lv_obj_set_size(tile, UI_CONTENT_W, LAUNCHER_STRIP_H);
  lv_obj_add_event_cb(tile, launcher_lock_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_align(ui_label(tile, UI_ICON_48, UI_INK, LV_SYMBOL_POWER),
               LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_align(ui_label(tile, UI_OSWALD_64, UI_INK, "LOCK"),
               LV_ALIGN_LEFT_MID, 80, 0);
  lv_obj_align(ui_label(tile, UI_ICON_48, UI_INK, LV_SYMBOL_RIGHT),
               LV_ALIGN_RIGHT_MID, 0, 0);

  return y + LAUNCHER_STRIP_H;
}

/****************************************************************************
 * Name: home screen
 ****************************************************************************/

static void launcher_create_home(void)
{
  lv_obj_t *scr = launcher_screen();
  lv_obj_t *card;
  lv_obj_t *obj;
  int32_t bottom;

  g_top_fps = launcher_topbar(scr, UI_INK, "IDLE");

  /* Uptime clock */

  card = launcher_card(scr, UI_INK, UI_MARGIN, 140, UI_CONTENT_W, 440);
  ui_place(ui_tag(card, UI_MUTED, "UPTIME CLOCK"), UI_OUTFIT_26,
           LAUNCHER_PAD, 47);
  obj = launcher_right(ui_label(card, UI_OUTFIT_26, UI_MUTED,
                                "NuttX / openvela"),
                       UI_OUTFIT_26, LAUNCHER_PAD, 47);
  lv_obj_align_to(ui_label(card, UI_ICON_24, UI_MUTED, LV_SYMBOL_SETTINGS),
                  obj, LV_ALIGN_OUT_LEFT_MID, -12, 0);

  g_home_clock = ui_num(card, UI_OSWALD_300, 0xffffff);
  ui_place(g_home_clock, UI_OSWALD_300, LAUNCHER_PAD, 165);
  g_home_sec = ui_num(card, UI_OSWALD_120, UI_ORANGE);
  ui_place(g_home_sec, UI_OSWALD_120, 770, 148);

  /* Free heap and frame rate */

  card = launcher_card(scr, UI_YELLOW, UI_MARGIN, 604, LAUNCHER_HALF_W, 260);
  ui_place(ui_tag(card, UI_INK, "HEAP FREE"), UI_OUTFIT_26, LAUNCHER_PAD,
           47);
  g_home_heap = ui_num(card, UI_OSWALD_120, UI_INK);
  ui_place(g_home_heap, UI_OSWALD_120, LAUNCHER_PAD, 118);

  card = launcher_card(scr, UI_BLUE, UI_MARGIN + LAUNCHER_HALF_W + UI_GAP,
                       604, LAUNCHER_HALF_W, 260);
  ui_place(ui_tag(card, 0xffffff, "FRAMES / S"), UI_OUTFIT_26,
           LAUNCHER_PAD, 47);
  g_home_fps = ui_num(card, UI_OSWALD_120, 0xffffff);
  ui_place(g_home_fps, UI_OSWALD_120, LAUNCHER_PAD, 118);

  card = launcher_card(scr, UI_PAPER, UI_MARGIN, 888, UI_CONTENT_W, 150);
  lv_obj_align(ui_tag(card, UI_DIM, "UPTIME"), LV_ALIGN_LEFT_MID,
               LAUNCHER_PAD, 0);
  g_home_uptime = ui_label(card, UI_OSWALD_64, UI_INK, "");
  lv_obj_align(g_home_uptime, LV_ALIGN_RIGHT_MID, -LAUNCHER_PAD, 0);

  bottom = launcher_create_tiles(scr);

  /* Room below the last tile when the list scrolls */

  obj = lv_obj_create(scr);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, 0, bottom);
  lv_obj_set_size(obj, 1, UI_GAP);

  g_home_scr = scr;
}

/* Cards rise into place one after another and the counters start at 0 */

static void launcher_home_intro(void)
{
  struct mallinfo mi = mallinfo();
  uint32_t i;
  uint32_t n = lv_obj_get_child_count(g_home_scr);

  for (i = 2; i < n; i++)
    {
      ui_enter(lv_obj_get_child(g_home_scr, i), 0, 120, (i - 2) * 40);
    }

  ui_num_count(g_home_heap, mi.fordblks >> 20, "M", 100);
  if (g_fps >= 0)
    {
      ui_num_count(g_home_fps, g_fps, "", 140);
    }
}

/****************************************************************************
 * Name: about screen
 ****************************************************************************/

static void launcher_about_close_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_screen_load(g_home_scr);
}

static void launcher_about_open_cb(lv_event_t *e)
{
  struct mallinfo mi = mallinfo();
  uint32_t i;

  UNUSED(e);

  lv_screen_load(g_about_scr);

  /* Rule draws from the left, rows follow it */

  ui_anim(g_about_rule, launcher_width_cb, 0, UI_CONTENT_W, UI_ENTER_TIME,
          0, NULL);
  for (i = 0; i < lv_obj_get_child_count(g_about_list); i++)
    {
      ui_enter(lv_obj_get_child(g_about_list, i), -60, 0, i * 40);
    }

  if (g_fps >= 0)
    {
      ui_num_count(g_about_fps, g_fps, "", 100);
    }

  ui_anim(g_about_bar, launcher_width_cb, 0, launcher_heap_used(&mi),
          UI_ENTER_TIME, 100, NULL);
}

static void launcher_create_about(void)
{
  lv_obj_t *scr = launcher_screen();
  lv_obj_t *card;
  lv_obj_t *row;
  lv_obj_t *obj;
  int32_t y;
  int i;

  lv_obj_set_pos(ui_back(scr, UI_INK, launcher_about_close_cb), UI_MARGIN,
                 56);
  launcher_right(ui_tag(scr, UI_DIM, "PORT DETAILS"), UI_OUTFIT_26,
                 UI_MARGIN, 98);
  ui_place(ui_label(scr, UI_OSWALD_120, UI_INK, "ABOUT"), UI_OSWALD_120,
           UI_MARGIN, 212);
  ui_place(ui_label(scr, UI_OUTFIT_52, UI_INK, "VSAI LogicPi A1"),
           UI_OUTFIT_52, UI_MARGIN, 352);
  ui_place(ui_label(scr, UI_OUTFIT_26, UI_DIM,
                    "openvela / NuttX, ported by team 349"),
           UI_OUTFIT_26, UI_MARGIN, 428);

  g_about_rule = ui_box(scr, UI_INK, 0);
  lv_obj_set_pos(g_about_rule, UI_MARGIN, 500);
  lv_obj_set_size(g_about_rule, UI_CONTENT_W, 6);

  /* Key value rows, clipped so they can slide in from the left */

  g_about_list = lv_obj_create(scr);
  lv_obj_remove_style_all(g_about_list);
  lv_obj_remove_flag(g_about_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(g_about_list, UI_MARGIN, 510);
  lv_obj_set_size(g_about_list, UI_CONTENT_W,
                  nitems(g_about_rows) * LAUNCHER_ROW_H);

  for (i = 0; i < nitems(g_about_rows); i++)
    {
      row = lv_obj_create(g_about_list);
      lv_obj_remove_style_all(row);
      lv_obj_set_pos(row, 0, i * LAUNCHER_ROW_H);
      lv_obj_set_size(row, UI_CONTENT_W, LAUNCHER_ROW_H);
      lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
      lv_obj_set_style_border_width(row, 2, 0);
      lv_obj_set_style_border_color(row, lv_color_hex(UI_RULE), 0);
      lv_obj_align(ui_tag(row, UI_DIM, g_about_rows[i][0]),
                   LV_ALIGN_LEFT_MID, 0, 0);
      lv_obj_align(ui_label(row, UI_OUTFIT_34, UI_INK, g_about_rows[i][1]),
                   LV_ALIGN_RIGHT_MID, 0, 0);
    }

  /* Live frame rate and heap */

  y = 510 + nitems(g_about_rows) * LAUNCHER_ROW_H + 80;
  card = launcher_card(scr, UI_BLUE, UI_MARGIN, y, UI_CONTENT_W, 360);
  ui_place(ui_tag(card, 0xffffff, "LIVE"), UI_OUTFIT_26, 48, 50);
  ui_place(ui_tag(card, 0xffffff, "FRAMES / S"), UI_OUTFIT_26, 48, 102);
  g_about_fps = ui_num(card, UI_OSWALD_120, 0xffffff);
  lv_obj_align(g_about_fps, LV_ALIGN_TOP_RIGHT, -48,
               30 - ui_ink_top(UI_OSWALD_120));
  g_about_heap = ui_label(card, UI_OUTFIT_34, 0xffffff, "");
  ui_place(g_about_heap, UI_OUTFIT_34, 48, 228);

  obj = ui_box(card, UI_BLUE_DIM, 0);
  lv_obj_set_pos(obj, 48, 288);
  lv_obj_set_size(obj, UI_CONTENT_W - 2 * 48, 20);
  g_about_bar = ui_box(obj, 0xffffff, 0);
  lv_obj_set_size(g_about_bar, 0, 20);

  /* Back bar */

  obj = ui_box(scr, UI_INK, UI_RADIUS);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(obj, lv_color_hex(UI_INK_2), LV_STATE_PRESSED);
  lv_obj_set_pos(obj, UI_MARGIN, 1672);
  lv_obj_set_size(obj, UI_CONTENT_W, 136);
  lv_obj_add_event_cb(obj, launcher_about_close_cb, LV_EVENT_CLICKED, NULL);
  row = lv_obj_create(obj);
  lv_obj_remove_style_all(row);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 32, 0);
  lv_obj_center(row);
  ui_label(row, UI_ICON_48, 0xffffff, LV_SYMBOL_LEFT);
  ui_label(row, UI_OSWALD_64, 0xffffff, "BACK");

  g_about_scr = scr;
}

/****************************************************************************
 * Name: all apps page
 *
 * Every built in entry of CONFIG_LVX_LAUNCHER_APPS, grouped, two cards per
 * row.  An app started here comes back to this page when it exits.
 ****************************************************************************/

static void launcher_apps_close_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_screen_load(g_home_scr);
}

static void launcher_apps_open_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_obj_scroll_to_y(g_apps_scr, 0, LV_ANIM_OFF);
  lv_screen_load(g_apps_scr);
}

static void launcher_create_apps(void)
{
  lv_obj_t *scr = launcher_screen();
  lv_obj_t *obj;
  int32_t y = 380;
  int group;
  int n;
  int i;

  lv_obj_set_pos(ui_back(scr, UI_INK, launcher_apps_close_cb), UI_MARGIN,
                 56);
  obj = launcher_right(ui_tag(scr, UI_DIM, ""), UI_OUTFIT_26, UI_MARGIN, 98);
  lv_label_set_text_fmt(obj, "%d INSTALLED", g_nentries);
  ui_place(ui_label(scr, UI_OSWALD_120, UI_INK, "APPS"), UI_OSWALD_120,
           UI_MARGIN, 212);

  for (group = 0; group < LAUNCHER_GROUPS; group++)
    {
      n = 0;
      for (i = 0; i < g_nentries; i++)
        {
          if (g_entries[i].app.group != group)
            {
              continue;
            }

          /* Group title and a thin rule before its first card */

          if (n == 0)
            {
              ui_place(ui_tag(scr, UI_DIM, g_group_names[group]),
                       UI_OUTFIT_26, UI_MARGIN, y);
              obj = ui_box(scr, UI_RULE, 0);
              lv_obj_set_pos(obj, UI_MARGIN, y + 44);
              lv_obj_set_size(obj, UI_CONTENT_W, 2);
              y += 70;
            }

          obj = launcher_tile(scr);
          lv_obj_set_pos(obj, UI_MARGIN + n % 2 * (LAUNCHER_HALF_W + UI_GAP),
                         y);
          lv_obj_set_size(obj, LAUNCHER_HALF_W, LAUNCHER_CARD_H);
          launcher_fill_tile(obj, &g_entries[i].app, false, true);
          lv_obj_add_event_cb(obj, launcher_app_cb, LV_EVENT_CLICKED,
                              (void *)(intptr_t)i);

          y += n % 2 ? LAUNCHER_CARD_H + UI_GAP : 0;
          n++;
        }

      y += n % 2 ? LAUNCHER_CARD_H + UI_GAP : 0;
      y += n > 0 ? 36 : 0;
    }

  /* Room below the last card when the page scrolls */

  obj = lv_obj_create(scr);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, 0, y);
  lv_obj_set_size(obj, 1, UI_GAP);

  g_apps_scr = scr;
}

/****************************************************************************
 * Name: lock screen
 *
 * The lock lives on the top layer over the home screen.  Dragging the
 * orange block, or swiping anywhere, moves the block with the finger.
 * Far enough and the lock lifts away, otherwise the block springs back.
 * A tap on the block unlocks too.
 ****************************************************************************/

static void launcher_unlocked_cb(lv_anim_t *a)
{
  lv_obj_add_flag(a->var, LV_OBJ_FLAG_HIDDEN);
}

static void launcher_unlock(void)
{
  lv_obj_scroll_to_y(g_home_scr, 0, LV_ANIM_OFF);
  launcher_home_intro();
  ui_anim(g_lock, launcher_translate_y_cb,
          lv_obj_get_style_translate_y(g_lock, 0), -UI_SCREEN_H,
          LAUNCHER_LIFT_TIME, 0, launcher_unlocked_cb);
}

static void launcher_lock_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_obj_set_style_translate_y(g_unlock, 0, 0);
  lv_obj_remove_flag(g_lock, LV_OBJ_FLAG_HIDDEN);
  ui_anim(g_lock, launcher_translate_y_cb, -UI_SCREEN_H, 0,
          LAUNCHER_LIFT_TIME, 0, NULL);
}

static void launcher_drag_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_point_t p;
  int32_t dy;

  lv_indev_get_point(lv_indev_active(), &p);

  if (code == LV_EVENT_PRESSED)
    {
      g_drag_start = p.y;
      g_drag_unlock = p.y >= LAUNCHER_UNLOCK_Y;
      return;
    }

  dy = MAX(g_drag_start - p.y, 0);

  if (code == LV_EVENT_PRESSING)
    {
      lv_obj_set_style_translate_y(g_unlock, -dy, 0);
    }
  else if (dy > LAUNCHER_UNLOCK_DIST ||
           (g_drag_unlock && dy < LAUNCHER_TAP_DIST))
    {
      launcher_unlock();
    }
  else
    {
      ui_anim(g_unlock, launcher_translate_y_cb, -dy, 0,
              LAUNCHER_BOUNCE_TIME, 0, NULL);
    }
}

static void launcher_create_lock(void)
{
  lv_obj_t *lock = lv_obj_create(lv_layer_top());
  lv_obj_t *obj;
  lv_anim_t a;

  lv_obj_remove_style_all(lock);
  lv_obj_remove_flag(lock, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(lock, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_size(lock, UI_SCREEN_W, UI_SCREEN_H);
  lv_obj_set_style_bg_color(lock, lv_color_hex(UI_BG), 0);
  lv_obj_set_style_bg_opa(lock, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(lock, launcher_drag_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(lock, launcher_drag_cb, LV_EVENT_PRESSING, NULL);
  lv_obj_add_event_cb(lock, launcher_drag_cb, LV_EVENT_RELEASED, NULL);

  launcher_topbar(lock, UI_INK, "OPENVELA");

  /* Hours and minutes stacked, seconds beside the minutes */

  g_lock_hour = ui_num(lock, UI_OSWALD_300, UI_INK);
  ui_place(g_lock_hour, UI_OSWALD_300, UI_MARGIN, 203);
  g_lock_min = ui_num(lock, UI_OSWALD_300, UI_INK);
  ui_place(g_lock_min, UI_OSWALD_300, UI_MARGIN, 492);
  ui_place(ui_tag(lock, UI_DIM, "UPTIME"), UI_OUTFIT_26, 428, 563);
  g_lock_sec = ui_num(lock, UI_OSWALD_120, UI_ORANGE);
  ui_place(g_lock_sec, UI_OSWALD_120, 428, 620);

  obj = ui_box(lock, UI_INK, 0);
  lv_obj_set_pos(obj, UI_MARGIN, 880);
  lv_obj_set_size(obj, UI_CONTENT_W, 6);

  ui_place(ui_label(lock, UI_OUTFIT_52, UI_INK, "openvela on\nLogicPi A1"),
           UI_OUTFIT_52, UI_MARGIN, 935);
  obj = ui_label(lock, UI_OUTFIT_26, UI_DIM,
                 "NuttX\nAmlogic S6\n4x Cortex-A510");
  lv_obj_set_style_text_line_space(obj, 8, 0);
  lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, 0);
  launcher_right(obj, UI_OUTFIT_26, UI_MARGIN, 939);

  /* Unlock block */

  g_unlock = ui_box(lock, UI_ORANGE, 0);
  lv_obj_add_flag(g_unlock, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_pos(g_unlock, 0, LAUNCHER_UNLOCK_Y);
  lv_obj_set_size(g_unlock, UI_SCREEN_W, UI_SCREEN_H - LAUNCHER_UNLOCK_Y);
  ui_place(ui_label(g_unlock, UI_OSWALD_120, 0xffffff, "SWIPE UP"),
           UI_OSWALD_120, UI_MARGIN, 130);
  ui_place(ui_label(g_unlock, UI_OUTFIT_34, 0xffffff,
                    "or tap here to unlock"), UI_OUTFIT_34, UI_MARGIN, 284);

  obj = ui_label(g_unlock, UI_ICON_48, 0xffffff, LV_SYMBOL_UP);
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -UI_MARGIN, 160);

  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_exec_cb(&a, launcher_translate_y_cb);
  lv_anim_set_values(&a, 0, -LAUNCHER_FLOAT_DIST);
  lv_anim_set_duration(&a, LAUNCHER_FLOAT_TIME);
  lv_anim_set_playback_duration(&a, LAUNCHER_FLOAT_TIME);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);

  g_lock = lock;
}

/****************************************************************************
 * Name: lifecycle
 ****************************************************************************/

static void launcher_periodic_cb(lv_timer_t *timer)
{
  struct mallinfo mi = mallinfo();
  struct timespec ts;
  unsigned int sec;
  bool anim = timer != NULL;
  char text[32];

  clock_gettime(CLOCK_MONOTONIC, &ts);
  sec = ts.tv_sec;

  g_fps = lv_display_get_inactive_time(NULL) > LAUNCHER_IDLE_TIME &&
          g_frames <= LAUNCHER_IDLE_FRAMES ? -1 : (int)g_frames;
  g_frames = 0;

  snprintf(text, sizeof(text), "%02u", sec / 3600 % 100);
  ui_num_set(g_lock_hour, text, anim);
  snprintf(text, sizeof(text), "%02u", sec / 60 % 60);
  ui_num_set(g_lock_min, text, anim);
  snprintf(text, sizeof(text), "%02u", sec % 60);
  ui_num_set(g_lock_sec, text, anim);
  ui_num_set(g_home_sec, text, anim);
  snprintf(text, sizeof(text), "%02u:%02u", sec / 3600 % 100,
           sec / 60 % 60);
  ui_num_set(g_home_clock, text, anim);

  snprintf(text, sizeof(text), "%uM", mi.fordblks >> 20);
  ui_num_set(g_home_heap, text, anim);
  lv_label_set_text_fmt(g_home_uptime, "%uh %um", sec / 3600,
                        sec / 60 % 60);
  lv_label_set_text_fmt(g_about_heap, "Heap %uKB free of %uKB",
                        mi.fordblks >> 10, mi.arena >> 10);
  lv_obj_set_width(g_about_bar, launcher_heap_used(&mi));

  launcher_fps_text(text, sizeof(text), " FPS");
  lv_label_set_text(g_top_fps, text);
  launcher_fps_text(text, sizeof(text), "");
  ui_num_set(g_home_fps, text, anim);
  ui_num_set(g_about_fps, text, anim);
}

/* "last" is the entry that just exited, or -1 on boot */

static int launcher_start(int last)
{
  lv_nuttx_dsc_t info;

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, &g_result);

  if (g_result.disp == NULL || g_result.indev == NULL)
    {
      fprintf(stderr, "launcher: no display or touchscreen\n");
      return -ENODEV;
    }

  lv_display_add_event_cb(g_result.disp, launcher_render_cb,
                          LV_EVENT_RENDER_READY, NULL);
  ui_hide_perf();

  if (g_nentries < 0)
    {
      launcher_load_entries();
    }

  g_fps = -1;
  launcher_create_about();
  launcher_create_apps();
  launcher_create_home();
  launcher_create_lock();
  lv_screen_load(g_home_scr);

  launcher_periodic_cb(NULL);
  lv_timer_create(launcher_periodic_cb, 1000, NULL);
  if (g_fifo >= 0)
    {
      lv_timer_create(launcher_fifo_cb, LAUNCHER_FIFO_POLL, NULL);
    }

  /* Back from an app: to the page it was started from */

  if (last >= 0)
    {
      lv_obj_add_flag(g_lock, LV_OBJ_FLAG_HIDDEN);
      if (g_from_apps)
        {
          lv_screen_load(g_apps_scr);
        }
      else
        {
          launcher_home_intro();
        }
    }

  return 0;
}

static void launcher_stop(void)
{
  lv_nuttx_deinit(&g_result);
  lv_deinit();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  FAR const struct builtin_s *app;
  FAR char *args[2];
  FAR char **argp;
  uint32_t idle;
  int last = -1;
  pid_t pid;

  UNUSED(argc);
  UNUSED(argv);

  if (lv_is_initialized())
    {
      fprintf(stderr, "launcher: LVGL is busy\n");
      return 1;
    }

#ifdef CONFIG_PIPES
  mkfifo(LAUNCHER_FIFO, 0666);
  g_fifo = open(LAUNCHER_FIFO, O_RDONLY | O_NONBLOCK);
#endif

  while (launcher_start(last) == 0)
    {
      g_launch = -1;
      while (g_launch < 0)
        {
          idle = lv_timer_handler();
          usleep(MAX(idle, 1) * 1000);
        }

      /* Show the pressed tile while the app starts */

      lv_refr_now(NULL);
      launcher_stop();

      last = g_launch;
      app = builtin_for_index(g_entries[last].index);
      args[0] = (FAR char *)g_entries[last].app.arg;
      args[1] = NULL;
      argp = g_fifo_args[0] != NULL ? g_fifo_args :
             args[0] != NULL ? args : NULL;
      pid = task_create(app->name, app->priority, app->stacksize, app->main,
                        argp);
      g_fifo_args[0] = NULL;
      if (pid < 0)
        {
          fprintf(stderr, "launcher: start %s failed: %d\n", app->name,
                  errno);
        }
      else
        {
          waitpid(pid, NULL, 0);
        }
    }

  launcher_stop();
  return 1;
}
