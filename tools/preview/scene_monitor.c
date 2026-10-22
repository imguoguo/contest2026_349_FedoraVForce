/****************************************************************************
 * tools/preview/scene_monitor.c
 *
 * Monitor on a fake procfs in out/proc that looks like the board's.
 ****************************************************************************/

#include <sys/stat.h>

#define MONITOR_PROC "out/proc"
#define main monitor_main
#include "../../app/monitor/monitor_main.c"
#undef main

static const struct
{
  int pid;
  const char *name;
  int prio;
  int cpu;
  const char *state;
  const char *load;
  int stack;
  int used;
}
g_fake[] =
{
  { 0, "CPU0 IDLE", 0, 0, "Assigned", "38.0%", 8192, 1200 },
  { 1, "CPU1 IDLE", 0, 1, "Assigned", "71.5%", 8192, 1100 },
  { 2, "CPU2 IDLE", 0, 2, "Assigned", "96.0%", 8192, 1100 },
  { 3, "CPU3 IDLE", 0, 3, "Running", "4.0%", 8192, 1100 },
  { 4, "hpwork", 224, 1, "Waiting,Semaphore", "0.1%", 2048, 600 },
  { 5, "nsh_main", 100, 0, "Waiting,Semaphore", "0.0%", 8192, 2300 },
  { 6, "launcher", 100, 2, "Waiting,Signal", "0.0%", 32768, 5100 },
  { 9, "monitor", 100, 0, "Running", "61.2%", 16384, 4100 },
  { 10, "coremark", 50, 3, "Running", "95.8%", 8192, 1900 },
  { 11, "gt9xx", 100, 1, "Waiting,Semaphore", "1.4%", 2048, 700 },
};

static void fake_file(int pid, const char *file, const char *text)
{
  char path[64];
  FILE *f;

  snprintf(path, sizeof(path), MONITOR_PROC "/%d", pid);
  mkdir(path, 0755);
  snprintf(path, sizeof(path), MONITOR_PROC "/%d/%s", pid, file);
  f = fopen(path, "w");
  fputs(text, f);
  fclose(f);
}

int main(void)
{
  lv_nuttx_result_t result;
  char buf[256];
  int i;

  mkdir(MONITOR_PROC, 0755);
  for (i = 0; i < nitems(g_fake); i++)
    {
      snprintf(buf, sizeof(buf),
               "Name:       %s\nType:       Task\nGroup:      %d\n"
               "CPU:        %d\nState:      %s\nFlags:      ---\n"
               "Priority:   %d\nScheduler:  SCHED_FIFO\n",
               g_fake[i].name, g_fake[i].pid, g_fake[i].cpu,
               g_fake[i].state, g_fake[i].prio);
      fake_file(g_fake[i].pid, "status", buf);
      snprintf(buf, sizeof(buf), "%6s\n", g_fake[i].load);
      fake_file(g_fake[i].pid, "loadavg", buf);
      snprintf(buf, sizeof(buf), "StackBase:  0x3100000\n"
               "StackSize:  %d\nStackUsed:  %d\n", g_fake[i].stack,
               g_fake[i].used);
      fake_file(g_fake[i].pid, "stack", buf);
    }

  monitor_start(&result);
  preview_step(1500);
  preview_shot("6-monitor.png");

  lv_nuttx_deinit(&result);
  lv_deinit();
  return 0;
}
