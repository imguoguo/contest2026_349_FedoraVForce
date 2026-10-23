# 桌面 v2 待办

## 计划（README 4.7–4.11）

代码都已完成并在编译机上编过（0 warning，nxstyle 通过），预览出图，等上板验证：

- [x] launcher：`LVX_LAUNCHER_PINNED` 磁贴、`ALL APPS` 磁贴和全部应用页
- [x] TFLite Micro：paint 的识别跑在 TFLM 上（10/22 板上认对一个 3）。识别代码、
  两份模型、500 张 MNIST 样本和训练工具在公共库 `app/digit`（paint、bench 共用，
  两个应用各编一份会重复定义符号）
- [x] bench：CPU 页加 TFLM 和手写推理的单次耗时、准确率对比；`bench run`
  跑完全部项目后退出
- [x] touch：轨迹、坐标、报点率、校准检查。GT9XX 驱动只报一个点
- [x] lvdemo：LVGL benchmark（汇总打到串口，`exit` 参数跑完自动退出）、
  widgets、贪吃蛇、打地鼠。openvela 应用不改源码：lvdemo 直接 include 它们，
  贪吃蛇改成竖屏 52×76 格，打地鼠放进中间 4:3 区域。象棋要读 `/sdcard` 上的
  PNG，没接
- [x] launcher 的 `/dev/launcher` 命名管道：在 NSH 里写“条目名 [参数]”启动应用，
  用来远程跑测试
- [x] defconfig：新应用和应用列表一个提交；LVGL 示例、性能监视、日志一个提交
- [x] monitor 的状态列显示等待的事件（Semaphore、Signal），不再只有 Waiting
- [x] bench、monitor、paint 隐藏 LVGL 性能浮层（打开 `LV_USE_PERF_MONITOR` 后
  每个应用都会有）

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
- [ ] 多点触控（nuttx 分支 `gt9xx-multitouch`，编译机上
  `~/contest2026/s6-nuttx-multitouch.bin`）：touch 里几根手指各一条轨迹、
  手指数对；launcher、paint 等单点使用不受影响（两指时以第一根为准），
  快速点按钮不丢；确认后再推到 fork、提 PR
- [ ] 全程 NSH 能输入

远程能做的（不需要人按屏幕，`/dev/launcher`）：

```text
nsh> echo bench run > /dev/launcher                      # bench 全部项目，串口 bench: 行
nsh> echo lvgl_benchmark benchmark exit > /dev/launcher  # LVGL benchmark 汇总
nsh> echo touch > /dev/launcher                          # 打开应用，看 free 前后
```

## 帧率（README 第五节，数据在 fps.md）

- [x] 基线 33ms -O0；-O2（`DEBUG_CUSTOMOPT`，arm64 上 `DEBUG_FULLOPT` 是 -Os）
  每帧耗时减半，CoreMark 2233 → 10847
- [x] “驱动关中断或持锁”在 33ms 下用 critmon 排除；16ms 下 NSH 卡死在 -O2 下
  没有复现；改成 16ms 后 5 个场景都是 58 fps
- [ ] LVGL 多渲染单元（`LV_USE_OS` + `LV_DRAW_SW_DRAW_UNIT_CNT`）
- [ ] NEON（`LV_USE_DRAW_SW_ASM`）
- [ ] 驱动只拷脏区域的列：现在 flush 占每帧的 70–80%。改动在
  `arch/arm64/src/s6`，要走 nuttx fork 的 PR，等确认
- 每一项都用 `echo bench run > /dev/launcher` 记录前后的 `bench:` 输出
