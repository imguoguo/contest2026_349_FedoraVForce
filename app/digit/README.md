# digit（手写数字识别公共库）

paint 用它识别画板上的数字，bench 用它比较两种推理实现的耗时和准确率。由
用到它的应用 `select LVX_DIGIT`，自己不单独出现在 menuconfig 里。

- `digit_preprocess()`：笔画坐标 → 28×28 灰度图
- `digit_tflm_classify()`：TFLite Micro 推理（`CONFIG_TFLITEMICRO`），全 int8
  模型，`digit_tflm.cc`
- `digit_classify()`：手写的 C 推理，int8 权重、每层一个缩放系数、浮点计算，
  留作对比，`digit.c`
- `g_digit_sample_images` / `g_digit_sample_labels`：MNIST 测试集里每隔 20 张
  取一张，共 500 张，bench 在板上用它们比较两种实现

## 模型

784→128→10 两层 MLP（ReLU），一次约 10 万次乘加，在 CPU 上跑。两份模型用同样
的数据和增强（±2px 平移、加粗）训练：

| 实现 | 模型 | 大小 | MNIST 测试集准确率 |
| --- | --- | --- | --- |
| TFLite Micro | 全 int8 `.tflite`（输入输出也是 int8） | 107496 字节 | 97.81% |
| 手写 C | int8 权重 + 每层缩放，numpy 训练 | 约 100KB | 97.91% |

TFLite Micro 只用到 `FULLY_CONNECTED` 和 `SOFTMAX` 两个算子，16KB 的 arena。
解释器放在静态存储里，第一次用时建好；flat build 里应用的静态变量在两次运行
之间保留，所以每次开机只建一次，不会泄漏。

16px 宽的屏幕笔画直接缩到 28×28 会细到不足 0.5px，模型认不出来。所以画的时候
记录笔画坐标，识别时按包围盒长边等比缩放到约 20×20，按 MNIST 的线宽（实心
半径 1.2px，到 1.7px 渐隐）重新画一遍，再按灰度重心移到中心（`digit.c`）。

## 训练

从 MNIST 的任一镜像下载 4 个文件，例如 https://github.com/fgnt/mnist ：

| 文件 | MD5 |
| --- | --- |
| `train-images-idx3-ubyte.gz` | `f68b3c2dcbeaaa9fbdd348bbdeb94873` |
| `train-labels-idx1-ubyte.gz` | `d53e105ee54ea40749a09fcbcd1e9432` |
| `t10k-images-idx3-ubyte.gz` | `9fb629c4189551a2d022fa330f9573f3` |
| `t10k-labels-idx1-ubyte.gz` | `ec29112dd5afa0611ce80d1b7f02629c` |

手写 C 版本只需要 numpy：

```bash
python3 tools/train.py <mnist 目录> -o mnist_mlp.npz   # 生成 digit_model.c 的输入
python3 tools/gen_model.py mnist_mlp.npz
python3 tools/gen_samples.py <mnist 目录>              # digit_samples.c
cc -O2 -I. -o digit_test tools/digit_test.c digit.c digit_model.c -lm
./digit_test <mnist 目录>
```

`digit_test` 先用 C 代码跑一遍 MNIST 测试集（应和 train.py 的结果一致），再把
模拟的触摸笔画 0–9 各 50 个走一遍预处理和识别。

TFLite Micro 版本要 TensorFlow，我们在编译机的容器里用虚拟环境跑：

```bash
python3 tools/train_tflm.py <mnist 目录> -o mnist_mlp.tflite
python3 tools/gen_tflm.py mnist_mlp.tflite              # digit_tflm_model.c
tools/tflm_test.sh <openvela apps 目录> <mnist 目录>    # 主机上用 TFLM 跑 digit_test
```
