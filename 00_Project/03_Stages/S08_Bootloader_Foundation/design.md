# S08 Bootloader Foundation Design

## Metadata

- Stage: `S08_Bootloader_Foundation`
- Status: `DESIGN_APPROVED`
- Owner: `Project Owner`
- Date: `2026-09-18`
- Baseline Commit: `fba93ad7ea36321d218790b50bcb493e95574b32`

## Goal

建立独立、精简、可诊断的裸机 Bootloader，并完成可靠的 Bootloader → Application 启动链。

S08 只交付三个核心能力：

```text
1. Internal Flash 分区与 Application 重定位
2. Bootloader 诊断基础：SEGGER RTT + 轻量 boot_log + CmBacktrace
3. Application Vector 校验与可靠跳转
```

S08 不处理 OTA 安装、PENDING 消费、回滚和看门狗；这些继续属于 S09/S10。

## Baseline

已建立独立 Bootloader 工程：

```text
03_Firmware/Bootloader/OTA_Bootloader/
├─ Core/
├─ Drivers/
├─ MDK-ARM/
└─ OTA_Bootloader.ioc
```

当前工程事实：

- MCU: `STM32F411CEU6`
- Runtime: Bare-metal，无 FreeRTOS
- MCU peripheral library: HAL 优先
- Cortex-M core 操作: CMSIS / 必要寄存器
- CubeMX firmware package: STM32CubeF4 V1.27.1
- SPI2 / W25Q64 引脚已在 .ioc 中预配置
- PB6 / PB7 已按 AT24C02 软件 I2C GPIO Open-Drain 预配置
- 本阶段不实现 W25Q64 / AT24C02 Driver

## In Scope

- 冻结 Bootloader / Application Internal Flash Layout；
- 修改 Bootloader Keil IROM；
- 修改 Application Keil IROM；
- 将 Application Vector Table 重定位到新的 APP Base；
- 建立统一 `APP_BASE_ADDR` / memory layout contract；
- 移植 Application 中已验证的 SEGGER RTT Vendor；
- 移植 Application 中已验证的 CmBacktrace Vendor；
- 将 CmBacktrace 配置改为 Bare-metal Cortex-M4；
- 建立轻量 `boot_log`，不引入 EasyLogger；
- 实现 APP Initial MSP / Reset_Handler 读取与合法性检查；
- 实现 Bootloader runtime cleanup；
- 设置 VTOR、MSP 并跳转 APP Reset_Handler；
- 验证正常 APP 可启动，非法 APP 被拒绝；
- 验证跳转后 Application FreeRTOS / SysTick / 外设中断正常；
- 记录 .map / .bin 实际体积。

## Out of Scope

- W25Q64 Driver；
- AT24C02 Driver；
- Software I2C Driver；
- Firmware Image 安装；
- PENDING Metadata consume；
- Internal Flash erase/program；
- Trial / Confirm / Rollback；
- IWDG / Reset Cause / Failure Counter；
- LCD / ST7789；
- Ymodem；
- Bluetooth；
- SHA / AES / HMAC / Digital Signature；
- Bootloader FreeRTOS；
- EasyLogger；
- 为 Bootloader 复制 Application 的 App / Service / Platform / Impl 五层架构。

## 1. Runtime and Architecture

Bootloader 保持裸机顺序执行：

```text
Reset_Handler
    ↓
main()
    ↓
minimal MCU init
    ↓
diagnostics init
    ↓
validate APP vector
    ↓
cleanup Bootloader runtime
    ↓
VTOR / MSP / Reset_Handler handoff
    ↓
Application Reset_Handler
```

第一版自研目录建议：

```text
OTA_Bootloader/
├─ Boot/
│  ├─ boot_main.*
│  ├─ boot_validate.*
│  └─ boot_jump.*
├─ Config/
│  ├─ boot_config.h
│  ├─ memory_layout.h
│  └─ cmb_user_cfg.h
├─ Diagnostics/
│  ├─ boot_log.*
│  ├─ cmbacktrace_port.*
│  └─ cmbacktrace_fault_handlers.S
├─ Vendor/
│  ├─ RTT/
│  └─ CmBacktrace/
├─ Core/
├─ Drivers/
└─ MDK-ARM/
```

CubeMX 生成的 Core/Drivers 保持其生成职责；自研 Bootloader 逻辑不塞入 `main.c`。

设计原则：

```text
Boot Core
→ 只关心启动决策、校验与跳转

Diagnostics
→ 只提供日志和 Fault 诊断

Config
→ 保存地址、开关和静态配置

Vendor
→ 保存第三方原始库
```

不为固定的 VTOR/MSP/NVIC 操作增加 Platform/Impl 抽象。

## 2. Internal Flash Layout

S08 冻结：

```text
STM32F411CEU6 Internal Flash: 512 KiB

Bootloader
Start : 0x08000000
Size  : 0x00010000
Range : 0x08000000 ~ 0x0800FFFF
       64 KiB

Application
Start : 0x08010000
Size  : 0x00070000
Range : 0x08010000 ~ 0x0807FFFF
       448 KiB
```

统一静态合同：

```c
#define BOOT_FLASH_BASE_ADDR   (0x08000000UL)
#define BOOT_FLASH_SIZE        (0x00010000UL)

#define APP_BASE_ADDR          (0x08010000UL)
#define APP_FLASH_SIZE         (0x00070000UL)
#define APP_FLASH_END_ADDR     (0x08080000UL)
```

`APP_FLASH_END_ADDR` 为 exclusive end。

### Bootloader Link

Keil IROM：

```text
Start = 0x08000000
Size  = 0x00010000
```

### Application Link

Keil IROM：

```text
Start = 0x08010000
Size  = 0x00070000
```

### Application Vector Table

Application 必须同时把 VTOR 配置到：

```text
0x08010000
```

链接地址与 VTOR 必须一致。Application 仍应能够独立启动其 Reset_Handler 并在 `SystemInit()` 阶段建立正确 VTOR；Bootloader 跳转前再次写 VTOR 作为 handoff contract。

## 3. Diagnostics

### 3.1 SEGGER RTT

RTT Vendor 直接复用 Application 当前已验证版本。

Bootloader 不移植 EasyLogger。

### 3.2 Lightweight boot_log

最小日志目标：

```text
BOOT_LOG_I(...)
BOOT_LOG_W(...)
BOOT_LOG_E(...)
```

底层直接使用 SEGGER RTT，不经过 Service/Platform/EasyLogger。

推荐日志内容：

```text
BOOT start
APP vector MSP
APP vector Reset_Handler
APP validation PASS/FAIL
jump start
jump rejected reason
```

日志不得成为启动成功的必要条件。

### 3.3 CmBacktrace

CmBacktrace Vendor 直接从 Application 复制；工程适配重新编写。

Bootloader 配置：

```c
#define CMB_USING_BARE_METAL_PLATFORM
#define CMB_CPU_PLATFORM_TYPE CMB_CPU_ARM_CORTEX_M4
```

Fault 输出继续使用 raw RTT，不依赖 `boot_log` 高层封装。

`cm_backtrace_init()` 工程信息改为 Bootloader 身份。

Fault 路径：

```text
HardFault / MemManage / BusFault / UsageFault
                  ↓
             CmBacktrace
                  ↓
             SEGGER RTT
                  ↓
                 halt
```

## 4. APP Vector Validation

读取：

```text
APP_BASE_ADDR + 0x00 → Initial MSP
APP_BASE_ADDR + 0x04 → Reset_Handler
```

最低合法性检查：

### MSP

必须落在 STM32F411CEU6 有效 SRAM 地址范围内，并满足 Cortex-M stack alignment 基本要求。

### Reset_Handler

必须：

- Thumb bit = 1；
- 清除 bit0 后地址位于 Application Flash Range；
- 不能是 erased value / obvious invalid vector。

S08 只做启动安全所需的 Vector sanity check，不把 Firmware Header/CRC/Metadata 校验混入本层。完整 Firmware Image validation 属于 S09。

## 5. Jump Contract

跳转前必须完成所有会继续使用当前 stack/local variable 的操作。

推荐顺序：

```text
validate APP
↓
log final jump information
↓
disable global IRQ
↓
stop SysTick
↓
disable / clear pending NVIC IRQ
↓
deinit Bootloader 已实际初始化且需要清理的外设/core state
↓
set SCB->VTOR = APP_BASE_ADDR
↓
data/instruction synchronization barrier as required
↓
set MSP = app_msp
↓
branch to app_reset_handler
```

重要约束：

- `__set_MSP()` 之后不得继续普通 C 函数调用、日志、HAL 调用或访问旧栈局部变量；
- 跳转必须立即进入 Application Reset_Handler；
- cleanup 只针对 Bootloader 实际建立的状态，不机械堆叠无依据的反初始化；
- Application 自身仍负责 `SystemInit → C runtime → main → HAL_Init → FreeRTOS`。

## 6. CubeMX Boundary

SPI2 与 PB6/PB7 已预配置用于后续 S09，但 S08 不实现存储能力。

S08 实施后尽量避免无必要重新运行 CubeMX。若必须重新生成：

- 第三方库、自研 Boot/Config/Diagnostics 不放入 CubeMX 生成目录；
- Fault Handler 与生成的 `stm32f4xx_it.c` 冲突必须显式解决；
- 重新生成后必须检查 Keil Group、include path、Heap/Stack、Fault Handler、VTOR 和 IROM 是否保持。

## 7. Verification Strategy

### Build / Static

- Bootloader Clean Build: 0 error；
- Application Clean Build: 0 error；
- 检查两个 Keil IROM 无重叠；
- 检查 Application VTOR = `0x08010000`；
- 检查 Bootloader .map / .bin <= 64 KiB；
- `git diff --check`。

### Diagnostics

- RTT 可观察 Bootloader 启动日志；
- 受控 Fault 能输出 CmBacktrace；
- Fault 后目标进入稳定 halt/loop；
- 不依赖 EasyLogger / FreeRTOS。

### Normal Jump

```text
Power/Reset
→ Bootloader @ 0x08000000
→ APP vector valid
→ Jump @ 0x08010000
→ Application startup
→ appMainTask / otaWorker / displayTask runtime normal
```

至少验证：

- Application LED/foreground behavior 正常；
- FreeRTOS tick 正常；
- Application 外设中断正常；
- RTT 可识别 Bootloader → Application 两段日志。

### Negative Tests

至少覆盖：

- invalid MSP → reject；
- invalid Reset_Handler → reject；
- erased/invalid APP vector → reject；
- Bootloader 不得跳到随机地址；
- 拒绝后保持可诊断状态。

## 8. Acceptance Criteria

1. Bootloader 链接范围固定为 64 KiB，Application 链接范围固定为后续 448 KiB；
2. Application VTOR 与链接地址一致为 `0x08010000`；
3. 两工程 Clean Build，无 Flash overlap；
4. Bootloader 使用 bare-metal + HAL/CMSIS，无 FreeRTOS；
5. RTT + 轻量 `boot_log` 工作；
6. CmBacktrace 使用 Bare-metal Cortex-M4 配置并能采集受控 Fault；
7. APP MSP / Reset_Handler 校验生效；
8. 合法 APP 可稳定跳转并进入 Application Reset_Handler；
9. 跳转后 Application FreeRTOS tick / interrupts / Runtime 正常；
10. 非法 APP 被拒绝并输出原因；
11. Bootloader .map/.bin 证明未超过 64 KiB；
12. 未实现 S09/S10 范围功能。

## 9. Stage Boundary to S09

S08 向 S09 提供：

```text
Fixed Internal Flash Layout
+
Independent Bootloader build
+
Diagnostics
+
APP Vector Validation
+
Reliable APP Jump
```

S09 在此基础上再引入 W25Q64 / AT24C02 / Metadata / Internal Flash Installation，不回头重写 S08 启动链。
