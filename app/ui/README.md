# ui（桌面公共库）

launcher、paint、bench、monitor 共用的外观，不是独立应用。由用到它的应用
`select LVX_UI`，只编译一份。

- `lvx_ui.h`：配色（`docs/launcher-v2` 的 BG、PAPER、INK……）、布局常量、字体
- 控件：色块 `ui_box`、标签 `ui_label`、加字间距的大写小标签 `ui_tag`、
  返回按钮 `ui_back`
- 动效：逐位纵向滚动的数字 `ui_num` / `ui_num_set`（320ms，ease-out）、
  从 0 数到当前值 `ui_num_count`、滑入 `ui_enter`
- `ui_place`：按文字的可见上沿定位，和字体在字形上方留多少空白无关

应用的 Makefile 和 CMakeLists.txt 里要加上本目录的头文件路径（manifest 把它
链接到 `packages/demos/contest2026_349_ui`）。

## 字库

Oswald（数字、标题）和 Outfit（正文）都是 SIL OFL 1.1，许可证全文在
`fonts/OFL-*.txt`。字库由 `tools/gen_fonts.sh` 用 lv_font_conv 1.5.3 从
[google/fonts](https://github.com/google/fonts) 的可变字体生成（先用
fontTools 切出静态字重），4 bpp、不压缩，只转用得到的字符：

| 字库 | 字重 | 字符 |
| --- | --- | --- |
| `ui_font_oswald_300` | 600 | `0-9:`（锁屏和桌面时钟、识别结果） |
| `ui_font_oswald_120` | 500 | 数字、大写字母、`/%:.-+` |
| `ui_font_oswald_64` | 500 | 数字、大小写字母、`/%:.-+` |
| `ui_font_outfit_26` / `_34` | 400 | ASCII |
| `ui_font_outfit_52` | 600 | ASCII |

字重是按参考图量出来的。单个字形高度不能超过 255px，否则会触发 LVGL 的
large 格式，300px 的 Oswald 数字是 252px。

```bash
pip install fonttools
app/ui/tools/gen_fonts.sh 'Oswald[wght].ttf' 'Outfit[wght].ttf'
```

图标用 LVGL 自带的 Montserrat 符号（`UI_ICON_24`、`UI_ICON_48`）。
