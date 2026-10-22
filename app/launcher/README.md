# launcher（LVGL 桌面）

开机由 `rcS` 自动启动，包括锁屏、仪表盘式桌面和 About 页。

- 锁屏：时钟显示开机以来的运行时间。解锁：把橙色块往上拖过 240px 再松手、
  直接点橙色块，或在屏幕任意位置上滑；拖不够时 260ms 弹回。桌面最下面的
  LOCK 条重新锁屏
- 桌面：运行时间、剩余堆、帧率，下面是应用磁贴
- About：板子和移植信息、实时帧率和堆

帧率每秒统计一次 LVGL 实际画了东西的刷新次数（`LV_EVENT_RENDER_READY`），
不是定时器触发的次数。超过 1 秒没有触摸时显示 IDLE。

## 应用和磁贴

- `CONFIG_LVX_LAUNCHER_APPS`（板级 defconfig）列出编进来的全部应用条目，例如
  `"paint bench monitor touch snake whackmole lvgl_benchmark lvgl_widgets"`。
  没有编进镜像的应用不显示
- `CONFIG_LVX_LAUNCHER_PINNED` 是其中放到桌面磁贴上的几个，默认
  `"paint bench monitor"`。About 固定排第二个，最后一个是 `ALL APPS`。三个
  以上时第一个是左边的竖长磁贴，第二、三个在右边上下排，后面两个一行，多出
  的单个占整行，超出屏幕可以滚动
- 全部应用页按 TEAM 349 / OPENVELA / LVGL 分组，两列卡片。从这里打开的应用
  退出后回到这一页

条目的程序名、启动参数、分组、颜色、图标、说明和标签在 `launcher_main.c` 的
`g_apps` 表里按条目名查：比如 `lvgl_benchmark` 启动的是 `lvdemo benchmark`。
查不到的条目当作同名的内置程序，用深色默认样式，标题是大写的名字。

## 从串口启动应用

打开 `CONFIG_PIPES` 时，launcher 建一个命名管道 `/dev/launcher`，往里写
“条目名 [参数…]”就像点了那张卡片，写了参数时替换条目自己的参数。用来在没人
操作屏幕时跑测试：

```text
nsh> echo bench run > /dev/launcher                       # 跑完 bench 全部项目后退出
nsh> echo lvgl_benchmark benchmark exit > /dev/launcher   # LVGL benchmark，跑完退出
```

应用运行时写进去的命令，等回到 launcher 后才执行。

启动应用前 launcher 会先释放 LVGL，等应用退出后再重新初始化。同一时间只能
运行一个 LVGL 应用，所以应用要从桌面打开。新加的应用退出前需要调用
`lv_nuttx_deinit` 和 `lv_deinit`。

页面可以在电脑上预览，见 `tools/preview`。
