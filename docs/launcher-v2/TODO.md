# 桌面 v2 待办

## 需要决定

- [ ] **字库转换工具**。README 要求用 lv_font_conv，但本机 npm 源不在网络白名单
  里，从 GitHub 拼起来的 lv_font_conv 又被禁止在本机执行。二选一：放行
  lv_font_conv（`app/ui/tools/gen_fonts.sh` 已写好），或者改用 fontTools +
  Pillow 写的转换脚本（`tools/preview/ttf2lv.py` 就是，目前只给预览用）。字库
  生成前，板上镜像里的 Oswald/Outfit 都是 Montserrat 占位
- [ ] **镜像传到 TFTP**。`scp -3 vela-build:… vela-tftp:…` 和 CONNECT.md 写法一致，
  但不在沙箱白名单里
- [ ] **驱动只拷脏区域的列**（帧率第 2 项的一部分）在 `arch/arm64/src/s6`，要
  走 nuttx fork 的 PR

## 上板验证（每次先看串口：启动正常、`launcher [6:100]`、没有 assert）

- [ ] `reboot` 能回到 U-Boot（`s6_bq201#`），以后上板不用按复位
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
