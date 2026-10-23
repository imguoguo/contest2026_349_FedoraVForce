# LogicPi A1 以太网支持（NuttX）

目标：让板子在 NuttX 下能用有线网络，NSH 里能 `ifconfig`、`ping`，能跑 telnetd 和 iperf，最好还能通过网络传文件，省掉每次 TFTP 重启。

参考资料是厂商的 Linux 内核源码 <https://github.com/U2FsdGVkX1/AMLogic-A311Y2_kernel>，编译机上已经克隆了一份（浅克隆）：

- 编译机：`~/contest2026/ref/AMLogic-A311Y2_kernel`
- 容器里：`/work/ref/AMLogic-A311Y2_kernel`

Amlogic 自己改过的代码大多在 `common_drivers/` 下面，跟主线同名的文件**以 `common_drivers/` 里的为准**。

---

## 一、硬件情况

| 项目 | 内容 | 出处 |
| --- | --- | --- |
| MAC | Synopsys DWMAC **4.00**（Ethernet QoS），`compatible = "amlogic,meson-axg-dwmac", "snps,dwmac-4.00"` | `common_drivers/arch/arm64/boot/dts/amlogic/meson-s6.dtsi` 的 `ethmac` |
| MAC 寄存器 | `0xfe368000`，长度 `0x12ec` | 同上 |
| Amlogic 包装寄存器（PRG_ETH） | `0xfe364000`，两个 32 位寄存器：PRG_ETH0 / PRG_ETH1 | 同上 `reg` 的第二段 |
| 中断 | GIC SPI 74，高电平。NuttX 里的中断号是 **106**（74 + 32，跟串口 SPI 169 → 201 一个算法） | 同上 |
| PHY | 外置 **RTL8211F**（PHY ID `001c.c916`），**RGMII**，地址 0 | `s6_s905d5_bq201_linux.dts` 的 `external_phy` |
| PHY 复位脚 | GPIOZ_15，低有效，开漏；拉低 20 ms，再等 50 ms | `meson-s6.dtsi` 的 `external_phy` |
| MDIO 选择器 | `0xfe360000`（`amlogic,g12a-mdio-mux`），用外置 PHY 时 `ETH_PHY_CNTL2`（偏移 0x88）写 0 | `common_drivers/drivers/net/mdio/mdio-mux-meson-g12a.c` 的 `g12a_enable_external_mdio()` |
| 板级参数 | `phy-mode = "rgmii"`，`amlogic,tx-delay-ns = <2>`，`internal_phy = <2>`（外置 PHY），`cali_val = <0x290000>` | `s6_s905d5_bq201_linux.dts` 的 `&ethmac` |
| 引脚 | `eth_pins` + `eth_rgmii_pins`（MDIO/MDC、RGMII 收发共 14 根），驱动能力 3 mA | `meson-s6.dtsi` |

板子是 bq201 这一型（U-Boot 提示符 `s6_bq201#`），对应 `s6_s905d5_bq201*.dts`。

### 时钟

时钟控制器基址是 `0xfe000000`（跟现在 I2C 驱动里用的 `S6_CLK_SYS_CLK_EN0_REG1` 是同一个基址），出处 `common_drivers/drivers/clk/meson/s6.c` / `s6.h`：

| 寄存器 | 地址 | 用到的位 |
| --- | --- | --- |
| SYS_CLK_EN0_REG0 | `0xfe000044` | bit 4 `sys_eth_phy`，bit 8 `sys_eth_mac` |
| SYS_CLK_EN0_REG1 | `0xfe000048` | bit 3 `sys_eth` |
| ETH_CLK_CTRL | `0xfe000164` | bit 7 `eth_clk125m`（fclk_div2 ÷ 8 = 125 MHz），bit 8 RMII 时钟，bit 9–11 RMII 时钟源选择，bit 0–6 RMII 分频 |

### PRG_ETH 寄存器

出处 `common_drivers/drivers/net/ethernet/stmicro/stmmac/dwmac-meson8b.c`：

| 位 | 含义 |
| --- | --- |
| PRG_ETH0[2:0] | 接口模式。RGMII 写 1，RMII 写 4（AXG 及以后的芯片走 `meson_axg_set_phy_mode()`） |
| PRG_ETH0[4] | m250 时钟源选择 |
| PRG_ETH0[6:5] | TX 时钟延时，单位 2 ns；板子设 2 ns，所以这里是 1 |
| PRG_ETH0[9:7] | m250 分频，用来得到 125 MHz 的 RGMII TX 时钟 |
| PRG_ETH0[10] | RGMII TX 时钟使能 |
| PRG_ETH0[11] | RMII 时钟反相（RGMII 下要清 0） |
| PRG_ETH0[12] | TX 时钟和 PHY 参考时钟使能 |
| PRG_ETH1[19:16] | **RX 时钟延时**，每级 200 ps |

Amlogic 自己加的 `aml_custom_setting()` 在 `internal_phy == 2` 时直接把 `cali_val` 写进 PRG_ETH1。板子的 `cali_val = 0x290000`，也就是：

- bit[19:16] = 9 → RX 时钟延时 9 × 200 ps = **1.8 ns**
- bit 21 = 1（驱动里没有定义，照抄即可）

---

## 二、千兆为什么收不到

现象（之前两块板都测过）：U-Boot 里千兆时板子发得出去、收不回来，旧板偶尔错位，新板完全收不到；主机限到 100M 以后一切正常。

RGMII 在千兆时是 125 MHz 双沿采样，时钟和数据之间要差大约 2 ns，这个延时要么 PHY 加，要么 MAC 加。从 Linux 的配置看，这块板是**全部由 MAC 加**：

- `phy-mode = "rgmii"`：Linux 的 `rtl8211f_config_init()`（`common_drivers/drivers/net/phy/realtek.c`）会把 RTL8211F 自己的 TX/RX 延时都关掉（page 0xd08 的 reg 0x11 bit 8 和 reg 0x15 bit 3）
- TX 方向：PRG_ETH0 加 2 ns
- RX 方向：PRG_ETH1 加 1.8 ns（就是上面的 `cali_val`）

100M 时 RGMII 时钟只有 25 MHz，窗口宽很多，延时差一点也不影响，所以只有千兆出问题。**最可能的原因是 U-Boot 没有写 PRG_ETH1 这个 RX 延时**（或者 PHY 那边 RX 延时的开关跟 MAC 不配套）。发送方向正常，也说明 TX 延时是对的。

这个推测可以在 U-Boot 里直接验证，不用改代码：

```text
md.l 0xfe364000 2                  # 看 PRG_ETH0 / PRG_ETH1，PRG_ETH1 的 bit[19:16] 是不是 9
mii write 0 0x1f 0xd08; mii read 0 0x15; mii read 0 0x11; mii write 0 0x1f 0
                                   # 看 PHY 的 RX 延时（0x15 bit 3）和 TX 延时（0x11 bit 8）
mw.l 0xfe364004 0x290000           # 按 Linux 的值写 RX 延时
```

写完以后把主机网口改回千兆（`advertise 0x02f`），在 U-Boot 里 `ping` 主机、`tftpboot` 一次，对一下 crc32。

- 如果好了，NuttX 驱动里照 Linux 写 PRG_ETH1 就能跑千兆。
- 如果还不行，再试 PHY 打开 RX 延时、MAC 不加延时的组合（`mw.l 0xfe364004 0`，PHY page 0xd08 reg 0x15 置 bit 3），或者在 0–15 之间扫一遍 PRG_ETH1[19:16]。

验证结果记下来，写到第七节。

**注意**：这些都是临时改寄存器，不要 `saveenv`。

---

## 三、整体方案

**先利用 U-Boot 已经初始化好的状态，再一步步自己接管。** 跟现在显示驱动的做法一样（`S6_MIPI_DSI` 也是用 U-Boot 点亮的屏）。

U-Boot 刚用这个网口 TFTP 下载过内核，所以进 NuttX 时引脚复用、时钟、PHY 复位、MDIO 选择器、PRG_ETH0 都已经是好的。booti 前 U-Boot 只是停掉了 DMA。

第一阶段只写 DWMAC 本身的驱动，其余直接沿用；第二阶段再把时钟、引脚、PHY 复位这些补上，做到不依赖 U-Boot。

### 第一阶段：能 ping 通（100M）

1. **新建 `arch/arm64/src/s6/s6_eth.c`**，Kconfig 加 `S6_ETH`（依赖 `NET`），Make.defs/CMakeLists 加上。
   - 用 NuttX 新的 `netdev_lowerhalf` 接口写（`include/nuttx/net/netdev_lowerhalf.h`），参考 `drivers/net/igc.c` 的结构：`transmit` / `receive` / `ifup` / `ifdown`，收发完成在中断里 `netdev_lower_rxready()` / `netdev_lower_txdone()`。这个接口要求 `CONFIG_MM_IOB=y`。
   - DWMAC 4.x 的寄存器布局、描述符格式和 STM32H7 的以太网一样（MAC 在 0x0000、MTL 在 0x0C00、DMA 在 0x1000），可以对照 `arch/arm/src/stm32h7/stm32_ethernet.c` 和 `hardware/stm32_ethernet.h`。
   - Linux 那边对应的是 `common_drivers/drivers/net/ethernet/stmicro/stmmac/` 下的 `dwmac4_core.c`、`dwmac4_dma.c`、`dwmac4_descs.c` 和头文件 `dwmac4.h`、`dwmac4_dma.h`、`dwmac4_descs.h`。
2. **初始化顺序**：
   1. 读 MAC 地址：DMA 复位前先从 `MAC_Address0_High/Low`（0x300 / 0x304）读出 U-Boot 写进去的地址，读到全 0 或全 F 就用一个固定的本地管理地址（第一字节 `0x02` 开头）。
   2. DMA 软复位：`DMA_Mode`（0x1000）bit 0 置 1，等它自己清零。RGMII 下复位要靠 PHY 送过来的 RX 时钟，PHY 没起来时这一步会一直等，要加超时。
   3. 设 `DMA_SysBus_Mode`、通道 0 的描述符基址和长度、`DMA_CH0_Control` 里的 DSL（见下面的缓存一节）。
   4. MTL：TX/RX 队列 0 打开，存储转发模式。
   5. MAC：写回 MAC 地址，打开 TX/RX，设速度和双工。
   6. 打开 DMA 通道 0 的 TX/RX 和中断（RI、TI、NIE、AIE、FBE），注册中断 106。
3. **PHY（RTL8211F）**：MDIO 通过 MAC 的 `MAC_MDIO_Address` / `MAC_MDIO_Data`（0x200 / 0x204）访问，CSR 时钟分频按 DWMAC4 的表选一个让 MDC 不超过 2.5 MHz 的值。
   - 第一阶段先把千兆广播关掉（reg 9 清掉 bit 8/9），重新协商，只跑 100M，避开第二节的问题。
   - 链路状态：每秒轮询一次 BMSR（reg 1）。速度和双工从 RTL8211F 的 PHYSR（page 0xa43，reg 0x1a）读：bit[5:4] 是速度（2 = 1000M、1 = 100M、0 = 10M），bit 3 是全双工。读到以后改 `MAC_Configuration` 的 PS/FES/DM：1000M 是 PS=0 FES=0，100M 是 PS=1 FES=1，10M 是 PS=1 FES=0。RGMII TX 时钟会跟着速度自动切到 125/25/2.5 MHz，不用改时钟寄存器。
   - 链路起来后调用 `netdev_lower_carrier_on()`，断开调 `netdev_lower_carrier_off()`。
4. **板级**：在 `vsai_logicpi_a1_boardinit.c` 里调用 `s6_eth_initialize()`。

### 第二阶段：千兆

按第二节的验证结果，在驱动里写 PRG_ETH0 的 TX 延时和 PRG_ETH1 的 RX 延时，PHY 那边按 Linux 的做法把它自己的延时关掉，再打开千兆广播。

做完以后用 iperf 跑一下吞吐，同时在主机上抓包或者看 `ifconfig` 的错误计数，确认没有 CRC 错误。

### 第三阶段：不依赖 U-Boot

把 U-Boot 做过的事情补上，这样以后换成别的启动方式（比如 U 盘、eMMC 直接启动不走 tftpboot）也能用网口：

- 打开第一节表格里的四个时钟门控位
- 引脚复用：`eth_pins` 和 `eth_rgmii_pins`，具体的寄存器和 function 号在 Linux 的 pinctrl 驱动（`common_drivers/drivers/gpio/pinctrl/pinctrl-meson-s6.c`）里查，写到现有的 `s6_gpio.c` 里
- GPIOZ_15 复位 PHY
- MDIO 选择器 `0xfe360000 + 0x88` 写 0
- PRG_ETH0：模式 = RGMII、TX 延时、m250 分频、TX 时钟使能、bit 12

**每做一步都在不同的启动方式下回归一次**，不要等全部写完再测。

---

## 四、要特别注意的地方

### 缓存一致性

DMA 不走缓存一致性（设备树里没有 `dma-coherent`），而 NuttX 现在把整个 DRAM（0–2 GB）都映射成普通可缓存内存。所以：

- 发送前对包缓冲区和描述符做 `up_clean_dcache()`；接收前对缓冲区做 `up_invalidate_dcache()`，读描述符前也要先 invalidate。
- Cortex-A55 的缓存行是 64 字节，DWMAC4 的描述符只有 16 字节。四个描述符共用一行时，CPU 写回一个描述符会把 DMA 刚写完的相邻描述符覆盖掉。解决办法二选一：
  - 用 `DMA_CH0_Control` 的 DSL 字段让每个描述符后面空 48 字节（总线宽度 64 位，DSL 单位是 8 字节，DSL = 6），描述符按 64 字节对齐；
  - 或者在 `s6_boot.c` 的 MMU 表里另外划一小块不可缓存的内存（`MT_NORMAL_NC`）专门放描述符。
- 包缓冲区也按 64 字节对齐，长度向上取整到 64 字节。

### 内存

`CONFIG_RAM_SIZE` 现在只有 32 MB（从 `0x03000000` 开始），描述符和缓冲区不要开太多。先用 TX/RX 各 16 个描述符、每个缓冲 1536 字节，再按 iperf 的结果调整。IOB 的数量也要相应设置。

DMA 地址用的是 32 位，NuttX 的内存都在 4 GB 以下，没问题。

### 中断和 SMP

现在开着 `CONFIG_SMP`。中断里只做确认中断和通知上半部，别在中断里拷贝数据或者长时间关中断。之前 16 ms 刷新出现过 NSH 收不到串口输入，网卡驱动别再加重这个问题。

### 不要动的东西

- 设备树里的 `mboxes = <&mbox_fifo S6_REE2AO_ETH>` 是给网络唤醒（WoL）用的，跟正常收发没关系，不用管
- 不要去碰内置 PHY（`int_mdio`、ETH_PLL 那一套），这块板用的是外置 PHY

---

## 五、配置

驱动做好以后，在 `board/contest_board/configs/nsh/defconfig` 里打开（以 `make menuconfig` 的实际结果为准，提交前用 `git diff` 看一下 savedefconfig 有没有把东西删掉）：

```text
CONFIG_S6_ETH=y
CONFIG_NET=y
CONFIG_NET_ETHERNET=y
CONFIG_NET_IPv4=y
CONFIG_NET_TCP=y
CONFIG_NET_UDP=y
CONFIG_NET_ICMP=y
CONFIG_NET_ICMP_SOCKET=y
CONFIG_MM_IOB=y
CONFIG_NETDEV_PHY_IOCTL=y
CONFIG_NETDEV_STATISTICS=y
CONFIG_NETUTILS_NETINIT=y
CONFIG_NETINIT_NOMAC=y            # 驱动自己读 MAC 地址的话可以不要
CONFIG_NETINIT_IPADDR=0xc0a80a02  # 192.168.10.2，跟 TFTP 用的同一个网段
CONFIG_NETINIT_DRIPADDR=0xc0a80a01
CONFIG_NETINIT_NETMASK=0xffffff00
CONFIG_SYSTEM_PING=y
CONFIG_NETUTILS_TELNETD=y
CONFIG_SYSTEM_TELNETD=y
CONFIG_NETUTILS_IPERF=y
CONFIG_NETUTILS_TFTPC=y
```

板子网口直连 TFTP 主机，主机那边的地址是 `192.168.10.1`，跟 U-Boot 用的一样。

---

## 六、验收

按顺序做，每一步都要通过才做下一步：

1. 启动日志里能看到 PHY ID（`001c` / `c916`）和链路速度
2. `ifconfig` 能看到 `eth0`，地址 `192.168.10.2`
3. `ping 192.168.10.1` 通，主机 `ping 192.168.10.2` 也通（两个方向都要测，之前的问题就是只有一个方向通）
4. 主机 `telnet 192.168.10.2` 能进 NSH
5. 主机跑 `iperf -s`，板子 `iperf -c 192.168.10.1`；再反过来测一次。记下 100M 下的吞吐
6. 用 `tftp` 从主机拉一个几 MB 的文件，跟主机上的 crc32 对比
7. 拔掉网线再插上，链路状态能正确变化，ping 能恢复
8. 第二阶段做完后，主机改回千兆，重复 3、5、6
9. 网络开着的时候 launcher 照常能用，帧率没有明显下降

---

## 七、待补充

- 第二节 U-Boot 里的验证结果（PRG_ETH0/1 原值、PHY 延时寄存器原值、改完后千兆是否正常）
- 100M / 1000M 的 iperf 数字
- 第三阶段引脚复用的具体寄存器
