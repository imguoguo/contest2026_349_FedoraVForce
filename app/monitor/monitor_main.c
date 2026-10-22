/****************************************************************************
 * app/monitor/monitor_main.c
 *
 * LVGL system monitor
 *
 * Load of each CPU, memory and the task list, refreshed every second from
 * procfs.  NuttX only has the total load in /proc/cpuload, but every CPU
 * runs an IDLE task of its own (PID 0 to CONFIG_SMP_NCPUS - 1), so the
 * load of a CPU is 100% minus the load of its IDLE task.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>

#include <dirent.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "lvx_ui.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef MONITOR_PROC
#  define MONITOR_PROC        "/proc"
#endif

#ifdef CONFIG_SMP_NCPUS
#  define MONITOR_NCPUS       CONFIG_SMP_NCPUS
#else
#  define MONITOR_NCPUS       1
#endif

#define MONITOR_MAX_TASKS     48
#define MONITOR_CPU_Y         320
#define MONITOR_CPU_H         96
#define MONITOR_BAR_X         150
#define MONITOR_BAR_W         620
#define MONITOR_MEM_Y         (MONITOR_CPU_Y + MONITOR_NCPUS * \
                               MONITOR_CPU_H + 60)
#define MONITOR_TASKS_Y       (MONITOR_MEM_Y + 230)
#define MONITOR_COLUMNS       7

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct monitor_task_s
{
  int pid;
  int prio;
  int load;                 /* 0.1% */
  int stack_used;
  int stack_size;
  char cpu[4];
  char name[24];
  char state[12];
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const char * const g_columns[MONITOR_COLUMNS] =
{
  "PID", "NAME", "PRI", "CPU", "STATE", "LOAD", "STACK"
};

static const char * const g_mem_tags[] =
{
  "TOTAL", "FREE", "LARGEST BLOCK"
};

static const int32_t g_column_w[MONITOR_COLUMNS] =
{
  90, 270, 90, 80, 170, 110, 126
};

static struct monitor_task_s g_tasks[MONITOR_MAX_TASKS];

static lv_obj_t *g_cpu_bar[MONITOR_NCPUS];
static lv_obj_t *g_cpu_value[MONITOR_NCPUS];
static lv_obj_t *g_mem[nitems(g_mem_tags)];
static lv_obj_t *g_table;
static lv_obj_t *g_note;
static bool g_running;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Read a small procfs file into buf, returns the length or -1 */

static int monitor_read(int pid, const char *file, char *buf, size_t size)
{
  char path[64];
  int fd;
  int n;

  snprintf(path, sizeof(path), MONITOR_PROC "/%d/%s", pid, file);
  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      return -1;
    }

  n = read(fd, buf, size - 1);
  close(fd);
  buf[MAX(n, 0)] = '\0';
  return n;
}

/* Value of a "Key:   value" line, copied up to a ',' or the line end */

static void monitor_field(const char *buf, const char *key, char *out,
                          size_t size)
{
  const char *p = strstr(buf, key);
  size_t n = 0;

  out[0] = '\0';
  if (p == NULL)
    {
      return;
    }

  for (p += strlen(key); *p == ' ' || *p == '\t'; p++);

  while (n < size - 1 && p[n] != '\0' && p[n] != '\n' && p[n] != ',')
    {
      out[n] = p[n];
      n++;
    }

  out[n] = '\0';
}

/* "  12.3%" -> 123, -1 without load measurement */

static int monitor_load(int pid)
{
  char buf[32];
  int whole = 0;
  int tenth = 0;

  if (monitor_read(pid, "loadavg", buf, sizeof(buf)) <= 0 ||
      sscanf(buf, "%d.%d", &whole, &tenth) < 1)
    {
      return -1;
    }

  return whole * 10 + tenth;
}

/* Busiest first, the IDLE tasks last as the CPU bars already show them */

static int monitor_by_load(const void *a, const void *b)
{
  const struct monitor_task_s *x = a;
  const struct monitor_task_s *y = b;
  int lx = x->pid < MONITOR_NCPUS ? -2 : x->load;
  int ly = y->pid < MONITOR_NCPUS ? -2 : y->load;

  return lx != ly ? ly - lx : x->pid - y->pid;
}

static int monitor_read_tasks(void)
{
  struct monitor_task_s *t;
  struct dirent *entry;
  char buf[512];
  char value[16];
  DIR *dir;
  int n = 0;

  dir = opendir(MONITOR_PROC);
  if (dir == NULL)
    {
      return 0;
    }

  while ((entry = readdir(dir)) != NULL && n < MONITOR_MAX_TASKS)
    {
      if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
        {
          continue;
        }

      t = &g_tasks[n];
      t->pid = atoi(entry->d_name);
      if (monitor_read(t->pid, "status", buf, sizeof(buf)) <= 0)
        {
          continue;
        }

      monitor_field(buf, "Name:", t->name, sizeof(t->name));
      monitor_field(buf, "CPU:", t->cpu, sizeof(t->cpu));
      monitor_field(buf, "State:", t->state, sizeof(t->state));
      monitor_field(buf, "Priority:", value, sizeof(value));
      t->prio = atoi(value);

      t->stack_used = 0;
      t->stack_size = 0;
      if (monitor_read(t->pid, "stack", buf, sizeof(buf)) > 0)
        {
          monitor_field(buf, "StackSize:", value, sizeof(value));
          t->stack_size = atoi(value);
          monitor_field(buf, "StackUsed:", value, sizeof(value));
          t->stack_used = atoi(value);
        }

      t->load = monitor_load(t->pid);
      n++;
    }

  closedir(dir);
  qsort(g_tasks, n, sizeof(g_tasks[0]), monitor_by_load);
  return n;
}

static void monitor_update_cb(lv_timer_t *timer)
{
  struct mallinfo mi = mallinfo();
  struct monitor_task_s *t;
  char text[24];
  int load;
  int n;
  int i;

  UNUSED(timer);

  /* CPU load from the IDLE task of each CPU */

  for (i = 0; i < MONITOR_NCPUS; i++)
    {
      load = monitor_load(i);
      if (load < 0)
        {
          lv_label_set_text(g_cpu_value[i], "N/A");
          lv_obj_remove_flag(g_note, LV_OBJ_FLAG_HIDDEN);
          continue;
        }

      load = 1000 - MIN(load, 1000);
      lv_label_set_text_fmt(g_cpu_value[i], "%d%%", (load + 5) / 10);
      lv_obj_set_width(g_cpu_bar[i], load * MONITOR_BAR_W / 1000);
    }

  /* Memory in MB with one decimal */

  lv_label_set_text_fmt(g_mem[0], "%d.%dM", mi.arena >> 20,
                        (mi.arena >> 10) % 1024 * 10 / 1024);
  lv_label_set_text_fmt(g_mem[1], "%d.%dM", mi.fordblks >> 20,
                        (mi.fordblks >> 10) % 1024 * 10 / 1024);
  lv_label_set_text_fmt(g_mem[2], "%d.%dM", mi.mxordblk >> 20,
                        (mi.mxordblk >> 10) % 1024 * 10 / 1024);

  /* Tasks, busiest first */

  n = monitor_read_tasks();
  lv_table_set_row_count(g_table, n + 1);

  for (i = 0; i < n; i++)
    {
      t = &g_tasks[i];
      lv_table_set_cell_value_fmt(g_table, i + 1, 0, "%d", t->pid);
      lv_table_set_cell_value(g_table, i + 1, 1, t->name);
      lv_table_set_cell_value_fmt(g_table, i + 1, 2, "%d", t->prio);
      lv_table_set_cell_value(g_table, i + 1, 3, t->cpu);
      lv_table_set_cell_value(g_table, i + 1, 4, t->state);

      if (t->load < 0)
        {
          snprintf(text, sizeof(text), "-");
        }
      else
        {
          snprintf(text, sizeof(text), "%d.%d%%", t->load / 10,
                   t->load % 10);
        }

      lv_table_set_cell_value(g_table, i + 1, 5, text);

      if (t->stack_used > 0)
        {
          snprintf(text, sizeof(text), "%d%%",
                   t->stack_used * 100 / MAX(t->stack_size, 1));
        }
      else
        {
          snprintf(text, sizeof(text), "%dK", t->stack_size >> 10);
        }

      lv_table_set_cell_value(g_table, i + 1, 6, text);
    }
}

/****************************************************************************
 * Name: user interface
 ****************************************************************************/

static void monitor_home_cb(lv_event_t *e)
{
  UNUSED(e);
  g_running = false;
}

/* Header row in small spaced capitals, only name and load stand out */

static void monitor_table_draw_cb(lv_event_t *e)
{
  lv_draw_task_t *task = lv_event_get_draw_task(e);
  lv_draw_dsc_base_t *base = task->draw_dsc;
  lv_draw_label_dsc_t *label = lv_draw_task_get_label_dsc(task);

  if (base->part != LV_PART_ITEMS || label == NULL)
    {
      return;
    }

  if (base->id1 == 0)
    {
      label->letter_space = 3;
    }

  if (base->id1 == 0 || (base->id2 != 1 && base->id2 != 5))
    {
      label->color = lv_color_hex(UI_MUTED);
    }
}

static void monitor_create(void)
{
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *obj;
  int32_t y;
  int i;

  lv_obj_remove_style_all(scr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, lv_color_hex(UI_INK), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_set_pos(ui_back(scr, UI_INK_2, monitor_home_cb), UI_MARGIN, 56);
  ui_place(ui_label(scr, UI_OSWALD_120, 0xffffff, "MONITOR"), UI_OSWALD_120,
           222, 50);

  /* One bar per CPU */

  ui_place(ui_tag(scr, UI_MUTED, "CPU LOAD"), UI_OUTFIT_26, UI_MARGIN,
           263);
  for (i = 0; i < MONITOR_NCPUS; i++)
    {
      y = MONITOR_CPU_Y + i * MONITOR_CPU_H;

      obj = ui_label(scr, UI_OUTFIT_34, 0xffffff, "");
      lv_label_set_text_fmt(obj, "CPU%d", i);
      ui_place(obj, UI_OUTFIT_34, UI_MARGIN, y + 20);

      obj = ui_box(scr, UI_INK_2, 0);
      lv_obj_set_pos(obj, UI_MARGIN + MONITOR_BAR_X, y + 12);
      lv_obj_set_size(obj, MONITOR_BAR_W, 40);
      g_cpu_bar[i] = ui_box(obj, UI_ORANGE, 0);
      lv_obj_set_size(g_cpu_bar[i], 0, 40);

      g_cpu_value[i] = ui_label(scr, UI_OSWALD_64, 0xffffff, "--");
      lv_obj_align(g_cpu_value[i], LV_ALIGN_TOP_RIGHT, -UI_MARGIN,
                   y + 6 - ui_ink_top(UI_OSWALD_64));
    }

  g_note = ui_label(scr, UI_OUTFIT_26, UI_DIM,
                    "Per CPU load needs SCHED_CPULOAD_SYSCLK");
  lv_obj_add_flag(g_note, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(g_note, LV_ALIGN_TOP_RIGHT, -UI_MARGIN,
               263 - ui_ink_top(UI_OUTFIT_26));

  /* Memory: total, free and largest free block */

  ui_place(ui_tag(scr, UI_MUTED, "MEMORY"), UI_OUTFIT_26, UI_MARGIN,
           MONITOR_MEM_Y);
  for (i = 0; i < nitems(g_mem_tags); i++)
    {
      g_mem[i] = ui_label(scr, UI_OSWALD_64, 0xffffff, "--");
      ui_place(g_mem[i], UI_OSWALD_64, UI_MARGIN + i * 312,
               MONITOR_MEM_Y + 60);
      ui_place(ui_tag(scr, UI_MUTED, g_mem_tags[i]), UI_OUTFIT_26,
               UI_MARGIN + i * 312, MONITOR_MEM_Y + 140);
    }

  /* Task list, scrolls when it is longer than the screen */

  ui_place(ui_tag(scr, UI_MUTED, "TASKS BY LOAD"), UI_OUTFIT_26, UI_MARGIN,
           MONITOR_TASKS_Y);

  g_table = lv_table_create(scr);
  lv_obj_set_pos(g_table, UI_MARGIN, MONITOR_TASKS_Y + 50);
  lv_obj_set_size(g_table, UI_CONTENT_W,
                  UI_SCREEN_H - MONITOR_TASKS_Y - 50 - 40);
  lv_obj_set_style_bg_opa(g_table, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_table, 0, 0);
  lv_obj_set_style_pad_all(g_table, 0, 0);
  lv_obj_set_style_bg_opa(g_table, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_border_side(g_table, LV_BORDER_SIDE_BOTTOM,
                               LV_PART_ITEMS);
  lv_obj_set_style_border_color(g_table, lv_color_hex(UI_INK_2),
                                LV_PART_ITEMS);
  lv_obj_set_style_border_width(g_table, 2, LV_PART_ITEMS);
  lv_obj_set_style_pad_hor(g_table, 0, LV_PART_ITEMS);
  lv_obj_set_style_pad_ver(g_table, 14, LV_PART_ITEMS);
  lv_obj_set_style_text_font(g_table, UI_OUTFIT_26, LV_PART_ITEMS);
  lv_obj_set_style_text_color(g_table, lv_color_white(), LV_PART_ITEMS);
  lv_obj_set_scrollbar_mode(g_table, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_event_cb(g_table, monitor_table_draw_cb,
                      LV_EVENT_DRAW_TASK_ADDED, NULL);
  lv_obj_add_flag(g_table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

  lv_table_set_column_count(g_table, MONITOR_COLUMNS);
  for (i = 0; i < MONITOR_COLUMNS; i++)
    {
      lv_table_set_column_width(g_table, i, g_column_w[i]);
      lv_table_set_cell_value(g_table, 0, i, g_columns[i]);
    }

  monitor_update_cb(NULL);
  lv_timer_create(monitor_update_cb, 1000, NULL);
}

static int monitor_start(lv_nuttx_result_t *result)
{
  lv_nuttx_dsc_t info;

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, result);

  if (result->disp == NULL || result->indev == NULL)
    {
      fprintf(stderr, "monitor: no display or touchscreen\n");
      return -1;
    }

  monitor_create();
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_result_t result;
  uint32_t idle;

  UNUSED(argc);
  UNUSED(argv);

  /* LVGL is one global instance, shared by every task of the flat build */

  if (lv_is_initialized())
    {
      fprintf(stderr, "monitor: LVGL is busy, start it from the "
              "launcher\n");
      return 1;
    }

  g_running = monitor_start(&result) == 0;
  while (g_running)
    {
      idle = lv_timer_handler();
      usleep(MAX(idle, 1) * 1000);
    }

  lv_nuttx_deinit(&result);
  lv_deinit();
  return 0;
}
