# preview（桌面页面渲染预览）

板上没法截图，这个工具在电脑上用和板上同一份 LVGL 把页面渲染成 PNG，和
`docs/launcher-v2/images` 的参考图对照。

- LVGL 源码用 open-vela/apps_graphics_lvgl 的 `dev-ai-contest-2026` 分支（和
  编译机上的 `apps/graphics/lvgl/lvgl` 逐文件一致），配置是板子的
  `CONFIG_LV_*`（`lv_kconfig.h`），显示也是 RGB565、direct 模式
- 每个应用一个场景 `scene_*.c`：直接 include 应用源码，用虚拟时间推进、模拟
  触摸，在指定时刻截图。NuttX 相关的部分由 `preview.h`、`preview.c` 和
  `stub/` 代替。运行时间固定从 05:28:27 开始，堆和参考图一样
- 每帧耗时在预览里是 0，帧率和跑分以板上为准

```bash
make LVGL_DIR=<apps_graphics_lvgl> run
python3 compare.py      # out/compare/：参考图 | 预览 | 差异，并打印平均差值
```

除了参考图同名的截图，还有锁屏回弹（`1b`）、重新锁屏（`1c`）、跑分第二页
（`5d`）、monitor（`6`），以及 0–5 个应用时的桌面排布（`layout-*`）。
launcher 场景最后会退出再启动 10 次并打印 LVGL 堆的剩余，检查有没有泄漏。
