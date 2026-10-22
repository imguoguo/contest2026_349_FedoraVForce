# lvdemo（LVGL 示例和 openvela 应用）

LVGL 自带的示例和 `packages/demos` 里的应用都没有退出的路，lvdemo 把它们包成
一个应用，按参数启动其中一个，左下角叠一个返回按钮，退出时照常释放 LVGL 回到
launcher。全部应用页里的 LVGL BENCH、WIDGETS、SNAKE、MOLES 卡片启动的都是它。

```text
lvdemo benchmark [exit]   LVGL 官方 benchmark，带 exit 时跑完自动退出
lvdemo widgets            LVGL 控件演示
lvdemo snake              贪吃蛇（packages/demos/snake_game）
lvdemo whackmole          打地鼠（packages/demos/Whackmole）
```

LVGL 的性能浮层（右下角）在 lvdemo 里保留。

## LVGL benchmark

需要 `CONFIG_LV_USE_DEMO_BENCHMARK`、`CONFIG_LV_USE_DEMO_WIDGETS`（benchmark
最后一个场景就是 widgets）、`CONFIG_LV_USE_PERF_MONITOR`（各场景的帧率和耗时
从它来），以及 Montserrat 14、24。跑完后屏幕上是汇总表，同时用 `LV_LOG` 把
CSV 格式的汇总打到串口，所以还要 `CONFIG_LV_USE_LOG`（级别 USER 就够）。
lvdemo 把 LVGL 的日志直接写到标准输出：

```text
Benchmark Summary (<LVGL 版本>)
Name, Avg. CPU, Avg. FPS, Avg. time, render time, flush time
<场景名>, <CPU>%, <FPS>, <每帧 ms>, <渲染 ms>, <flush ms>
...
All scenes avg.,<CPU>%, <FPS>, <每帧 ms>, <渲染 ms>, <flush ms>
```

示例把各场景的统计累加在一个静态表里，自己从不清零。同一次开机里第二次跑，
汇总是所有次的平均，lvdemo 会在串口提醒；要干净的结果就重启后跑一次。

## openvela 应用

这两个应用的 `main` 都是死循环，所以不单独编译它们，而是在 lvdemo 里直接
include 它们的源文件，调它们建界面的函数（`snake_game_create`、
`whackmole_app_create`），源码不改。它们自己的 Kconfig 开关不能同时打开，否则
符号重复。

- 贪吃蛇：原版按 1200px 宽的横屏设计（60×35 格，每格 20px），竖屏上两边会被
  裁掉。lvdemo 先 include 它的头文件、改成 52×76 格，再 include `.c`（头文件
  有 include guard，`.c` 里不会再覆盖），正好铺满竖屏。点 START 后蛇自动跑。
  它的状态是静态变量，每次启动前清零，退出时释放蛇身
- 打地鼠：原版按 4:3 横屏设计，背景和洞按屏幕百分比拉伸，竖屏上会变形。
  lvdemo 把游戏放到屏幕中间 1080×810 的 4:3 区域，按它自己的洞位置表重新
  摆洞，锤子挪到屏幕上跟手，START 按钮放大。它的定时器和对象指针是静态变量，
  不清零的话第二次启动会删已经不存在的定时器，所以每次启动前清零

没接的：象棋要从 `/sdcard` 读 15 张 PNG，需要文件系统和 PNG 解码；计算器要
libuv 和 C++ 界面库；木鱼要音频；其它的要联网。
