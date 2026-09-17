# S06 RTOS Runtime / Concurrency Model Design

## Metadata

- Stage: `S06_RTOS_Runtime`
- Status: `DESIGN_APPROVED`
- Branch: `main`
- Current Role: `S06 Design Role`
- Owner: Project Owner
- Updated At: `2026-09-17`

## 1. Objective

S06 不再承担 FreeRTOS 移植任务。当前 Application 已经运行 FreeRTOS，本阶段目标是正式冻结 Application Runtime / Concurrency Model，为 S07 `OTA_Service_V1` 提供稳定、可解释、可验证的任务拓扑、资源所有权和 IPC 基础。

本阶段同时完成现有 ST7789 显示能力的板级适配，并将 LCD 纳入 Runtime，作为 OTA 运行状态的可视化出口。

S06 结束后应能够明确回答：

```text
Application 中长期存在几个 Task？
每个 Task 负责什么？
UART/Ymodem 由谁消费？
Firmware Download 由谁拥有？
Display 由谁拥有？
Task 之间如何通信？
哪些代码允许阻塞？
OTA 运行时正常业务是否继续？
错误发生后哪些模块降级、哪些属于 Fatal？
```

## 2. Current Baseline

当前 Application 已具备：

```text
FreeRTOS Kernel
Platform RTOS abstraction
├─ thread
├─ queue
├─ notify
├─ mutex
├─ semaphore
├─ timer
└─ time

S05 Ymodem path
PC Sender
   ↓
USART1 / DMA / RingBuffer
   ↓
service_uart
   ↓
ymodem_parser
   ↓
ymodem_receiver
   ↓
ymodem_sink
   ↓
Firmware Storage
   ↓
W25Q64 Slot B
```

当前启动关系为：

```text
defaultTask
   ↓
app_system_start()
   ↓
appSystem
   ↓
app_main()
```

`defaultTask` 仅承担启动桥接职责，启动 `appSystem` 后删除自身。当前 `app_main()` 仍为单一长期循环，因此 S06 的核心工作不是“增加 RTOS”，而是把 Application 从单一长期循环正式整理为可扩展的并发 Runtime。

## 3. Design Principles

S06 遵循以下原则：

1. Task 按“独立调度 / 独立阻塞 / 独立资源所有权”拆分，不按模块数量机械拆 Task。
2. 不建立 `UART Task + Ymodem Task + Flash Task + CRC Task + LED Task` 等过细线程模型。
3. 优先使用 Event-driven / Blocked Task，禁止无意义 Busy Loop。
4. Service 与 Task 分离：Service 是能力边界，Task 是执行上下文。
5. 资源优先采用 Single Owner（单一所有者），只有真实共享时才增加 Mutex。
6. OTA UI 只消费业务语义，不感知 Ymodem Packet、Flash 地址等底层细节。
7. LCD 故障不应阻止 Application 正常运行或 OTA；OTA 故障不应让前台 Application 失去正常业务能力。
8. S06 不提前实现 S07 的 `PENDING / Reset Request` 业务状态机，也不实现 Bootloader Trial / Confirm / Rollback。

## 4. Runtime Task Topology

S06 第一版冻结三个长期 Application Task：

```text
                     FreeRTOS Runtime
                           │
                    defaultTask
                           │
                    start appSystem
                           │
                    delete itself
                           ▼
                       appSystem
                    Priority NORMAL
                    /              \
                   /                \
                  ▼                  ▼
           otaWorker             displayTask
        Priority ABOVE_NORMAL  Priority BELOW_NORMAL
                  │                  │
               BLOCKED            BLOCKED
```

长期 Application Task：

```text
1. appSystem
2. otaWorker
3. displayTask
```

FreeRTOS Idle / Timer Service 等 Kernel Task 不属于本阶段 Application Task 拆分范围。

## 5. appSystem — Foreground Application Task

`appSystem` 继续沿用当前已有线程，不新增额外 `foregroundTask`。

职责：

```text
Application lifecycle
Worker Task startup
正常前台业务
Demo firmware behavior
Application-level status
```

第一版 Demo Firmware 行为：

```text
Firmware v1.0
→ 正常工作表现：LED Blink

Firmware v1.1
→ 正常工作表现：LED PWM Breath
```

LED 在这里用于直观证明当前运行的是哪个 Firmware Version，而不是承担 OTA 状态指示职责。

OTA 下载期间，v1.0 的 LED Blink 应继续运行，以证明 OTA 是 Background Runtime，不阻断正常 Application 前台工作。

初始优先级：

```text
PLATFORM_THREAD_PRIORITY_NORMAL
```

## 6. otaWorker — Background OTA Worker

`otaWorker` 是后台固件接收执行上下文。

平时状态：

```text
otaWorker
   ↓
wait notification / runtime event
   ↓
BLOCKED
```

OTA 活动时：

```text
UART DMA / Rx Event
        │
        ▼
    RingBuffer
        │
   Task Notify
        │
        ▼
    otaWorker
        │
  service_uart
        │
  Ymodem Receiver
        │
 Firmware Storage
        │
 Image Validation
        │
 result/status
        │
        ▼
     BLOCKED
```

第一版职责：

```text
UART/Ymodem session execution
Firmware receive
Slot B write
Firmware image validation
OTA runtime state/progress generation
```

不拆分独立：

```text
UART Task
Ymodem Task
Flash Task
CRC Task
```

S05 已验证的数据路径继续复用，S06 只改变其正式 Runtime Ownership。

初始优先级：

```text
PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL
```

理由是 UART 接收具有时效性，RX 数据到达后应尽快消费 RingBuffer；但该 Task 必须使用 Blocked / timeout 机制，不能因为优先级较高而形成长期 Busy Loop。

## 7. displayTask — Display Resource Owner

`displayTask` 是 ST7789 / Graphics 的唯一 Application Owner。

```text
appSystem / otaWorker
        │
   Display Event
        │
        ▼
   Display Queue
        │
        ▼
    displayTask
        │
   Display Model
        │
 platform_graphics
        │
      ST7789
        │
      SPI1
```

约束：

```text
appSystem 不直接刷新 ST7789
otaWorker 不直接刷新 ST7789
其他 Service 不直接争用 Display Bus
```

所有 UI 更新通过 display event 进入 `displayTask`，由 Display Task 自己更新 Model 并 Render。

初始优先级：

```text
PLATFORM_THREAD_PRIORITY_BELOW_NORMAL
```

Display 属于软实时输出，即使刷新晚几十毫秒也不会破坏通信正确性，因此优先级低于前台业务和 OTA UART 消费路径。

## 8. LCD / ST7789 Board Adaptation

S06 第一项实现任务就是完成当前已预留显示能力的板级适配。

当前硬件连接冻结为：

```text
PB10 → LCD_RST
PA1  → LCD_BL / Backlight
PA4  → LCD_CS
PA5  → LCD_CLK  / SPI1_SCK
PA6  → LCD_DC
PA7  → LCD_MOSI / SPI1_MOSI
```

屏幕为单向写入，因此不要求 LCD MISO。

现有软件基础：

```text
platform_graphics
platform ST7789 driver
platform_bsp display SPI binding
SPI1 display bus
```

S06 需要补齐的板级 binding：

```text
LCD CS GPIO
LCD DC GPIO
LCD RESET GPIO
LCD BACKLIGHT GPIO
Display static config
```

Display static config 至少包括：

```text
width
height
x offset
y offset
MADCTL
max SPI clock
```

其中 X/Y Offset、MADCTL 必须通过真实屏幕显示确认，不凭经验硬编码后直接视为完成。

## 9. Display Content Boundary

LCD 用于 Runtime / OTA 状态可视化，而不是替代 v1.0/v1.1 LED Demo。

第一版显示信息建议：

```text
FW VERSION : V1.0
SYSTEM     : RUNNING
OTA STATE  : IDLE
TARGET     : SLOT B
PROGRESS   : 0%
RESULT     : NONE
```

OTA 接收时：

```text
OTA STATE  : RECEIVING
PROGRESS   : xx%
```

接收完成后：

```text
OTA STATE  : VERIFYING
```

最终：

```text
OTA STATE  : SUCCESS / FAILED
```

S06 不要求 LVGL。第一版继续使用现有 Graphics 字符 / 字符串绘制能力。

## 10. IPC Model

S06 第一版不建立 Global Event Bus。

真正需要冻结的 IPC 只有两条：

```text
UART ISR / RX callback
        │
        │ Task Notification
        ▼
     otaWorker

     otaWorker
        │
        │ Queue
        ▼
    displayTask
```

### 10.1 ISR → otaWorker

使用 `platform_notify`。

用途：

```text
UART RX READY
OTA START / WAKEUP
CANCEL / SHUTDOWN（如后续需要）
```

第一版可预留 bit flag：

```text
OTA_NOTIFY_START
OTA_NOTIFY_UART_RX
OTA_NOTIFY_CANCEL
OTA_NOTIFY_SHUTDOWN
```

具体 flag 值在实现时冻结。

### 10.2 otaWorker → displayTask

使用 `platform_queue`，因为需要传递结构化状态。

Display Event 只携带 UI 需要的业务语义：

```text
OTA_IDLE
OTA_RECEIVING
OTA_VERIFYING
OTA_SUCCESS
OTA_FAILED
```

可附带：

```text
progress
image size
target version
error code
```

Display Event 不携带：

```text
Ymodem raw packet
Flash raw address
UART DMA internal state
```

### 10.3 appSystem IPC

S06 第一版不强制建立：

```text
appSystem → displayTask
otaWorker → appSystem
```

以后 S07 需要升级成功后请求 Reset、Metadata 更新或 Application 状态变化时再扩展，不提前设计复杂 Event Bus。

## 11. Display Model

`displayTask` 内部维护自己的 Display Model：

```text
display_model_t
├─ firmwareVersion
├─ systemState
├─ otaState
├─ otaProgress
├─ targetSlot
└─ lastError
```

运行方式：

```text
Display Event
      ↓
update model
      ↓
render model
```

Display 不应每次为了绘制界面反向查询 OTA Worker 内部状态。

OTA Progress 需要节流，避免每个 Ymodem Packet 都触发 LCD redraw。第一版建议按以下任一条件触发刷新：

```text
progress change >= 5%
OR
elapsed >= 200 ms
```

具体参数可在板测阶段调优，不属于架构硬约束。

## 12. Resource Ownership

S06 第一版资源 Ownership 冻结为：

| Resource / State | Owner |
| --- | --- |
| Application lifecycle | `appSystem` |
| Firmware demo LED behavior | `appSystem` |
| USART1 Ymodem receive session | `otaWorker` |
| Firmware download operation | `otaWorker` |
| Slot B firmware write | `otaWorker` |
| Firmware image validation execution | `otaWorker` |
| ST7789 | `displayTask` |
| SPI1 Display Bus usage | `displayTask` |
| Display Model | `displayTask` |

SPI2 本身保持 Platform shared resource 定位，不在 S06 强行变成 OTA 私有设备；但 Firmware Download 期间 Slot B Program/Erase 的业务 Owner 为 `otaWorker`。

## 13. Task Lifecycle

启动流程：

```text
main
 │
 ├─ HAL / Clock / GPIO / DMA / SPI / UART base init
 ├─ MX_FREERTOS_Init
 └─ scheduler start
        │
        ▼
   defaultTask
        │
   app_system_start()
        │
   delete itself
        │
        ▼
     appSystem
        │
        ├─ create otaWorker
        │      └─ self init → READY → BLOCKED
        │
        └─ create displayTask
               └─ display init → READY → BLOCKED
        │
        ▼
 Application normal runtime
```

Worker Task 长期存在，不采用“OTA 开始时动态创建、结束后删除”的模式。

## 14. Initialization Ownership

采用混合初始化模型：

```text
Global/base hardware
→ existing startup / appSystem orchestration

Task-owned resource runtime init
→ owning Task
```

`displayTask` 自己完成：

```text
Display SPI runtime preparation
ST7789 init
Backlight control
Initial screen render
```

`otaWorker` 自己完成：

```text
OTA runtime preparation
UART/Ymodem session runtime preparation
Firmware receive context preparation
```

共享 SPI2 生命周期不强制整体迁移给 OTA Worker，避免未来其他 SPI2 设备无法扩展。

## 15. Blocking And Timeout Policy

### appSystem

允许：

```text
RTOS delay
bounded periodic wait
Application event wait
```

禁止：

```text
OTA receive busy loop
Flash erase/program 长时间占用前台线程
LCD full redraw 直接阻塞前台业务
```

### otaWorker

允许：

```text
Task notify wait
UART/Ymodem timeout wait
bounded storage operation
```

要求：

- 没有 OTA 时处于 `BLOCKED`；
- 等待 UART 数据时不轮询；
- 现有 Ymodem Timeout / Retry / Cancel 语义继续复用；
- Flash 操作必须有明确返回值和失败路径。

### displayTask

允许：

```text
Queue wait
bounded LCD render
```

没有 Display Event 时应处于 `BLOCKED`，不做无意义刷新。

## 16. Error Recovery Model

错误按 Fatal / Degraded 区分。

### Display failure

```text
ST7789 init failure
Display render failure
        ↓
DISPLAY DEGRADED
        ↓
RTT / EasyLogger evidence
        ↓
Application + OTA continue
```

屏幕故障不能阻止 OTA。

### OTA failure

```text
Ymodem timeout / cancel / CRC failure
Storage write failure
Image validation failure
        ↓
OTA FAILED
        ↓
Display / Log update
        ↓
Application foreground continues
        ↓
otaWorker returns to recoverable BLOCKED state where possible
```

### Application core failure

真正无法继续运行的 Application Runtime 初始化失败才进入 Fatal 路径。

## 17. Logging Boundary

S06 不新增独立 Log Task。

原则：

```text
ISR 保持最小化
Task context 输出 Runtime log
不为了日志改变 OTA 时序
```

实施阶段需要确认现有 `service_log` 在多 Task 调用下的实际线程安全边界；若底层已有序列化则直接复用，若不存在则只针对真实共享点增加保护，不建立复杂 Logging Manager。

## 18. Verification Strategy

S06 测试优先复用现有 Toolkit：

```text
toolkit build
toolkit flash run
toolkit rtt
toolkit snapshot halt|resume
toolkit firmware pack
toolkit ymodem
toolkit logic spi|i2c
```

### LCD

当前 Logic Analyzer 固定接在：

```text
SPI2 / W25Q64
Software I2C / AT24C02
```

因此 S06 不要求采集 LCD SPI1 波形。

LCD 验收采用：

```text
Visual inspection
+
RTT init/runtime evidence
```

只有出现花屏、错位、颜色错误、偶发初始化失败等问题时，才临时把 Logic Analyzer 转接到 SPI1 调试。

### Runtime Concurrency

核心验收场景：

```text
v1.0 normal runtime
LED continues blinking
LCD shows OTA IDLE
        ↓
toolkit ymodem
        ↓
OTA receiving in background
LED still blinking
LCD shows RECEIVING / Progress
SPI2 writes firmware
        ↓
Image validation
        ↓
LCD shows SUCCESS or FAILED
        ↓
otaWorker returns BLOCKED
```

证明：

```text
Foreground business continues
OTA background works
Display observes runtime state
```

GDB Runtime Snapshot 用于检查 Task 状态与关键变量；RTT 用于初始化、状态切换、错误路径证据；Logic Analyzer 继续用于 SPI2/I2C 需要时的外部总线证据。

## 19. Out Of Scope

S06 不实现：

- LVGL；
- Touch / CTP；
- Global Event Bus；
- Device Manager；
- 独立 UART Task；
- 独立 Flash Task；
- 独立 Ymodem Task；
- 独立 LED Task；
- OTA Metadata `PENDING` 提交；
- 自动 Reset 请求；
- Bootloader 安装；
- Trial / Confirm / Rollback；
- Watchdog Health Arbitration；
- OTA encryption / signature。

## 20. S06 Completion Contract

S06 完成时必须得到稳定 Runtime Contract：

```text
Long-lived Tasks
├─ appSystem
├─ otaWorker
└─ displayTask

ISR → otaWorker
└─ Task Notification

otaWorker → displayTask
└─ Queue

Foreground
└─ v1.0 Blink / v1.1 Breath

Background
└─ Ymodem / Firmware Storage / Validation

Display
└─ Runtime / OTA status visualization
```

验收至少证明：

1. LCD/ST7789 板级适配可用；
2. 三线程 Runtime 正常启动；
3. Worker 空闲时正确阻塞，无明显 Busy Loop；
4. v1.0 前台 LED Blink 与后台 Ymodem 同时运行；
5. LCD 能正确显示 OTA 状态和进度；
6. Slot B 接收后仍可通过现有 Firmware Validation；
7. Ymodem 中止/失败后前台 Application 继续运行；
8. Display 故障不会破坏 OTA 核心路径；
9. Build / Flash / RTT / GDB / Ymodem 现有 Toolkit 回归不被破坏；
10. S07 可以在该 Runtime Contract 上直接实现正式 OTA Service V1。
