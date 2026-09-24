# N32H787 单核 M7 热词识别（DS-CNN_S）部署方案

整理日期：2026-09-21（开源 DEMO 裁剪时同步更新引用）。本文记录已实现方案的
设计细节，面向需要深入修改的开发者；快速上手请看根目录 [README.md](../README.md)。

**本工程是单核 Cortex-M7 应用。** Cortex-M4 核保持复位，不使用摄像头、JPEG 或
双核通信；本文档只涉及当前的 KWS 数据路径与内存布局。

## 1. 必须保留的决策

1. 实际芯片为 **N32H787XIB7**，构建目标名 `n32h787_kws_demo`。
2. **运行时完整模型和权重在 DTCM，不能改回从 Flash 读权重。** Flash 只保留掉电存储的加载副本。
3. M7 主体代码与推理内核在 ITCM，模型、tensor arena 和诊断数组在 DTCM。
4. DSP/推理不需要 SDRAM。链接脚本保留 SDRAM 声明不代表已经启用或验证成功。
5. D-Cache 已启用，但 I2S 环形缓冲区不缓存；Flash 也保持不缓存。
6. 音频链路固定 **16 kHz 单声道、921600 8N1**，不连接主机也自动识别并打印结果。
7. **`g_m4_shared` 的字段偏移是 OpenOCD 的 ABI。** `flash_pause`=+680、
   `flash_paused`=+684、`m7_flash_paused`=+688、`m7_flash_abi`=+692、
   `boot_request`=+924，`m4_shared.h` 的六个 `static_assert` 保留。
   新增字段只能追加在 `boot_request` 之后（本轮的 KWS 诊断数组就是这样加的）。

## 2. 模型身份与输入输出

| 项目 | 当前值 |
|---|---|
| 模型文件 | `models/DS_CNN_S.tflite` |
| 文件大小 | 95,480 B；链接段对齐后 95,488 B |
| 模型 MD5 | `9101fcc3499cf45aa2ec6cadd301a703` |
| 模型 SHA-256 | `e4e6af6a3fc7db4f7fdea3430da82f979ad9c342082d6afab13c6810ea85c907` |
| 与参考实现的关系 | 与 `ML-KWS-for-MCU/Pretrained_models/DS_CNN/DS_CNN_S.tflite` **逐字节相同**（MD5 一致） |
| 量化 | **无。float32 模型**，输入输出都是 float |
| 输入 | float32，`[1,490]`，张量名 `fingerprint_input` |
| 输出 | float32，`[1,12]`，张量名 `labels_softmax`，已过 Softmax |
| 运算量 | 约 0.5M MAC/次（DS-CNN-S，Speech Commands v0.02 的 12 类） |
| 算子 | `RESHAPE`×1、`DEPTHWISE_CONV_2D`×5、`CONV_2D`×4、`AVERAGE_POOL_2D`×1、`FULLY_CONNECTED`×1、`SOFTMAX`×1 |
| TFLM arena 预留 | 128 KiB = 131,072 B；实际使用量由 `kws_arena_bytes` 报告，不与预留量混淆 |

12 个类别的下标顺序就是模型输出顺序：`0` silence、`1` unknown、`2..11`
依次为 `yes no up down left right on off stop go`。下发到串口的名称来自
`kws_class_labels[]`，与训练脚本 `[silence, unknown] + 十个目标词` 的构造顺序一致。

使用工程中的 TFLM 静态库 `firmware/USER/tflm_sdk/lib/libtensorflow-microlite.a`，
SHA-256 为 `b181a56b3e5c04b86cdbf59927f716a152ce8009b2bdadebbad746fd2695e4f5`。
这个库里同时有 CMSIS-NN 的 int8 内核和 float 参考内核，本模型走 float 路径，
因此没有 CMSIS-NN 的加速。库版本变化后应重新做结果一致性和性能验证。

代码：[kws_classifier.cc](../firmware/USER/src/kws_classifier.cc)、
[kws_mfcc.c](../firmware/USER/src/kws_mfcc.c)。

## 3. 完整数据路径

```text
WM8978 麦克风（I2S 主机，MCLK/BCLK/LRCK 由编解码器产生）
  -> I2S1 从机接收，16 kHz 立体声 int16
     -> DMA1 通道 0 + DMAMUX1（I2S1_RX）走循环链表，写入 .kws_audio 的 8 块环
        -> 每块完成中断只做 __DMB() 和计数递增
        -> kws_audio_read() 每 60 ms 取一块（960 帧，只取左声道）
           -> 3 个 MFCC 帧（帧移 320 = 20 ms，窗长 640 = 40 ms）
              -> 49×10 特征环形缓冲左移 3 帧
                 -> TFLM Invoke（float32）
                    -> 最近 3 次 softmax 求平均
                       -> 去抖 -> USART1 打印一行
```

编解码器是 I2S 主机，位时钟不会停，所以接收端只需要在启动时对齐一次 WS 边沿，
之后由 DMA 连续搬运。一个 DMA 块被刻意定义成一次应用轮次（60 ms），
块、轮次和特征窗推进量是同一个量，消费侧不必处理半块。

代码：[kws_audio.c](../firmware/USER/src/kws_audio.c)、
[kws_app.c](../firmware/USER/src/kws_app.c)。

## 4. MFCC 前端与 TF 训练前端的对应关系

前端是 Arm 参考实现 `ML-KWS-for-MCU/Deployment/Source/MFCC/mfcc.cpp` 的移植，
只是**输出 float 而不是 q7**——q7 分支的 `sum *= 2^mfcc_dec_bits` 加 clamp
只服务于量化模型，本模型用不到，已删除。

参数与原实现完全一致：16 kHz，窗长 640，帧移 320，周期 Hann 窗，补零到
1024 点 FFT，40 个 mel 滤波器（20 Hz–4000 Hz），10 个 DCT-II 系数。FFT 是自己写的
基-2 复数 FFT，旋转因子在 init 时生成，没有引入 CMSIS-DSP。

模型是 TensorFlow 训出来的，前端必须与
`contrib_audio.audio_spectrogram(..., magnitude_squared=True)` +
`contrib_audio.mfcc(...)` 等价。逐行对照 TF 的 kernel 源码，结论是 Arm 的
`mfcc.cpp` 是忠实移植：

| 环节 | TensorFlow | Arm `mfcc.cpp` | 结论 |
|---|---|---|---|
| 频谱 | `magnitude_squared=True` → 功率 `re²+im²` | `buffer[i] = re*re + im*im` | 一致 |
| 开方位置 | 在 mel 滤波器组内部 `spec_val = sqrt(input[i])` | mel 循环内 `arm_sqrt_f32` 后乘权重 | 一致 |
| mel 带宽 | `mel_span/(num_channels_+1)` = span/41 | `span/(NUM_FBANK_BINS+1)` = span/41 | 一致 |
| 三角窗 | 中心 `mel_low+(i+1)*spacing`，跨 `[c_{bin-1}, c_{bin+1}]` | `left/center/right` 用 `+bin*Δ` 展开 | 完全等价 |
| mel 刻度 | `1127*log1p(f/700)`，20–4000 Hz | `1127*logf(1+f/700)`，`MEL_LOW/HIGH_FREQ` | 一致 |
| log 下限 | `kFilterbankFloor = 1e-12` | 仅在 `mel_energy == 0.0` 时填 `FLT_MIN` | 可忽略 |
| DCT | `fnorm = sqrt(2/N)` 统一，无 i=0 特例 | `sqrt(2.0/input_length)` 统一 | 一致 |

两处已知的无关紧要差异：TF 把 `mel_energy` 下限钳到 `1e-12`，Arm 只在恰好为 0 时
填 `FLT_MIN`；Arm 的滤波器组只扫到 bin 511（TF 覆盖到 512），但 4000 Hz 以上权重
本来就是 0。**不要"顺手修正"成 TF 的写法**：这份前端与模型是配套的。

## 5. 滑动窗口、平均与去抖

参数取自 Arm 参考实现的 `realtime_test`：`recording_win = 3`、
`averaging_window_len = 3`、`detection_threshold = 90`（float 模型下即 `> 0.90f`）。

- 每轮消费 960 采样（60 ms），算 3 个新 MFCC 帧，49 帧特征缓冲左移 3 帧。
- **冷启动保护**：前 17 轮（49/3）缓冲未填满，直接跳过推理，避免开机打出垃圾热词。
- 每轮推理一次，对最近 3 次 softmax 输出求平均，取平均后的 argmax。
- 只有「最优类平均分 > 0.90 **且** 类别与上次播报不同」才打印。最优类回落到
  Silent/Unknown 时重新武装，同一热词在沉默前只打印一次。
- 只播报 `yes no up down left right on off stop go`（下标 2..11），
  Silent 和 Unknown 不播报。
- 音频流出现断点时 `kws_audio_take_discontinuity()` 会置位，此时必须复位 MFCC
  分帧和滑动窗口——被静默拼接进特征窗的缺口是热词识别唯一无法恢复的错误。

## 6. 串口协议

USART1，921600 8N1（PA9/PA10）。固件用 `uart_log.c` 轮询 `USART_FLAG_TXDE` /
`USART_FLAG_TXC` 发送；工程用 `--specs=nosys.specs` 且没有 `_write`/`fputc`
重定向，**libc 的 `printf` 不会输出任何东西**。

开机打印一行类别表：

```text
kws: ds-cnn-s float32, 16 kHz, classes: silence unknown yes no up down left right on off stop go
```

检测到一个热词就打印一行 `<keyword> <score>%\r\n`：

```text
yes 92%
```

初始化失败时打印 `kws: WARNING ...` 行，但不中止：状态码同时写进诊断数组，
可以用调试器读。

主机侧读串口的工具是 `tools/kws_monitor.py`（`--port`/`--baud`/`--seconds`/`--log`）。

## 6.1 PB3 指示灯

**和打印同一个条件**：任何一个热词通过阈值和去抖、准备打印时，PB3 红灯亮 1 秒
（`KWS_LED_MS`）。Silent/Unknown 不亮，低于 90% 不亮，同一个词第二次出现要等
回到沉默再亮。

- **PB3 低电平点亮**（active-low），和 PF10、PI8 一样。`GPIO_Configuration()`
  在切成输出模式之前先把这三个脚拉高，所以「灭」就是 `GPIO_SetBits`。改极性会
  让灯常亮。
- 计时用 `mwTick` 的到期时间，不是在循环里 toggle：热词是按人说话的节奏随机到来
  的，没有固定周期可跟。`led_service()` 每轮查一次，所以实际亮
  `1000 ~ 1060 ms`（一轮音频 60 ms）。
- 到期比较写成 `(int32_t)(mwTick - g_led_deadline) >= 0`，**不要**写成
  `mwTick >= g_led_deadline`：`mwTick` 每 49 天回绕一次，无符号比较会让灯在回绕
  之后一直亮着。`tools/tests/test_kws_led.py` 里有一条用例专门守这个。
- PI8 与 PF10 都是 500 ms 心跳：与 PI8 同一处 `heartbeat` 到期翻转驱动，每 500 ms
  一起 `GPIO_TogglePin`，含义是「程序还在跑」。PB3 只表示「刚识别到一个词」。
  初始化失败的死循环分支里也会同步翻转这两只脚，便于在固件挂掉时从灯态判活。

## 7. RAM 规划

`system_n32h7xx.c` 的 `TCM_SIZE_VALUE = 0x15`：ITCM 256 KiB、DTCM 512 KiB、
AXI SRAM2 256 KiB，另外固定 AXI SRAM1 128 KiB。
**TCM 与可分配 AXI SRAM 之间存在容量取舍，不能只扩大 linker LENGTH。**

下表为 2026-09-21 本地 ELF 的布局快照（`text 249,452 / data 1,768 / bss 202,680`）；
区间采用 `[起始,结束)`，符号地址随后续构建可能变化。

| 区间 / 起点 | 分配用途 | 说明 |
|---|---|---|
| `0x00000400..0x00040000` | M7 ITCM 代码区 | `.QuickCodes`；低 1 KiB 不占用 |
| `0x15000000..0x15006000` | bootloader | USB-MSC 引导保留区 |
| `0x15006000..0x151e0000` | 应用 Flash | bootloader 暴露的 LUN 上界见 `bootloader.h` |
| `0x20000000..0x20017500` | `.kws_model` | 95,488 B；MODEL_DTCM 共 128 KiB，余 32 KiB |
| `0x20020000..0x20048740` | `.kws_arena` | DTCM；tensor arena 128 KiB + TFLM 对象状态 |
| `0x200486a0` | `g_kws_diag` | 24 个 word，落在 `.kws_arena` 里，DTCM 不参与 D-Cache |
| `0x20050000..0x20080000` | FlashOS 暂存窗口 | 烧录算法、栈、返回断点、两个 62 KiB 缓冲；链接脚本有 ASSERT 挡住 arena 越界 |
| `0x24000000..0x240078c0` | `.kws_audio` | 30,912 B；I2S 环形缓冲与 DMA 描述符 |
| `0x2401e000..0x24020000` | M7 私有 data/bss/stack | 8 KiB；MSP 顶 `0x24020000` |
| `0x30000000..0x30000400` | `g_m4_shared` 邮箱 | 字段偏移是 OpenOCD ABI，见第 1 节 |
| `0x30015000..0x30015400` | OpenOCD work area | 供其 CRC 校验例程使用 |
| `0x30015f00` | 启动标志对 | `m7_flash_poll()` 读取，bootloader 请求跳转用 |

两条必须守住的边界，都在链接脚本里有 ASSERT：

- **DMA 不能访问 TCM。** I2S 环形缓冲必须在 AXI SRAM，且落在 `m7_cache.c` 的
  MPU region 1（`0x24000000..0x2407FFFF`，Normal non-cacheable）里。region 2 把
  `0x2401e000` 起的 8 KiB 改回 write-back 给 M7 自己用，所以
  `.kws_audio` 有一条 `ASSERT(... <= 0x2401e000)`。
- **arena 不能长进 FlashOS 暂存窗口。** `_ekws_arena <= 0x20050000` 的 ASSERT
  就是为此加的；两者重叠不会破坏 Flash，只会让烧录布局悄悄依赖 arena 的大小。

布局依据：[M7 linker](../firmware/Makefile/LinkFile/n32h787_kws_demo_CM7.ld)、
[system_n32h7xx.c](../firmware/Driver/CMSIS/device/system_n32h7xx.c)、
[m7_cache.c](../firmware/USER/src/m7_cache.c)。

## 8. 启动顺序与模型搬运

`main()` 的顺序受缓存/MPU 与外设缓冲的依赖约束，不能随意调整：

1. `SCB->VTOR = 0x15006000`，`PWR_Configuration()`、`RCC_Configuration()`、
   `GPIO_Configuration()`、`DMA_Configuration()`、`I2C_Configuration()`、
   `USART_Configuration()`、`SysTick_Config(600000U)`、`NVIC_Configuration()`、
   `CPU_DELAY_INTI()`。
2. 拉低 `RCC->M4RSTREL` 的 EN 位，把第二核保持在复位。本固件是单核，
   但被覆盖的上一版固件可能已经把 M4 放出来了，而 `board_sdram.c` 和编解码器
   驱动的总线归属检查都假设这一位是清的。
3. `SDRAM_Configuration()`（此时 D-Cache 还没开）。
4. `kws_uart_init()`、`kws_mfcc_init()`、`kws_classifier_init()`。
5. 清邮箱控制字（`flash_pause` / `flash_paused` / `m7_flash_paused` /
   `boot_request`），再发布 `g_m4_shared.magic`。这一步必须在
   `m7_cache_enable()` **之前**：邮箱区开缓存后由 OpenOCD 在背后写，
   清字要发生在任何可能轮询它的代码之前。
6. `m7_cache_enable()`：关 D-Cache → 重写 MPU → 开 D-Cache，并发布
   `m7_flash_abi`（OpenOCD 的烧录闸门靠它）。
7. `kws_audio_init()`：WM8978 走 I2C4 配置并起 DMA。**必须在缓存/MPU 就绪之后**，
   否则 CPU 可能把 AXI SRAM 的环形缓冲缓存下来读到脏数据。
8. `kws_app_run()` 无限循环，每轮仍调用 `m7_flash_poll()`，保留 USB 重新烧录通道。

启动代码（`startup_n32h78x_cm7_gcc.s`）在 `main()` 之前依次搬运 `.QuickCodes`、
搬运**完整 `.kws_model`** 到 DTCM 并逐 word 校验、清空 NOLOAD 的 `.kws_arena`，
然后才跑 C++ 构造函数。所以 TFLM 的 C++ 对象状态放在 `.kws_arena` 里是安全的。
搬运是纯 word 循环、没有尾字节处理，`.kws_model` 的大小必须是 4 的倍数——
95,488 B 满足，`embed_tflite.py` 的 `alignas(16)` 保证段对齐。

`g_kws_model_data` 的运行地址是 `0x20000000`；Flash 加载地址由 `_sikws_model`
给出，会随构建变化，不能硬编码在拷贝逻辑里。
`.kws_model >MODEL_DTCM AT>FLASH` 里的 Flash 是加载地址，**并非推理地址**。

启动文件是厂商文件，仍然 import 旧的 `_sperson_*` / `_person_*` 名字；
链接脚本里把这几个符号别名到 KWS 的同名符号（`_skws_model` 等），
启动文件本身无需改动。

## 9. Cache 与烧录安全

当前 MPU 策略见 [m7_cache.c](../firmware/USER/src/m7_cache.c)：

- AHB `0x30000000` 起 512 KiB：邮箱与启动标志对，对 M7 不缓存且不可执行。
- AXI `0x24000000` 起 512 KiB：I2S 环形缓冲，不缓存且不可执行，CPU 与 DMA 视图一致。
- M7 私有 `0x2401e000` 起 8 KiB：更高优先级 region，write-back/write-allocate。
- Flash `0x15000000` 起 2 MiB：只读、Normal non-cacheable。
- TCM 直接供 M7 执行和读写，性能不依赖 D-Cache。

本板此前在 Flash D-Cache line fill 时出现过错位数据/故障，因此暂不缓存 Flash，
不把"开缓存后不报错"当成模型结果正确。

烧录走 `n32h7x_flash`：先把第二核按在复位（`n32h7x_hold_m4_in_reset`），
再让 M7 在轮次间隙 clean/disable cache 后应答，然后交给 FlashOS。
`n32h7x_hold_m4_in_reset` 只保留 `mww 0x58030174 0`——摄像头、DVP2/JPEG 总线主设备
和旧 M4 的邮箱握手都已随本轮移除。

## 10. 构建、烧录与验收

Windows 下在 `firmware/Makefile` 目录执行 `build.cmd flash` 即可一键构建并烧录；
Linux/WSL 从工程根目录：

```sh
make -C firmware/Makefile -j4 PYTHON=python
python -m pytest tools/tests -q                       # 主机侧单元测试

make -C firmware/Makefile probe   PYTHON=python       # 只连接不写入
make -C firmware/Makefile flash   PYTHON=python       # 构建并烧录应用到 0x15006000

# 复查链接布局和模型身份，不触碰硬件
arm-none-eabi-size -A firmware/Makefile/build/n32h787_kws_demo.elf
arm-none-eabi-nm -n firmware/Makefile/build/n32h787_kws_demo.elf | grep kws
md5sum models/DS_CNN_S.tflite
```

串口监视器与烧录互不争用（NSLink 的调试接口和虚拟串口相互独立），烧录时
可以保持 `tools/kws_monitor.py` 运行，复位后会打印新固件的开机横幅。

运行期诊断数组 `g_kws_diag` 仍保留在固件中，下标在
[kws_app.h](../firmware/USER/inc/kws_app.h) 里命名（`KWS_DIAG_*`）。
如需读取，可在应用保持运行时用 OpenOCD 的 `mdw` 手工读回（先 halt、读完即
resume，**不要为读诊断额外复位**：BOOT 在重启后短暂关闭 AP0，而适配器没有
SRST，异常时只能断电恢复）。重点看：

| 下标 | 期望 |
|---|---|
| `KWS_DIAG_ARENA_BYTES` | 远小于 128 KiB，否则收敛 arena 预留 |
| `KWS_DIAG_INVOKE_MAX` | 换算成 ms；决定 60 ms 的一轮够不够 |
| `KWS_DIAG_LOST_BLOCKS` / `KWS_DIAG_DMA_ERROR` | 都应为 0 |
| `KWS_DIAG_READ_TIMEOUTS` / `KWS_DIAG_DISCONTINUITIES` | 应为 0 |
| `KWS_DIAG_CLASSIFIER_ERR` / `KWS_DIAG_AUDIO_STATUS` / `KWS_DIAG_CODEC_STATUS` | 都应为 0 |
| `KWS_DIAG_INPUT_TYPE` / `KWS_DIAG_OUTPUT_TYPE` | 都是 1（`kTfLiteFloat32`） |
| `KWS_DIAG_ROUND_MAX` | 整个轮次含等音频的耗时 |

**单轮推理超过约 50 ms 时**，把 `KWS_RECORDING_WIN` 从 3 调到 5（100 ms 一轮）：
一轮超时会让音频环溢出，表现是随机丢块而不是识别错误。

真机说话测试：串口终端 921600 观察，对麦克风说 `yes`/`no`/`stop` 等，
确认打印对应热词、不连续刷屏、静默时不误报，同时 PB3 红灯随识别亮约 1 秒。

## 11. 已知限制

- 真机识别率与摆放环境（麦克风增益、音源距离、噪声）相关，未在本 DEMO 中
  做针对性调参。若识别明显不对，先怀疑 MFCC 数值链路：`kws_mfcc.c` 不含任何
  硬件依赖，可以编成 x86 程序，配一段 numpy 参考实现对同一段音频逐帧比对，
  一次就能定位是前端还是模型的问题。第 4 节的对照表就是为这条路径准备的。
- SDRAM 在本应用中未被 DSP/推理链路使用（链接脚本保留 SDRAM 声明仅为兼容
  启动流程中的初始化）。
