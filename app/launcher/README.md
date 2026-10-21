# launcher（LVGL 桌面）

队伍 349 FedoraVForce 的 LVGL 触控桌面，映射到 openvela
`packages/demos/contest2026_349_launcher`，开机由 `rcS` 自动启动。

## 页面

- **锁屏**：时分竖排大数字，逐位滚动；拖动底部橙色块过线或点按解锁，屏幕任意处上滑也可
- **桌面**：运行时钟、剩余堆、帧率、运行时长；PAINT / ABOUT / BENCH 三个入口
- **画板 · 手写数字识别**：写一个 0-9 的数字，停笔 0.7 秒后在板上本地识别，
  显示结果、置信度、前三名概率和模型实际看到的 28x28 输入
- **About**：移植信息与实时帧率、堆占用
- **Bench**：5 个 LVGL 渲染场景（矩形、圆角描边、文字、圆弧、整屏滑动），
  按真实显示刷新计帧并统计每帧渲染耗时；随后在后台线程跑 CoreMark（单核）

## 文件

| 文件 | 内容 |
| ---- | ---- |
| `launcher_main.c` | 锁屏 / 桌面 / About / 画板、动画、主循环 |
| `launcher_bench.c` | 跑分页 |
| `launcher_digits.c` | 手写识别：笔画按 MNIST 线宽重画到 28x28，784-128-10 MLP 推理 |
| `launcher_model.c` | 模型权重（int8，MNIST 测试集 97.7%），由训练脚本生成 |
| `fonts/` | Oswald / Outfit 子集字库（lv_font_conv 生成，SIL OFL 1.1，许可证见同目录） |

## 说明

- 识别在 CPU 上做一次约 10 万次乘加，不依赖 NPU
- 帧率上限由 LVGL 刷新周期决定（默认 33 ms，即 30 FPS），跑分页同时给出每帧耗时
- CoreMark 需要 `CONFIG_BENCHMARK_COREMARK` 和 `CONFIG_PIPES`（板级 defconfig 已开启）；
  未编入时页面显示 N/A。CoreMark 运行约 15 秒，期间界面静止、优先级低于界面，
  因为它按墙钟计时，界面占用 CPU 会拉低分数
- 注意：开启 CoreMark 后，构建时会从 github.com/eembc/coremark 下载源码，需要联网
- Montserrat 24 / 32 / 40 / 48（图标与回退字体）由本应用 Kconfig 自动选中

## 授权与致谢

- `fonts/` 下的字库由 Oswald（Copyright 2016 The Oswald Project Authors）和
  Outfit（Copyright 2021 The Outfit Project Authors）转换而来，**沿用 SIL Open
  Font License 1.1**（不随本仓其余代码的 Apache-2.0），许可证全文见同目录
- 识别模型用 MNIST 数据集训练（Yann LeCun、Corinna Cortes、Christopher J.C.
  Burges），训练与生成脚本见 `tools/`

运行：NSH 下执行 `launcher`。
