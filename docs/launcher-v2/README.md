# LogicPi A1 桌面 v2 设计说明

这份文档说明我们想把板子上的 LVGL 桌面改成什么样。参考图在 `images/` 里，是按设计稿在电脑上用同版本 LVGL 渲染出来的 1080×1920 截图。图上的数字（FPS、剩余内存、运行时间、识别概率这些）都只是占位，实际以板上数据为准。

## 一、现在是什么样

比赛仓 `dev-ai-contest-2026`（`b1e7413`）里，桌面由三个应用组成：

- `app/launcher`：磁贴风格的桌面，开机由 `rcS` 自动启动。它读 `CONFIG_LVX_LAUNCHER_APPS`，给每个已编入的内置应用生成一个磁贴。点开应用时，launcher 先释放 LVGL，启动应用后 `waitpid` 等它退出，再重新初始化 LVGL 回到桌面。还有锁屏，以及一个轮播运行时间和剩余内存的大磁贴。
- `app/paint`：全屏画板，可以选颜色、调笔刷粗细、清空。
- `app/bench`：CPU 和内存测试（整数、浮点矩阵乘、4 MB 拷贝和填充）、全屏刷新帧率、纯色/渐变/网格测试图。

整体是深蓝色调，字体只有 Montserrat。

## 二、这次要做什么

**保留"launcher + 独立应用"的架构**，在这个基础上换一套视觉风格，并且加功能：

1. launcher：新的锁屏、仪表盘式桌面、About 页，桌面磁贴仍然按应用列表自动生成。
2. paint：换新风格，加**板上本地的手写数字识别**。
3. bench：换新风格，加**LVGL 渲染场景跑分**和 **CoreMark**，原来的 CPU/内存测试和测试图保留。
4. 新增 monitor 应用：显示 4 个核的实时负载、任务列表、内存。
5. 帧率：目前被刷新周期卡在 30 FPS，要能配置，并且查清楚到 60 FPS 的瓶颈在哪。

驱动和板级代码不在这次范围内，除非是为了帧率做的必要改动（要单独提交）。

## 三、视觉规范

整体是"编辑感"风格：浅底、黑字、大号窄体数字，用色块来区分功能，留白要多。

### 颜色

| 名称 | 色值 | 用途 |
| --- | --- | --- |
| BG | `#e9ecef` | 浅色页背景 |
| PAPER | `#f8f9fa` | 白色卡片、画板底色 |
| RULE | `#c8cdd3` | 分隔线 |
| INK | `#101317` | 主文字、黑色卡片、粗分隔线 |
| INK_2 | `#262b33` | 深色页上的按钮底色 |
| DIM | `#5a6470` | 次要文字 |
| MUTED | `#9aa3ad` | 小号大写标签 |
| ORANGE | `#ff5b1f` | 强调色：秒数、解锁块、主按钮、识别结果 |
| BLUE | `#2d5bff` | 帧率相关卡片 |
| BLUE_DIM | `#5a7bff` | 蓝卡片上的进度条底色 |
| YELLOW | `#ffd43b` | 内存卡片 |

bench 和 monitor 是深色页，背景用 INK，按钮和条形图底色用 INK_2。

### 字体

- **Oswald**（窄体无衬线）：大数字和标题。需要 120px、64px 两档；锁屏时钟数字单独一档 300px（只需要 `0-9:`）。
- **Outfit**（几何无衬线）：正文。需要 26px、34px、52px 三档。
- 图标沿用 LVGL 自带的 Montserrat 符号字体。
- 两种字体都是 SIL OFL 1.1，用 `lv_font_conv` 转成 C 字库，只转用得到的字符子集；许可证全文放在字库同目录。**单个字形高度超过 255px 会触发 LVGL 的 large 格式**，所以时钟字号不能再大。
- 小标签全部大写，加字间距（参考图里的 `UPTIME`、`HEAP FREE`、`PORT DETAILS`）。

### 布局

- 屏幕 1080×1920 竖屏，左右边距 72px，卡片间距 24px，圆角 12px。
- 页面顶部一行：左边队名 `FEDORA-V FORCE / 349`，右边是 `OPENVELA` 或实时 FPS。
- 二级页面左上角是一个黑色方形返回按钮（深色页里用 INK_2）。

### 动效

- 数字变化时逐位纵向滚动，约 320ms，ease-out。
- 进入页面时卡片依次滑入，数值从 0 数到当前值，时长 250–400ms。
- 解锁块上的箭头上下浮动 40px，单程 800ms，往复循环。
- 动效不能影响触摸响应。帧率不够时宁可去掉动效。

## 四、各页面

### 4.1 锁屏（launcher）

参考：`images/1-lock.png`

- 板子没有 RTC，**时钟显示的是开机以来的运行时间**：时、分两行竖排，用 300px 的大数字；秒数用橙色 Oswald 放在右侧，上面标 `UPTIME`。
- 中间一条粗黑线，下面左边写 `openvela on LogicPi A1`，右边三行小字 `NuttX` / `Amlogic S6` / `4x Cortex-A510`。
- 底部约 1/3 屏是橙色解锁块：大字 `SWIPE UP`，下面一行 `or tap here to unlock`，右侧是浮动的上箭头。
- 解锁方式：把解锁块往上拖过约 240px 再松手；或者直接点解锁块；或者在屏幕任意位置上滑。拖不够距离松手时，解锁块在 260ms 内弹回原位。
- 桌面上要有办法重新锁屏（现在的 Lock 磁贴保留）。

### 4.2 桌面（launcher）

参考：`images/2-home.png`、`images/2a-home-entering.png`（入场动画的中间帧）

从上到下：

1. 顶部一行：队名，右边实时 `NN FPS`。
2. 黑色大卡片 `UPTIME CLOCK`，右上角 `NuttX / openvela`；中间是 `HH:MM` 大数字，秒数用橙色上标。
3. 两张半宽卡片：黄色 `HEAP FREE`（剩余堆，单位 M）、蓝色 `FRAMES / S`（实时帧率）。
4. 一张白色通栏 `UPTIME`，显示 `5h 28m` 这种格式。
5. 应用磁贴区，按 `CONFIG_LVX_LAUNCHER_APPS` 生成。参考图里 PAINT 是左边的橙色竖长磁贴，ABOUT 和 BENCH 在右边上下排。

应用磁贴要显示**图标、名字（Oswald 大写）、一行说明**，右上角有一个 `>` 箭头，可以带一个小标签（比如 PAINT 上的 `AI DIGITS`）。这些信息现在 launcher 里拿不到，需要设计一个办法让每个应用提供自己的颜色、图标、说明和标签。比如在 launcher 里维护一张按应用名查的表，查不到的应用用默认样式。应用数量变化时，磁贴排布要能自适应：1 到 5 个应用都不能排乱，超出的部分可以滚动。

About 是 launcher 内部的页面，不是独立应用，但在桌面上和应用磁贴并排显示。

帧率的定义：每秒统计 LVGL 实际刷新（refresh）完成的次数，不是定时器触发的次数。

### 4.3 About（launcher）

参考：`images/3-about.png`、`images/3a-about-entering.png`

- 返回按钮，右上角 `PORT DETAILS`，大标题 `ABOUT`，副标题 `VSAI LogicPi A1`，下面一行小字 `openvela / NuttX, ported by team 349`。
- 粗黑线（入场时从左往右展开），然后是一张键值表：SOC `Amlogic S6 (ARM64)`、INTERRUPT `GICv3`、DISPLAY `MIPI DSI 1080x1920`、FORMAT `RGB565`、TOUCH `GT9XX over I2C`。建议再加一行 CPU `4x Cortex-A510, SMP`。
- 蓝色卡片：`LIVE FRAMES / S` 和大号帧率数字，下面写 `Heap 23552KB free of 32768KB` 再加一条进度条。
- 底部黑色通栏按钮 `< BACK`。

### 4.4 画板与手写数字识别（paint）

参考：`images/4a-paint-empty.png`、`images/4-paint.png`、`images/4b-paint-result-moving.png`（结果卡片滑入的中间帧）

**画板**

- 全屏 PAPER 底色画布，底部是一条深色圆角工具栏：返回、四个颜色色块（黑/橙/蓝/黄，选中的加白框）、清空、识别开关（眼睛图标）。
- 笔画宽度约 16px，要连续，快速划动时不能断成点。现有 paint 的笔刷粗细切换，以及"选白色当橡皮"的用法保留。

**识别**

- 打开识别开关后，抬笔 0.7 秒没有新笔画就自动识别一次。
- 顶部滑入一张深色结果卡片，标题是 `ON-DEVICE AI / RESULT`：左边是橙色大号识别结果，旁边是置信度；中间列出前三名和各自的概率；右边显示**模型实际看到的 28×28 输入**（放大显示），底下一行 `tap to dismiss`。点卡片收起。
- 全部在 CPU 上计算，不用 NPU，一次推理大约 10 万次乘加。

**模型和预处理**（这部分最容易做错，单独写清楚）

- 模型是 784→128→10 的两层 MLP（ReLU），在 MNIST 上训练。训练时加随机平移（±2px）和随机加粗做数据增强。权重量化成 int8，每层一个缩放系数，推理时用浮点计算。MNIST 测试集上的准确率目标 ≥97%。
- 训练脚本和"权重→C 源文件"的生成脚本放进 paint 应用自己的 `tools/` 目录（`app/paint/tools/`），用纯 numpy 就能跑。MNIST 数据不进仓库，在说明里写清楚从哪里下载。
- **预处理不能直接把屏幕像素缩小到 28×28**：16px 宽的笔画缩小后只剩不到 0.5px，模型从来没见过这么细的笔画，识别会全错（我们试过，0 会被认成 2，1 会被认成 5）。正确做法是：
  1. 画的时候记录笔画的坐标点序列。
  2. 取所有点的包围盒，按长边等比缩放，放进 28×28 网格中间约 20×20 的区域（和 MNIST 一致）。
  3. 按 MNIST 的线宽重新画一遍笔画（28 网格里笔画半径约 1.7px，这里已经包含抗锯齿的过渡边缘，实心部分线宽约 2.4px），得到 0–1 的灰度图。
  4. 按灰度重心把图平移到网格中心。
- 验收：手写 0–9 每个数字写 3 遍以上，大部分要认对；把预处理后的 28×28 图显示出来，看起来要和 MNIST 样本差不多。

### 4.5 跑分（bench）

参考：`images/5a-bench-idle.png`（运行前）、`images/5b-bench-scene1.png`（场景运行中）、`images/5c-bench-scene3.png`、`images/5-bench.png`（结果）

- 深色页。左上返回按钮和大标题 `BENCH`。
- 左边一个圆环，中间是平均 FPS（`AVG FPS`）；右边是 `COREMARK` 分数，单位 `iterations / s, 1 core`。下面一行小字说明当前的帧率上限（比如"Frame rate capped at 30 by the 33 ms refresh period"，数值按实际配置显示）。
- 5 个渲染场景，每个一行，显示名称、每帧渲染耗时（ms / frame）、条形图、FPS：
  1. Rectangles：大量纯色矩形
  2. Rounded + border：圆角加描边
  3. Text：大段文字
  4. Arcs：圆弧
  5. Full-screen slide：整屏滑动
- 每个场景按真实显示刷新计帧，同时统计每帧的渲染耗时。**FPS 会被刷新周期卡住，每帧耗时才反映真实的渲染开销**，两个都要显示。
- 场景跑完后跑 CoreMark：放在后台线程里单核运行，优先级低于界面，约 15 秒，期间界面保持静止。CoreMark 按墙钟计时，界面占 CPU 会拉低分数。CoreMark 需要 `CONFIG_BENCHMARK_COREMARK` 和 `CONFIG_PIPES`，没编进去时显示 `N/A` 并提示缺哪个配置。注意：开启 CoreMark 后，编译时会从 GitHub 下载源码。
- 底部橙色大按钮 `RUN BENCHMARK`，跑完变成 `RUN AGAIN`。
- 原来的 CPU/内存测试和测试图保留，作为同页的第二组或者另一个分页，风格统一成新的样子。

### 4.6 系统监视（新应用 monitor）

没有参考图，按 bench 的深色风格做：

- 顶部 4 条横条，分别是 CPU0–CPU3 的实时负载，每秒刷新一次。NuttX 的 `/proc/cpuload` 只有总负载，没有单核数据。每个核都有自己的 IDLE 任务（`ps` 里的 `CPU0 IDLE` … `CPU3 IDLE`），可以用"100% 减去该核 IDLE 任务的占用"来算单核负载。需要开启 CPU 负载统计（`CONFIG_SCHED_CPULOAD_SYSCLK` 之类）。
- 内存：总量、剩余、最大空闲块。
- 任务列表：PID、名称、优先级、所在 CPU、状态、栈使用。按 CPU 占用排序，可以上下滚动。
- 作用是在答辩现场直观地展示 SMP 4 个核都在工作，也方便调试帧率。
- 加到 `CONFIG_LVX_LAUNCHER_APPS` 里，从桌面启动。

## 五、帧率

现在的情况：

- `CONFIG_LV_DEF_REFR_PERIOD=33` 和 `CONFIG_LV_NUTTX_VSYNC_TIMER_PERIOD=33`，所以上限就是 30 FPS。
- 编译开着 `CONFIG_DEBUG_NOOPT`（-O0）；LVGL 只用一个渲染单元（`LV_USE_OS=0`）；没开 NEON 汇编。
- 显示驱动是"影子缓冲 + 扫描缓冲"：每次刷新把脏区域所在的整行拷到扫描缓冲，再清 dcache。全屏一帧要拷 4 MB。
- 我们试过把两个周期直接改成 16ms，launcher 能起来，但 **NSH 随后不响应串口输入了**，原因还没查。

要求：

1. 刷新周期改成可配置，默认保持 33ms 不变，先保证稳定。
2. 用 bench 的每帧耗时找出瓶颈，按影响大小逐项试：关 NOOPT、多核渲染、驱动只拷脏区域的列、NEON。每改一项都要记录前后的数据。
3. 查清楚 16ms 下 NSH 卡死的原因并修掉，然后再提高刷新率。
4. 任何情况下 NSH 都必须能正常输入。这是调试的生命线。

## 六、构建和配置

- 三个应用都要同时支持 Make 和 CMake（`Makefile`、`Make.defs`、`CMakeLists.txt`、`Kconfig`），和现有应用保持一致。
- 字库、模型这些生成出来的 C 文件和用到它的应用放在一起，生成脚本放进该应用的 `tools/` 子目录。几个应用共用的字库和配色，要想办法只放一份（比如放进 launcher，或者单独做一个公共目录），不要每个应用复制一遍。
- 板级 `configs/nsh/defconfig` 的改动单独提交，`LVX_LAUNCHER_APPS` 加上 monitor。
- 代码风格遵循 NuttX 的 nxstyle，提交前检查一遍。
- 每个应用的 README 更新成新的功能说明。

## 七、验收清单

- [ ] 开机自动进锁屏，三种解锁方式都能用，拖不够时会弹回
- [ ] 桌面的数字每秒更新，FPS 是真实刷新次数
- [ ] 桌面磁贴按应用列表生成，增减应用后排布不乱
- [ ] 每个应用都能打开、退出、再打开，反复 10 次不崩溃，内存不持续下降
- [ ] 手写 0–9 大部分能认对，结果卡片显示完整
- [ ] bench 能跑完 5 个场景并给出 CoreMark 分数
- [ ] monitor 能看到 4 个核的负载
- [ ] 全程 NSH 都能正常输入
- [ ] 记录各项优化前后的 FPS 和每帧耗时

## 八、参考图清单

| 文件 | 内容 |
| --- | --- |
| `1-lock.png` | 锁屏 |
| `2-home.png` / `2a-home-entering.png` | 桌面 / 入场动画中间帧 |
| `3-about.png` / `3a-about-entering.png` | About / 入场动画中间帧 |
| `4a-paint-empty.png` / `4-paint.png` / `4b-paint-result-moving.png` | 空画板 / 写了数字 / 结果卡片滑入中 |
| `5a-bench-idle.png` / `5b-bench-scene1.png` / `5c-bench-scene3.png` / `5-bench.png` | 跑分：运行前 / 场景 1 / 场景 3 / 结果 |
