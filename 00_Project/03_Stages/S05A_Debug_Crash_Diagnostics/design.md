# S05A Debug & Crash Diagnostics Design

## 1. Stage Metadata

- Stage: `S05A_Debug_Crash_Diagnostics`
- Status: `CLOSED`
- Branch: `main`
- Baseline Commit: `ec6119f64306d027c86208329823cf42b77ceebf`
- Design Basis: Project Owner supplied S05A implementation plan, 2026-09-15
- Design Commit: `32f3368`
- Updated At: `2026-09-15`

## 2. Goal

建立一套不依赖 GUI 的 Agent 可调用调试与 Fault 诊断链，保持现有
Keil / ARMCC 构建方式，使用 Keil AXF、J-Link GDB Server、GNU Arm GDB、
Cortex-M Fault Context、CmBacktrace 和 SEGGER RTT 生成可交叉核对的现场证据。

本阶段不迁移 GCC / CMake，不引入 OpenOCD、完整 Core Dump 或 Bootloader
Fault Recovery 框架。

## 3. Architecture

```text
Agent
  ├─ Build / Flash / RTT existing tools
  └─ GDB automation
       ├─ Runtime Snapshot
       └─ Fault Capture
            ↓
       J-Link GDB Server
            ↓ SWD
       STM32F411CE
            ├─ Cortex-M Fault Context
            └─ CmBacktrace → raw RTT
```

工程自研适配位于 `04_Impl/impl_diagnostics`；CmBacktrace 源码保留在
`05_Vendors/CmBacktrace`，不修改其原始实现。Fault Handler 只依赖静态
上下文、CMSIS 系统寄存器、raw RTT 和 CmBacktrace，不依赖普通日志服务、
Mutex、Queue、动态内存或阻塞 UART。

## 4. Frozen Tool Contract

```text
Target: STM32F411CE
Interface: SWD
Speed: 4000 kHz
GDB Port: 2331
```

GDB Runtime Snapshot：

```text
resume: attach → snapshot → continue& → disconnect → quit
halt:   attach → snapshot → detach → quit
```

GDB 自动化不得执行 `load`。Flash 由 `flash_app.bat` 独立负责。

受控 Fault 的固定顺序为：

```text
Build
→ flash_app.bat prepare
→ 启动 J-Link GDB Server
→ 启动 GDB 客户端并 target remote
→ 设置 Fault Capture breakpoint
→ monitor reset
→ continue&
→ 保存 Fault Context / CmBacktrace RTT
→ detach / quit
→ 释放 J-Link
→ 启动 RTT Logger
```

J-Link 同一时刻只有一个 Owner；RTT Logger 必须在 GDB 和 Server 释放
Probe 后启动。

## 5. Fault Contract

受控测试类型：

```text
Invalid Address
Undefined Instruction
Divide by Zero
```

测试入口由 `DIAG_FAULT_TEST_ENABLE` 编译期关闭，测试构建显式打开。
Divide-by-zero 测试先设置 `SCB_CCR_DIV_0_TRP_Msk`。

Fault Context 至少包含：

```text
EXC_RETURN, stacked SP, MSP, PSP
CFSR, HFSR, MMFAR, BFAR
R0, R1, R2, R3, R12, LR, PC, xPSR
```

项目自有汇编适配器独占 HardFault、MemManage、BusFault 和 UsageFault
四个入口，并把硬件提供的 EXC_RETURN 与自动压栈地址交给 C 适配层。

## 6. Acceptance

- 手工 GDB 控制能力和 Runtime Snapshot resume/halt 通过真实板测；
- `continue& → disconnect → quit` 后 MCU 继续运行；
- `detach` 后 MCU 保持暂停；
- 三类受控 Fault 均能生成 GDB、SCB、源码、Backtrace、Stack 和 RTT 证据；
- GDB 与 CmBacktrace 的 Fault PC / 核心现场基本一致；
- 失败路径返回非零并清理本次创建的进程；
- 不隐式烧录 Flash，J-Link 在工具切换后可再次使用；
- 正常固件 Build / Flash / RTT 回归通过。

## 7. Deferred Boundary

S04 Reset Persistence 和 Power-cycle Persistence 继续作为跨阶段延期回归，
不把未执行结果改写为 PASS；两项必须在 `S07_OTA_Service_V1` 关闭前完成。
