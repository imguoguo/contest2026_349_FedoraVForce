# 帧率记录

README 第五节。每一步都用 bench 跑同样的 5 个场景，记录串口里的 `bench:` 行：
FPS 是真实刷新次数，ms/frame 是性能计数器测的每帧耗时（其中 flush 是驱动拷贝
那一段）。FPS 会被刷新周期卡住，看优化效果主要看 ms/frame。

## 0. 已知情况（读代码）

- 刷新周期只由 `CONFIG_LV_DEF_REFR_PERIOD` 决定；`LV_NUTTX_VSYNC_TIMER_PERIOD` 只在
  libuv 模式下用到
- fbdev 每帧把脏区域的包围盒交给 `FBIO_UPDATE`，s6 驱动把包围盒覆盖的整行拷到
  扫描缓冲再清 dcache。`s6_dsi_updatearea`、`fb.c` 的 `FBIO_UPDATE`、
  `up_clean_dcache` 里都没有关中断或拿锁；要用 critmon / irqmonitor 在板上实测确认

## 1. 基线：33ms，DEBUG_NOOPT（-O0）

| 场景 | FPS | ms/frame | flush ms |
| --- | --- | --- | --- |
| Rectangles | 29 | 13.0 | 10.9 |
| Rounded + border | 29 | 19.1 | 10.9 |
| Text | 29 | 16.2 | 10.6 |
| Arcs | 28 | 24.8 | 6.0 |
| Full-screen slide | 29 | 19.0 | 12.6 |

CoreMark 2233 iterations/s（单核）。镜像 `87ccd38`，10/22。

- flush（驱动把整行拷进扫描缓冲并清 dcache）每帧 6–12.6ms，是大头。整屏滑动
  约 4MB / 12.6ms ≈ 330MB/s，对 A510 偏慢，-O0 编出来的 memcpy 可能是原因之一
- Arcs 每帧 24.8ms，已经超过 16ms 周期

## 2. 33ms，-O2（关 DEBUG_NOOPT）

`CONFIG_DEBUG_CUSTOMOPT=y`（`DEBUG_OPTLEVEL` 默认 `-O2`）。注意 arm64 上
`DEBUG_FULLOPT` 是 `-Os`，不是 `-O2`。10/22。

| 场景 | FPS | ms/frame | flush ms | 对比 -O0 |
| --- | --- | --- | --- | --- |
| Rectangles | 29 | 6.7 | 6.1 | 13.0 → 6.7 |
| Rounded + border | 29 | 8.4 | 6.1 | 19.1 → 8.4 |
| Text | 29 | 7.7 | 5.9 | 16.2 → 7.7 |
| Arcs | 29 | 8.0 | 3.3 | 24.8 → 8.0 |
| Full-screen slide | 29 | 8.7 | 7.0 | 19.0 → 8.7 |

CoreMark 10847 iterations/s（-O0 是 2233，4.9 倍）。镜像 2.69MB → 2.23MB。

- 每帧耗时都在 9ms 以内，FPS 还是被 33ms 卡在 29
- flush 现在占每帧的 70–80%，整屏约 4MB / 7.0ms ≈ 590MB/s；下一步“驱动只拷
  脏区域的列”会有效果

## 3. 16ms 下 NSH 失去响应

先查“驱动刷新时长时间关中断或持锁、挡住串口中断”这个怀疑（10/22，33ms，-O0）：

- `SCHED_CRITMONITOR` 打开后跑 bench：每个 CPU 持有临界区最长 16.25µs，自旋锁
  忙等最长 2.8µs。显示驱动拷贝每帧 6–12ms，但全程开着中断，挡不住串口中断。
  这个怀疑在 33ms 下可以排除。注意：busywait 的阈值要设成很大的数，设 0 会在
  第一次忙等时就 `CRITMONITOR_PANIC`
- 临时给 `s6_serial.c` 加了调试（不提交）：每 5 秒直接写 FIFO 打出 CONTROL、
  STATUS、收发缓冲区头尾、中断次数和接收错误次数。期间看到的几次“NSH 不响应”，
  数据显示驱动收到的每个字节都被 NSH 读走并回显了，接收错误 0 次，NSH 也有
  `nsh>` 提示符。那是看串口的方式（只截 screen 窗口最后几行，超长输入把光标
  弄乱）造成的误判，不是卡死。以后读串口要看整个窗格
- STATUS 的 `TX_WERR`（往满的发送 FIFO 写）开机后就是 1，原因待查，目前看不出
  影响
- `s6_uart_receive()` 清错误位是裸的读-改-写，没走 `modifyreg32` 的锁，理论上
  会和别的核开关中断使能位冲突。实测没有发生过接收错误，不是这几次现象的
  原因，留作驱动的改进点
- 16ms（-O2）下没有复现：跑 bench 的整个过程和之后，NSH 一直能响应
  （`uname`、`free`），板上操作也流畅（10/22）。之前在 -O0 下的那次卡死没有
  再现，原因没有查到；当时每帧要 13–25ms，比 16ms 周期还长，launcher 主循环几乎
  不睡，也许和这个有关。以后如果再遇到，先用 monitor 看 `nsh_main` 的状态

## 4. 16ms，-O2

`CONFIG_LV_DEF_REFR_PERIOD=16`。10/22。

| 场景 | FPS | ms/frame | flush ms |
| --- | --- | --- | --- |
| Rectangles | 58 | 6.7 | 6.1 |
| Rounded + border | 58 | 8.4 | 6.1 |
| Text | 58 | 7.6 | 5.8 |
| Arcs | 58 | 8.0 | 3.3 |
| Full-screen slide | 58 | 8.7 | 7.0 |

CoreMark 10824 iterations/s，和 33ms 时一样，界面多刷新的那些帧没有拖慢它
（它在 CPU3 上）。每帧耗时没变，FPS 翻倍到 58，被 16ms 周期卡住。
