# 桌面 v2 待办

## 计划（README 4.7–4.11，不需要板子的先做）

1. launcher：`LVX_LAUNCHER_PINNED` 决定桌面磁贴，加 `ALL APPS` 磁贴和全部应用页
   （TEAM 349 / OPENVELA / LVGL 三组，两列卡片，能滚动）。应用表按条目 id 查：
   程序名、启动参数、分组、颜色、图标、说明、标签；`LVX_LAUNCHER_APPS` 里写条目
   id，查不到的 id 当作不带参数的内置程序。从全部应用页启动的应用退出后回到这一页
2. touch：按 bench 的深色风格。GT9xx 驱动每次只报一个点（`npoints = 1`），所以
   是单点轨迹，多点要改驱动，这次不做
3. lvdemo：`lvdemo benchmark` / `lvdemo widgets`，角上叠一个返回按钮；benchmark
   结束时把汇总打到串口。openvela 自带的应用都是 `while (1)` 或 `uv_run` 死循环，
   自己退不出来。不改它们的源码，lvdemo 直接调它们创建界面的函数
   （`snake_game_create`、`whackmole_app_create`……），自己跑循环、叠返回按钮、
   退出时释放 LVGL；这些应用自己的 Kconfig 不打开，免得重复符号。候选：贪吃蛇、
   打地鼠、象棋（象棋只有主循环用 libuv）。计算器要 libuv + C++，木鱼要音频，
   不接
4. LVGL benchmark 要 `LV_USE_PERF_MONITOR`，打开后 sysmon 会给每个 LVGL 应用自动
   叠一个性能浮层（sys layer），刷新定时器也不再暂停。我们自己的应用把 sys
   layer 藏起来，lvdemo 里保留
5. TFLite Micro：TensorFlow 在本机临时虚拟环境里训练同结构的模型（同样的
   数据和增强），全 int8 量化成 `.tflite`，Python 里先验准确率，再转成 C 数组。
   识别代码（手写推理、TFLM 封装、两份模型、测试样本）挪到公共库 `app/digit`，
   paint 和 bench 共用；bench 加一项：两种实现单次推理耗时和同一批样本上的准确率。
   TFLM 依赖 `HAVE_CXX` + `LIBCXX`、flatbuffers、gemmlowp、kissfft、ruy（参考
   `sim:tflm`），源码都在工作区里。先确认 arm64 能编过；接不进来就保留手写版本，
   把问题记下来
6. defconfig 分开提交：新应用和 `LVX_LAUNCHER_PINNED`；TFLM 和 C++；LVGL 示例、
   sysmon 和日志。manifest 加 `app/touch`、`app/lvdemo`、`app/digit`，编译机上手动
   建软链
7. 预览：全部应用页、touch、lvdemo（含 openvela 应用）都出图；README 和待办更新

## 需要决定

- [x] **字库转换工具**：用全局安装的 lv_font_conv 1.5.2 生成（10/22）
- [x] **镜像传到 TFTP**：`scp -3` 已经可以用（10/22）
- [ ] **清华 PyPI 镜像**：`pypi.tuna.tsinghua.edu.cn` 仍被拦（可能白名单写的是
  别的域名）。目前用 pypi.org 装 TensorFlow
- [ ] **驱动只拷脏区域的列**（帧率第 2 项的一部分）在 `arch/arm64/src/s6`，要
  走 nuttx fork 的 PR

## 上板验证（每次先看串口：启动正常、`launcher [6:100]`、没有 assert）

- [x] `reboot` 能回到 U-Boot（`s6_bq201#`），以后上板不用按复位（10/22）
- [x] TFLM + libc++ 版本启动正常：`launcher [6:100]`、`nsh>`，无 assert；堆 30.7MB
  剩 22.3MB，4 个 IDLE 各约 25%，栈染色生效（10/22）
- 注意：板子重新上电后 `vela-tftp` 的 `192.168.10.1` 和 100M 限速会丢，按
  CONNECT.md 第 6 节恢复
- [ ] 基线：开机日志、`free`、`ps`（编译机上 `~/contest2026/s6-nuttx-base.bin`
  是改动前的镜像）
- [ ] 锁屏：三种解锁方式；拖不够 240px 松手会弹回
- [ ] 桌面：数字每秒滚动；FPS 触摸时显示数字、静止 1 秒后显示 IDLE；从桌面
  LOCK 条重新锁屏
- [ ] About：进入动画、帧率和堆
- [ ] paint、bench、monitor 各打开 / 退出 10 次，前后 `free` 不持续下降
- [ ] paint：0–9 每个写 3 遍，记录结果（串口有 28×28 输入和前三名）；快速划线
  不断；橡皮、粗细、清空
- [ ] bench：5 个场景和 CoreMark 跑完，串口的 `bench:` 行记下来作为帧率基线；
  CoreMark 期间 NSH 能输入
- [ ] monitor：4 个核的负载会变（NSH 后台跑 `smp` 或 CoreMark 时）
- [ ] 全部应用页：分组、滚动，启动的应用退出后回到这一页
- [ ] paint 的识别跑在 TFLM 上；bench 里 TFLM 和手写版本的耗时、准确率对比
- [ ] `lvdemo benchmark` 跑完，串口有总分和各项；`lvdemo widgets` 能操作、能退出
- [ ] 接进来的 openvela 应用布局正常，能退出回到 launcher，退出后 `free` 不降
- [ ] touch：轨迹、坐标、报点率，校准检查给出偏差
- [ ] 全程 NSH 能输入

## 帧率（README 第五节）

- 刷新周期只看 `CONFIG_LV_DEF_REFR_PERIOD`。`CONFIG_LV_NUTTX_VSYNC_TIMER_PERIOD`
  只在 libuv 模式下用到，我们的应用不走 libuv。bench 显示的帧率上限按
  `LV_DEF_REFR_PERIOD` 算
- fbdev 每帧把所有脏区域的**包围盒**交给 `FBIO_UPDATE`，s6 驱动再拷包围盒覆盖
  的整行并清 dcache；驱动和 `fb.c` 里都没有关中断或全局锁
- 逐项试，每项都用 bench 记录前后的 `bench:` 输出：关 NOOPT
  （`CONFIG_DEBUG_FULLOPT`）→ LVGL 多渲染单元（`LV_USE_OS` +
  `LV_DRAW_SW_DRAW_UNIT_CNT`）→ NEON（`LV_USE_DRAW_SW_ASM`）→ 驱动只拷脏列
- 16ms 下 NSH 卡死，先做这两个实验：
  1. 16ms 镜像里打开 monitor 看 `nsh_main`：一直 Ready 说明被同为 100 优先级、
     渲染跟不上时几乎不睡的 launcher 饿住了；一直 Waiting 说明字符没送到，查
     UART 中断和 RX 溢出
  2. launcher 优先级降到 NSH 之下，看 NSH 是否恢复
