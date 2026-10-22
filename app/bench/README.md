# bench（跑分）

从桌面启动，点 RUN BENCHMARK：

1. 5 个渲染场景各跑 3 秒：大量纯色矩形、圆角加描边、大段文字、圆弧、整屏
   滑动。每个场景是一个单独的 screen，下面不会再画别的东西
2. 每行显示每帧耗时（ms / frame）、条形图和 FPS。FPS 按真实刷新计数，会被
   刷新周期（`LV_DEF_REFR_PERIOD`）卡住；每帧耗时用性能计数器测，反映真实
   的渲染开销，两个都要看
3. 然后在 CPU3 上以低于界面的优先级跑一次 CoreMark，约 15 秒，界面保持静止

每个场景的结果也会打到串口，包括其中 flush（驱动拷贝）花的时间，方便对比
优化前后：

```text
bench: Rectangles          30 fps    4.1 ms/frame  (flush 1.2 ms)
bench: CoreMark 1234 iterations/s
```

CoreMark 需要 `CONFIG_BENCHMARK_COREMARK` 和 `CONFIG_PIPES`，没有编进去时显示
N/A。打开 CoreMark 后编译时会从 GitHub 下载源码。

下面两个按钮：

- CPU：原来的 CPU / 内存测试（整数、浮点矩阵乘、4 MB 拷贝和填充，每项约
  0.5 秒），然后比较 paint 手写识别的两种实现：TFLite Micro 和手写的 C 推理，
  在同一批 500 张 MNIST 测试图（`app/digit`）上的单次推理耗时和准确率。只计
  推理本身，第一次调用建 TFLM 解释器的时间不算
- 纯色、渐变、网格测试图（点屏幕切换下一张）

CPU 页的结果也打到串口：

```text
bench: digit TFLite Micro  <us> us/inference  <认对张数> / 500 right
```

`bench run`：启动后直接开始跑，渲染场景和 CoreMark 之后接着跑 CPU 页的测试，
最后打出 `bench: done` 并退出。配合 launcher 的 `/dev/launcher`，可以只用串口
跑完全部项目：`echo bench run > /dev/launcher`。
