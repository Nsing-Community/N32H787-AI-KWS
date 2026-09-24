# N32H7x CMSIS-DAP

Linux 与 Windows 使用同一份配置：`n32h7x_cmsisdap.tcl`。烧录算法驱动在
`flashos/`，三个文件必须保持同目录、不可改名（tcl 按相对路径 source）：

| 文件 | 作用 |
| --- | --- |
| `flashos/flashos.tcl` | 烧录驱动：SRAM 布局、BKPT 调用协议、EraseRange、双缓冲编程循环与耗时报告 |
| `flashos/flashop.bin` | Flash 算法本体（612 字节），加载到片内 SRAM 中执行 |
| `flashos/flashop_sym.tcl` | 算法入口偏移与 blob 长度，缺失则拒绝加载 |

算法细节、SRAM 布局选择依据与实测速度见 [flashos/README.md](flashos/README.md)。
`flash_op/` 是早期版本使用的厂商 blob，当前流程不再引用，仅保留存档。

## Supported Setup

- 实测 MCU：**N32H787XIB7**，Cortex-M7 / AP0（应用为单核 M7，M4 保持复位）。
- 调试器：板载 **NSLink**，CMSIS-DAPv2（usb_bulk），VID:PID `19f5:3106`，SWD。
- Flash 范围：`0x15000000..0x151dffff`，扇区 4096 字节。本 DEMO 应用位于
  **`0x15006000`**，前 24 KiB 为出厂 USB-MSC Bootloader 保留区，烧录不擦除。
- OpenOCD：Linux xPack OpenOCD 0.12.0+，或 N32Studio 自带的 Windows 版本。
- SWD 速率：连接/复位阶段 **1000 kHz**（保守），编程/校验阶段 **6000 kHz**
  （实测拐点，再高速率无收益）。
- 烧录成功后执行一次软件复位并 `reset run`；编程或校验失败时不会复位。

## make 目标

在仓库根目录：

```sh
make -C firmware/Makefile probe   # 只连接/poll，不复位、不写入，连接异常时先跑它
make -C firmware/Makefile flash   # 构建并烧录应用到 0x15006000，校验通过后复位运行
```

Windows 下推荐直接使用 `firmware/Makefile/build.cmd flash`（自动定位 N32Studio
自带的 make、GCC 与 OpenOCD）。

## 烧录流程做了什么

`n32h7x_flash` 在擦除前依次完成：连接 AP0（等待 BOOT 关闭 AP0 的窗口结束）、
halt 并校验 CPUID、**检查/配置 TCM 内存分区**、与已运行固件的 cache 握手
（`n32h7x_quiesce_cached_m7`）、保持 M4 复位、停 SysTick、清活动异常、关中断，
随后把算法加载到 DTCM `0x20050000`，擦除占用扇区（单条 `EraseRange`）、
分块编程并逐块在片上校验，最后 UnInit 并 `reset run`。

### TCM 分区自动配置

固件按 0x15 分区编译（256 KiB ITCM + 512 KiB DTCM + 256 KiB AXI SRAM2）。
若芯片处于出厂默认分区（TCM 未使能，DTCM 地址不可访问），配置会在 halt 后
自动向 SDK 邮箱寄存器写入期望值并触发一次系统复位，由 Boot ROM 完成重映射，
随后自动重连并继续烧录——首次烧录时日志中出现一次复位与 AP0 重连属正常现象。
该操作只改 RAM 分区邮箱，不修改 Flash 或 option bytes，与固件 `SystemInit`
中 `ConfigTcmSize(0x15)` 的行为完全一致，不会造成复位循环。

## Windows 与 WSL

- **Windows**：`firmware\Makefile\build.cmd flash`。若 NSLink 的 CMSIS-DAP
  接口未绑定 WinUSB，使用 `tools\winusb\install_nslink_winusb.ps1` 安装
  （N32Studio 通常已自动完成）。
- **WSL**：先在 Windows 侧 `usbipd list` 找到 NSLink 总线号，再
  `usbipd attach --wsl --busid <busid>` 把设备挂进 WSL；用完
  `usbipd detach --busid <busid>` 归还 Windows。同一时刻只有一侧能占用调试器。
- NSLink 的虚拟串口与调试接口相互独立，串口监视器可以在烧录时保持打开。

## 复位与异常恢复

- NSLink 未接 SRST，正常烧录不依赖硬件复位，`reset_config none`。
- BOOT 在每次复位后都会短暂关闭 AP0；脚本已内置等待与 re-examine 逻辑。
  若连接持续报 AP0 disabled / Polling failed（例如手动复位后立即访问内核），
  **对板子断电再上电**是最可靠的恢复手段，然后重新执行 `make flash`。
- 本配置提供的是 `n32h7x_flash` 命令，不是供 GDB `load` 使用的原生 Flash bank。
