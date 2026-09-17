# probe-rs + J-Link WinUSB 兼容性实验报告

## 实验元数据

- 实验执行日期：2026-09-16
- 报告归档日期：2026-09-17
- 报告归档基线：`90eb83947f2aaf2945cf863590b96586ca4e8326`
- 分支：`main`
- 目标 MCU：`STM32F411CEU6`
- Debug Probe：`J-Link V9.8 PLUS`（Hardware V9.x）
- probe-rs：`0.31.0`，git commit `3de1cae`
- Windows：x86_64
- 协议：SWD
- 实验范围：验证 probe-rs 作为现有 Toolkit 的候选 Probe / Debug Backend；不修改生产固件、Flash Layout、S05B 正式实现或系统 PATH

> [!summary]
> `probe-rs 0.31.0` 在 J-Link V9 的 Debug Interface 切换为 WinUSB 后，可以完成 STM32F411CE 的 Probe 枚举、SWD Target 识别、Memory Read、Reset、Halt / Run、Flash / Verify、RTT、GDB Server 和 GNU GDB 调试。实验结束后已恢复 SEGGER Driver，并重新通过原有 Toolkit 的 Build / Flash / RTT / Snapshot 回归。结论为 `PROBE_RS_BACKEND_FEASIBLE`，但当前 J-Link V9 在 Windows 下需要在 SEGGER Driver 与 WinUSB 之间切换，暂不替代现有默认 SEGGER Backend。

## 1. 实验前基线

实验前使用 SEGGER 官方驱动与现有工具链：

```text
J-Link Debug Interface (MI_02)
  Driver: SEGGER jlink.inf
  Version: 2.70.8.0

J-Link CDC UART (MI_00)
  Driver: SEGGER jlinkcdc.inf
  Version: 1.34.0.44950
```

probe-rs 环境识别结果：

| 检查项 | 结果 | 说明 |
|---|---|---|
| `probe-rs --version` | PASS | `probe-rs 0.31.0` |
| STM32F411CE Target Database | PASS | 芯片名称可查询 |
| `probe-rs list` | PASS | 可枚举 J-Link V9.8 PLUS |
| `probe-rs info` | FAIL | SEGGER Driver 下无法打开 Debug Interface，提示 `incompatible driver is installed for this interface` |

注意：该失败路径曾出现错误文本明确存在但进程 Exit Code 为 0 的情况，因此 probe-rs Adapter 不能仅使用 Exit Code 判断业务成功。

## 2. WinUSB 切换

J-Link V9 的 J-Link Configurator 中 WinUSB 选项不可用，因此使用 Zadig 只替换 Debug Interface：

```text
Device: BULK interface (Interface 2)
USB ID: 1366:0105:02
Current Driver: jlink 2.70.8.0
Target Driver: WinUSB
```

切换后确认：

```text
MI_02:
  Active Driver: oem542.inf
  Provider: libwdi
  Service: WinUSB
  Stack: \Driver\WINUSB
  ProblemCode: 0

MI_00:
  SEGGER jlinkcdc.inf
  未改变

USB Composite Device:
  Microsoft usb.inf
  未改变
```

实验只修改 `MI_02`，未修改 CDC UART 和 USB Composite Device。

## 3. probe-rs 功能验证

| 功能 | 结果 | 证据 / 说明 |
|---|---|---|
| Probe Enumeration | PASS | J-Link V9.8 PLUS 可被 `probe-rs list` 枚举 |
| Target Info / SWD | PASS | 成功连接 STM32F411CE，WinUSB → J-Link → SWD 链路建立 |
| Memory Read | PASS | 成功读取目标 SRAM |
| Reset | PASS | Reset 后可重新连接 Target |
| Halt / Run | PASS | 通过 probe-rs GDB Server 完成；`probe-rs debug` 非交互 Halt/Run 入口记为 LIMITED |
| Flash | PASS | 已知正常 Application 成功写入 |
| Verify | PASS | 写入后校验成功 |
| RTT Run | PASS | 成功捕获运行时 RTT 日志 |
| RTT Attach | PASS | 不重新 Flash 的情况下连接运行中 Target 并读取 RTT |
| GDB Server | PASS | probe-rs GDB Server 可供 GNU Arm GDB 连接和调试 |
| GNU GDB | PASS | Register、Memory、Backtrace、Variable、`continue&` 均成功 |

Flash 仅在 Target Info 和只读 Memory Read 已成功后执行，未在 USB / SWD 状态不确定时写入 Flash。

## 4. RTT 与 GDB 关键证据

RTT 捕获到正常 Application 启动信息：

```text
EasyLogger V2.2.99 is initialize success
log service initialized
Application Foundation start
Storage SPI init result: 0
Application init result: 0
```

GNU GDB 通过 probe-rs GDB Server 成功解析运行现场：

```text
FreeRTOS prvIdleTask
uwTick = 0x36e95
```

同时验证了寄存器读取、内存读取、Backtrace、变量读取以及 `continue&` 恢复运行。

`probe-rs debug` 的交互式 CLI 在本次版本中没有作为 Agent 自动化的明确非交互 Halt / Run 主入口，因此该入口记为 `LIMITED`；明确的 Halt / Continue 能力已通过 GDB Server 路径验证。

## 5. 已知限制

### 5.1 Driver Backend 互斥

当前 J-Link V9 + Windows 组合下：

```text
SEGGER Driver
  → JLink.exe / J-Link GDB Server / J-Link RTT Logger / Keil Debug

WinUSB
  → probe-rs
```

probe-rs 在恢复 SEGGER Driver 后重新执行 `info` 会再次提示 `incompatible driver`，这是预期行为，说明当前已回到 SEGGER 官方驱动链路。

因此当前不适合把 probe-rs 设置为该 J-Link 的默认 Backend，也不适合在同一个 Driver 状态下同时依赖 SEGGER 工具和 probe-rs。

### 5.2 Semantic Result 不能只看 Exit Code

本次观察到两类需要 Adapter 语义判断的情况：

1. `probe-rs info` 在 Driver 不兼容时可以输出明确错误，但 Exit Code 不一定能独立代表业务结果；
2. GDB Server 清理阶段出现过 `failed to fill whole buffer`，但主体连接、调试操作、Target 状态和 Probe 释放均已成功。

未来若实现 `ProbeRs Adapter`，应综合：

```text
ExitCode
+ stdout
+ stderr
+ Target 状态
+ Probe 释放状态
```

形成语义结果，而不是机械使用 `ExitCode == 0`。

## 6. SEGGER Driver 恢复与原工具链回归

实验结束后恢复 J-Link Debug Interface 的 SEGGER Driver，并重新拔插 Probe。

恢复状态：

```text
MI_02: jlink.inf / oem25.inf / 2.70.8.0
MI_00: jlinkcdc.inf / oem27.inf / 1.34.0.44950
USB Composite Device: unchanged
Probe Owner: released
```

恢复后重新执行完整 Recovery 回归：

| 项目 | 结果 | 说明 |
|---|---|---|
| J-Link Commander | PASS | STM32F411CE / SWD / Cortex-M4 正常识别，VTref 约 3.285 V |
| `toolkit.bat build` | PASS | Exit 0 |
| `toolkit.bat flash run` | PASS | Exit 0 |
| `toolkit.bat rtt 5` | PASS | Exit 0 |
| `toolkit.bat snapshot resume` | PASS | Exit 0 |
| Probe Owner cleanup | PASS | 实验结束后 Probe 已释放 |
| Git 工作区 | CLEAN | 实验本身未留下生产代码修改 |

因此原有 `Keil + SEGGER J-Link + GNU GDB + RTT` 主链路已恢复。

## 7. 结论

```text
PROBE_ENUMERATION:              PASS
STM32F411CE_SWD:                PASS
MEMORY_READ:                    PASS
RESET:                          PASS
HALT_RUN:                       PASS
FLASH_VERIFY:                   PASS
RTT_RUN_ATTACH:                 PASS
PROBE_RS_GDB_SERVER:            PASS
GNU_GDB_COMPATIBILITY:          PASS
SEGGER_TOOLCHAIN_RECOVERY:      PASS

PROBE_RS_BACKEND:               FEASIBLE
ORIGINAL_TOOLCHAIN:             RESTORED
```

本实验说明 probe-rs 可以作为当前 Toolkit 的候选 MCU Probe / Debug Backend，但不替代 Toolkit 本身。更合理的长期结构仍为：

```text
Human / Agent
      ↓
Toolkit
      ↓
Workflows
      ↓
Backend / Adapter
├─ SEGGER J-Link
└─ probe-rs
```

当前 S05B 已完成并关闭，本实验不回写 S05B 正式实现，不改变阶段状态。未来若实际引入 ProbeRs Backend，应以独立 Toolkit Enhancement 的方式增加，并保持现有 J-Link Backend 可用。
