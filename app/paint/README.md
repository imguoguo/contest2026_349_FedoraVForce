# paint（画板与手写数字识别）

全屏画板，从桌面启动。底部工具栏：返回、四种颜色、橡皮（纸色）、笔刷粗细
（默认 16px）、清空、识别开关（眼睛）。

打开识别后，抬笔 0.7 秒没有新笔画就识别一次画布上的所有笔画（从上次清空
开始），顶部滑入结果卡片：结果、置信度、前三名和模型实际看到的 28×28 输入，
点卡片收起。写下一个数字前先清空。每次识别也会把 28×28 输入和结果打到串口。

## 模型

784→128→10 两层 MLP（ReLU），int8 权重，每层一个缩放系数，推理时用浮点计算，
一次约 10 万次乘加，在 CPU 上跑。MNIST 测试集准确率 97.91%（量化后）。

16px 宽的屏幕笔画直接缩到 28×28 会细到不足 0.5px，模型认不出来。所以画的时候
记录笔画坐标，识别时按包围盒长边等比缩放到约 20×20，按 MNIST 的线宽（实心
半径 1.2px，到 1.7px 渐隐）重新画一遍，再按灰度重心移到中心（`digit.c`）。

## 训练

只需要 numpy。从 MNIST 的任一镜像下载 4 个文件，例如
https://github.com/fgnt/mnist ：

| 文件 | MD5 |
| --- | --- |
| `train-images-idx3-ubyte.gz` | `f68b3c2dcbeaaa9fbdd348bbdeb94873` |
| `train-labels-idx1-ubyte.gz` | `d53e105ee54ea40749a09fcbcd1e9432` |
| `t10k-images-idx3-ubyte.gz` | `9fb629c4189551a2d022fa330f9573f3` |
| `t10k-labels-idx1-ubyte.gz` | `ec29112dd5afa0611ce80d1b7f02629c` |

```bash
python3 tools/train.py <mnist 目录> -o mnist_mlp.npz   # 带 ±2px 平移和加粗增强
python3 tools/gen_model.py mnist_mlp.npz              # 生成 digit_model.c
cc -O2 -I. -o digit_test tools/digit_test.c digit.c digit_model.c -lz -lm
./digit_test <mnist 目录>
```

`digit_test` 先用 C 代码跑一遍 MNIST 测试集（应和 train.py 的结果一致），再把
模拟的触摸笔画 0–9 各 50 个走一遍预处理和识别。
