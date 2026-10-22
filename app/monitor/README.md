# monitor（系统监视）

从桌面启动，每秒刷新一次：

- 4 个核各自的负载。NuttX 的 `/proc/cpuload` 只有总负载，但每个核有自己的
  IDLE 任务（PID 0–3），单核负载 = 100% 减去该核 IDLE 任务的占用
- 内存：总量、剩余、最大空闲块（`mallinfo`）
- 任务列表：PID、名称、优先级、所在 CPU、状态、占用、栈使用，按占用排序，
  IDLE 任务排在最后，可以上下滚动

需要 `CONFIG_FS_PROCFS` 和 CPU 负载统计（`CONFIG_SCHED_CPULOAD_SYSCLK`），
栈使用需要 `CONFIG_STACK_COLORATION`，没有时只显示栈大小。
