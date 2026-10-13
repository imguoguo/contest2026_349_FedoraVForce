# AGENTS.md — VSAI LogicPi A1 NuttX/openvela 适配

## 项目背景

- 2026 openvela AI 硬件开发者大赛 · **新硬件适配赛道**（队伍 349 FedoraVForce）。
- 目标：为 **VSAI LogicPi A1**（Amlogic Meson S6，ARM64 Cortex-A，GICv3）适配 NuttX/openvela，跑通最小 NSH 基线并逐步增加外设。
- 本仓为参赛专属仓：全部参赛代码、产物、AI Coding 日志（`logs/`）提交到这里，通过 PR 自行 review 合入（首次需在官网签署 CLA）。

## 工作区布局

- openvela 全量源码在本仓**上一级目录**（repo + manifest 管理，分支 `dev-ai-contest-2026`）：`nuttx/` `apps/` `vendor/` `prebuilts/` 等。
- 本仓 `board/contest_board/` 经 manifest `<linkfile>` 软链到 `vendor/openvela/boards/contest2026_349_board`——**板级适配代码只写在这里**。
- **不要改动工作区内 nuttx/apps/vendor 等公共仓**。芯片级改动（如 `arch/arm64/src/s6`）走 fork open-vela/nuttx → PR 到 `dev-ai-contest-2026` 分支。
- 注意：`board/contest_board/` 目前仍是组委会的 `CONTEST2026_000_*` 占位骨架（Kconfig/CMakeLists/README），接入时需重命名为 349 对应名称。

## 现有移植基础（私有 fork，参考实现）

- 仓库：`git@github.com:U2FsdGVkX1/nuttx.git`，分支 `releases/13.0`（基于上游 NuttX 13.0）。
- 顶端 5 个提交为 s6 移植：最小启动（`s6_boot.c`）、UART（`s6_serial.c`）、MIPI DSI + framebuffer（1080x1920）及简化整理。
- 接入计划：将 s6 芯片支持**重写**到 `dev-ai-contest-2026`（PR 到官方 nuttx），板级代码整理进本仓 `board/contest_board/`。
- 关键硬件参数：
  - DRAM/NSH 起始 `0x03000000`（defconfig 中 RAM 16MB）
  - U-Boot `booti` 加载地址 `0x03080000`（`dramboot.ld`）
  - UART0 控制台 921600 波特
  - GICv3：GICD `0xff200000`，GICR `0xff240000`

## 构建与部署

- fork 树参考验证（standalone，需同级 nuttx-apps 与 `aarch64-none-elf-gcc`）：

  ```bash
  ./tools/configure.sh vsai-logicpi-a1:nsh && make -j
  ```

- 参赛构建（在 openvela 工作区根目录，即本仓上一级；工具链用 `prebuilts/gcc/linux-x86_64/aarch64-none-elf`）：

  ```bash
  ./build.sh vendor/openvela/boards/contest2026_349_board/configs/nsh [menuconfig|distclean] [-j]
  ```

- 部署：板上 U-Boot 从 SD/eMMC 读取 `nuttx.bin` 到 `0x03080000` 后 `booti`。

## 协作约定

- 缺少任何信息（硬件细节、方案取舍、参数）时**交互式提问，不要猜测**。
- 代码保持**足够简单**：避免不必要的变量、函数与抽象。
- 遵循 NuttX/openvela 编码规范：`.clang-format` 格式化、nxstyle 风格；git 提交带 `Signed-off-by:`，commit message 用 nuttx 惯例（`子系统: 简述`）。
- AI Coding 日志需主动导出到 `logs/`（勿忽略该目录），提交截止 9 月 20 日。
