# S10 Trial Confirm Rollback Test Plan

## Metadata

- Stage: S10_Trial_Confirm_Rollback
- Stage status: READY_FOR_VERIFICATION
- Test role: Verification Role
- Plan status: REVISED_ON_2026-09-21
- Created: 2026-09-20
- Scope: 本阶段尚未形成正式证据的预烧录、板级 Trial / Rollback / 视觉验收，以及完成后的回归和证据整理
- Formal design: design.md
- Implementation plan: implementation_plan.md
- Current evidence: 04_Test/Reports/Stages/S10_Trial_Confirm_Rollback/verification.md
- Current matrix: 04_Test/Reports/Stages/S10_Trial_Confirm_Rollback/verification_matrix.md

本文件是 S10 的独立测试编排方案。它不修改 S10 冻结设计、验收条件或生产代码；执行结果只能写入验证报告和交接文件，不能预先填入本方案。

## 1. 目标与边界

### 1.1 目标

在一个有明确起点和终点的测试批次中，补齐以下证据：

1. Known-Good v1.0 基线可重复建立并可回读；
2. Trial 尚未 Confirm 时的软件复位、IWDG 复位和真实断电均进入 Rollback；
3. Bootloader 完成 TRIAL → ROLLBACK → restore → NONE，且保留 confirmed Slot/Version；
4. Rollback 在破坏性阶段被复位或断电后，下一次启动从头恢复；
5. 关键 LED/LCD 现场现象与 RTT、GDB、Metadata 证据一致；
6. 板测结束后重新执行最终回归，确认没有因测试操作或临时测试资产引入回归。

### 1.2 已有证据，不在执行阶段无目的重复

以下项目当前已有本阶段新鲜证据，只有在前置检查发现固件、工具或目标状态不一致时才重跑：

- S10 Host Test、S10 Contract、既有 S04/S05/S07/S07A/S09 Host 回归；
- Python Firmware Pack、YMODEM 和 S04 Persistence 回归；
- Application / Bootloader Clean Build，且当前记录为 0 error / 0 warning；
- 正式 NONE 启动、Runtime Ready、strict Confirm RTT/GDB 证据；
- IWDG 配置、Debug Halt Freeze、Continue Resume 和 direct no-feed reset；
- 既有 v1.1 YMODEM 传输达到 READY_TO_INSTALL 的证据。

历史日志可以帮助定位问题，但不能替代本方案执行批次产生的新证据。

### 1.3 本方案不吸收的项目

- S09 Deferred Fault Injection：erase/program 分段、Internal CRC、Metadata body/commit-marker 和 S09 Power Loss；若补测，结果只能回填 S09 验证报告；
- S05C 真实 SPI/I2C 逻辑分析仪采集：属于 S05C 的非阻塞 follow-up，不作为 S10 Rollback 证据；
- 新增产品功能、修改 S10 验收条件、重做 Metadata V3、增加 Failure Counter 或第二套 Installer。

## 2. 当前未完成项与测试编号

| 编号 | 测试批次 | 当前状态 | 目标证据 |
|---|---|---|---|
| S10-00 | 测试前只读预检 | 未执行 | 工具、端口、Probe、输出目录和测试资产可用 |
| S10-01 | Known-Good Baseline | BLOCKED；旧 YMODEM 预烧录流程已废弃，新的 External Loader + Metadata Baseline 尚未完成 | Slot A v1.0、Slot B 状态、Metadata NONE、Internal APP、LED/LCD |
| S10-02 | Trial 软件复位回滚 | 未形成正式 PASS | Confirm 前 TRIAL 复位后自动 Rollback |
| S10-03 | Trial IWDG 复位回滚 | 只有无 Trial 边界的 IWDG 证据 | Confirm 前 IWDG reset cause、Rollback、恢复结果 |
| S10-04 | Trial 真实断电回滚 | PENDING | Confirm 前断电、上电后 Rollback 和 v1.0 恢复 |
| S10-05 | Rollback 完成链 | PENDING | Bootloader RTT/GDB、CRC/vector、ROLLBACK → NONE、Metadata 保持 |
| S10-06 | Rollback 中断后从头恢复 | PENDING / NOT_EXECUTED | 破坏性阶段复位/断电后重新从 confirmed Slot 恢复 |
| S10-07 | Known-Good Image destructive gate | 板级证据未形成 | confirmed Header/CRC/Version 无效时，Internal APP 不得擦除 |
| S10-08 | LED/LCD 现场验收 | PENDING / NOT_EXECUTED | OTA、Trial、Rollback、恢复后的可见现象 |
| S10-09 | 最终全回归和证据收口 | 待上述批次完成 | 自动化回归、Build、差异检查、报告和交接一致 |

S10-02 至 S10-04 必须分别执行。一个复位原因的 PASS 不得替代另外两个复位原因的证据。

本轮新增的前置门禁：S10-01 必须先完成 External Loader 的物理写入/读回，再由 Metadata Baseline 临时固件写入并校验 AT24C02，才允许进入 S10-02。旧 YMODEM 预烧录流程及其 Sender 结果不再作为入口证据。

## 3. 固定状态模型

所有测试只允许在以下状态之间转移：

~~~text
F0  Known-Good stable baseline
    confirmedSlot=A, confirmedVersion=1.0.0
    pendingSlot=NONE, upgradeState=NONE
        ↓ install v1.1 and commit TRIAL
F1  Trial before Confirm
    confirmed=A/1.0.0, pending=B, upgradeState=TRIAL
        ↓ reset / IWDG reset / power-cycle
F2  Rollback in progress
    upgradeState=ROLLBACK is persisted before destructive restore
        ↓ confirmed image restore + Internal CRC/vector PASS
F3  Recovered stable baseline
    pendingSlot=NONE, confirmed=A/1.0.0, upgradeState=NONE
        ↓ interrupted restore
F2  next boot restarts restore from confirmed Slot
~~~

禁止的状态跳转：

- 未证明进入 F1 就执行 reset、断电或 Rollback 结论；
- 未证明进入 F3 就开始下一种故障注入；
- 发送端报告成功但未读到目标 RTT/Metadata，就把安装或 Confirm 记为成功；
- 用历史 S09 Factory baseline 代替本轮 F0；
- 在 F2 未完成时直接烧录新固件覆盖现场。

如果实际状态无法归入 F0～F3，标记为 BLOCKED，停止当前批次，先恢复或调查，不跨批次继续。

## 4. 资源和工具互斥规则

### 4.1 固定资源

- Target：STM32F411CE，SWD @ 4000 kHz；
- J-Link：同一时间只能有一个 J-Link Commander、RTT Logger、GDB Server 或 GDB 客户端链路；
- Serial：当前配置为 COM9 / 115200，同一时间只能有一个 YMODEM sender 或串口占用者；
- Power：需要人工可控的目标板电源断开/恢复；
- Output：06_Output/Logs 为工具输出目录，不能让不同批次覆盖同名日志。

### 4.2 入口约束

统一使用既有入口：

~~~text
05_Tools\toolkit.bat build ...
05_Tools\toolkit.bat flash ...
05_Tools\toolkit.bat rtt ...
05_Tools\toolkit.bat snapshot ...
05_Tools\toolkit.bat ymodem python ...
05_Tools\toolkit.bat factory restore ...
~~~

不自行搜索或直接调用 UV4.exe、JLink.exe、JLinkRTTLogger.exe，不建立第二套板测脚本。

### 4.3 日志保存

每个批次开始前建立唯一记录目录：

~~~text
06_Output/Logs/S10/<run-id>/<case-id>/
~~~

run-id 使用本轮日期时间，case-id 使用本方案编号。工具可能写入固定名称的根日志；每次动作完成后立即复制到当前批次目录，并记录命令、开始/结束时间、退出码和目标状态。未复制前不得开始下一次会覆盖同名输出的工具动作。

### 4.4 External Loader + Metadata Baseline 门禁

旧的 Factory Restore/YMODEM W25Q64 预烧录流程已删除。S10-01 固定使用：

~~~text
External Loader 预烧录 Slot A
→ External Loader 独立读回 Header/Payload
→ Metadata Baseline 临时固件只读验证 W25Q64
→ AT24C02 Metadata 写入、回读和基线校验
~~~

External Loader 通过 `W25Q64_STM32F411.stldr` 写入 Payload `0x90001000`，再写入 Header `0x90000000`；Metadata Baseline 不擦除或写入 W25Q64。只有完整取得 Slot A、Slot B、Metadata 的结果后，才能建立 F0。

## 5. 总执行顺序

~~~text
S10-00 只读预检
   ↓
S10-01 Known-Good Factory Baseline
   ↓
S10-02 Trial 软件复位 → Rollback → F3
   ↓
S10-03 Trial IWDG 复位 → Rollback → F3
   ↓
S10-04 Trial 真实断电 → Rollback → F3
   ↓
S10-05 读取并确认完整 Rollback 链
   ↓
S10-06 Rollback 中断 → 下一次启动从头恢复 → F3
   ↓
S10-07 Known-Good Image destructive gate（每个子项独立恢复）
   ↓
S10-08 LED/LCD 现场记录
   ↓
S10-09 最终自动化回归、报告、交接
~~~

执行规则：

1. S10-00 任何硬件资源不满足时，不进入破坏性批次；
2. S10-01 是后续所有板测的入口，不能用历史 baseline 替代；
3. 每个 S10-02～S10-07 完成后必须回到 F3，再开始下一个批次；
4. 任一批次失败，先保存证据和现场，不自动重试、不随机换工具、不跳到下一批次；
5. S10-01、S10-04、S10-06、S10-07 涉及破坏性操作或真实断电，执行前必须由现场人员确认目标、电源和恢复路径；
6. 不具备真实电源控制或稳定 J-Link/串口时，相关用例记为 PENDING 或 BLOCKED，不能用模拟输出替代。

## 6. 批次详细方案

### S10-00：测试前只读预检

#### 前置条件

- 已读取 PROJECT_CONTEXT.md、S10 design.md、implementation_plan.md、handoff.md、本方案和当前验证报告；
- 当前分支、HEAD 与交接记录一致；
- 用户尚未要求执行 Metadata Baseline 或电源操作，本批次不改变目标状态。

#### 固定动作

~~~powershell
git status --short --branch
git rev-parse HEAD
05_Tools\toolkit.bat ymodem devices --json
Test-Path .\06_Output\Packages\app_v1.0.img
Test-Path .\06_Output\Packages\app_v1.1.img
Test-Path .\05_Tools\Config\toolchain.local.bat
Test-Path .\06_Output\Logs\toolkit_jlink.lock
~~~

同时检查：

- 没有外部 Keil Debug、RTT Viewer、RTT Logger、GDB Server 或其他 J-Link 客户端；
- COM9 的实际设备与配置一致；
- v1.0/v1.1 .img 均是 [64-byte Header][Payload]，版本、长度和 CRC 可读；
- 当前工程生产源中没有临时 S10_TEST、TEST ONLY、强制失败或强制复位路径；
- 目标板、电源、J-Link、串口线和 LED/LCD 均连接可靠。

#### 通过判据

所有资产可定位、J-Link 锁未被占用、串口可识别、没有残留测试客户端，且没有未解释的工作区冲突。

#### 失败处理

只记录失败原因并停止。不得因为 COM9 或 J-Link 不可用而改用未声明的端口、探针或工具。

### S10-01：Known-Good Factory Baseline

#### 破坏性确认点

该批次会重写 External Flash Slot A、AT24C02 Metadata 和 Internal APP。没有现场确认时只准备命令，不执行：

~~~powershell
05_Tools\toolkit.bat metadata baseline -ConfirmDestructive -Image .\06_Output\Packages\app_v1.0.img
~~~

#### 固定动作

1. 关闭所有额外 J-Link 客户端，保留串口和目标电源；
2. 执行上面的 Metadata Baseline；其内部先调用 External Loader 预烧录并读回，再启动临时固件写入 AT24C02；
3. 在临时固件报告 Metadata baseline PASS 后、正式 Application 烧录或目标复位前，保存 External Loader 和 RTT 证据；
4. 保存 A/B 原始 64-byte Header、A `+0x1000` 载荷起点、JEDEC ID、Header 解析结果、Payload CRC/Metadata 和时间戳；
5. 只有检查点 C1 通过后，才允许烧录正式 Application；烧录/复位后立即执行检查点 C2，不得先开始 Trial；
6. 读取并记录 Bootloader/应用 RTT 中的 Slot、Metadata、版本、CRC/vector 和启动状态；
7. 现场记录 v1.0 LED、LCD 初始画面和正常运行状态。

#### 预烧录与 Slot A 不变量检查点

所有检查点都必须通过独立的 W25Q64 读取确认，不能只引用 Sender 或目标端自检日志。每次至少读取：

~~~text
Slot A + 0x0000       = 64-byte Header 原始值
Slot A + 0x1000       = Payload 起始数据
Slot B + 0x0000       = 64-byte Header 原始值
W25Q64 JEDEC          = EF 40 17
Header                 = Magic/Format/HeaderSize/ImageSize/CRC 可解析
~~~

固定检查点：

| 检查点 | 时机 | 预期 | 失败定位 |
|---|---|---|---|
| C0 | Slot A/B 破坏性擦除完成、发送前 | A/B Header 均为 EMPTY/擦除态 | 擦除阶段异常，停止，不发送 |
| C1 | `app_v1.0.img` 接收、Header-last 提交和目标自检完成；正式 Application 烧录前 | A=VALID v1.0，A 载荷起点非全 FF；B=EMPTY；Metadata=NONE/A | 预烧录写入、Header 提交或自检链异常 |
| C2 | 正式 Application 烧录并复位后、任何 OTA/按键前 | A 仍为 VALID v1.0，B 仍 EMPTY，Metadata 不变 | C1→C2 边界：正式烧录、复位或启动链影响 W25Q64 |
| C3 | B 接收完成、尚未安装/Confirm 前 | A Header 与 A 载荷起点不变；B=VALID v1.1 或项目定义的待安装态 | OTA 写 B 时误擦 A |
| C4 | Bootloader 完成 PENDING→TRIAL 后、进入 B 前 | A 仍为 VALID v1.0；B=VALID v1.1；Metadata=TRIAL/confirmed=A | Bootloader 状态转换或安装边界影响 A |
| C5 | 已进入 B、尚未确认 B 可用/提交 Confirm 前 | A 仍为 VALID v1.0；confirmed 仍为 A | B 启动或 Confirm 前运行阶段影响 A |

其中 C1～C5 任一点发现 A Header 全 `0xFF`、Magic/CRC 失效或 A 载荷起点全 `0xFF`，立即停止后续测试并保留现场。C1 通过而 C2 失败，不能归因于 YMODEM 写入；C2 通过而 C3 失败，优先排查 OTA 写入目标地址和擦除范围；C3/C4/C5 分别对应 Bootloader 转换和 Trial 运行边界。

#### 必须同时成立的结果

~~~text
Internal APP       = v1.0，可启动
Slot A             = VALID，v1.0
Slot B             = EMPTY 或项目定义的 inactive 状态
confirmedSlot      = A
confirmedVersion   = 1.0.0
pendingSlot        = NONE
upgradeState       = NONE
Application        = RUNNING / STABLE
~~~

此外必须存在 C1、C2 两份独立物理读取证据。没有 C1/C2，只有临时固件 RTT 的 `baseline PASS`，S10-01 仍为 BLOCKED。

#### 判定和停止条件

- Soft-I2C BUSY、BOOT halt、没有初始 C、YMODEM 超时或 Metadata 不可读：BLOCKED，不进入 S10-02；
- 发送端成功但目标没有对应 RTT/Metadata：FAIL，不进入 S10-02；
- C1/C2 任一物理 Header 检查失败：BLOCKED，禁止进入 S10-02；
- 只有上述全部字段、C1/C2 物理读取和现场启动证据都满足，才把状态记为 F0。

### S10-02：Trial 软件复位回滚

#### 目标

证明 Trial 在 Confirm 前发生软件复位时，不被误认为 Confirm，也不依赖 Failure Counter，而是进入 Bootloader Rollback。

#### 共同安装动作

1. 起点必须是 F0；
2. 确认 app_v1.1.img 的 Header、版本和文件 CRC；
3. 启动 RTT 记录；
4. 按冻结板测流程第一次按 PA0 进入 OTA，等待目标报告可接收；
5. 使用 Python YMODEM 发送 v1.1，保存 JSON 结果和目标 READY_TO_INSTALL；
6. 第二次按 PA0 触发安装；
7. 在 Application 进入 TRIAL、自动 Confirm 之前建立 GDB/RTT checkpoint。

#### Confirm 前 checkpoint

必须新鲜记录：

~~~text
upgradeState       = TRIAL
confirmedSlot      = A
confirmedVersion   = 1.0.0
pendingSlot        = B
pending Slot       = VALID
Application system = RUNNING
readyMask          = MAIN + OTA + DISPLAY
Confirm commit     = 尚未发生
~~~

Checkpoint 必须在 firmware_confirm() 事务执行前或等价的 Confirm 请求边界取得。若无法用当前 AXF 符号、RTT 或已有 GDB 资产稳定定位该边界，停止并记为 BLOCKED，不得靠“启动后马上复位”的时间猜测代替。

#### 注入与判定

1. 保存 checkpoint 后只执行一次 GDB software reset；不执行 load、不重新烧录、不擦除 External Flash；
2. 释放 GDB 后读取完整 Bootloader RTT；
3. 再用 Bootloader/Application AXF 做只读 GDB snapshot；
4. 检查 TRIAL → ROLLBACK、confirmed image prevalidate、restore、Internal CRC/vector 和 ROLLBACK → NONE 顺序；
5. 检查最终 confirmedSlot=A、confirmedVersion=1.0.0、pendingSlot=NONE，并看到 v1.0 恢复行为。

最终状态满足 F3 才算 PASS；否则保留现场并停止。

### S10-03：Trial IWDG 复位回滚

#### 目标

把已有的 IWDG timeout 证据与真实 TRIAL 边界连接起来，证明 IWDG reset 也会触发同一条 Rollback 路径。

#### 固定动作

1. 从 F0 重新安装 v1.1，重复 S10-02 的 YMODEM、PA0 和 Confirm 前 checkpoint；
2. 记录 upgradeState=TRIAL、confirmedSlot=A 和 pendingSlot=B；
3. 使用已经验证过的 no-feed 方式停止合法 Feed，使 IWDG 超时；
4. 不使用第二个 J-Link 客户端；不在目标已经自动 Confirm 后才注入；
5. 上电/复位后先抓取 Bootloader RTT，再抓取 Application/Bootloader GDB snapshot；
6. 记录 IWDGRSTF 仅作为诊断字段，Rollback 决策必须仍由 Metadata TRIAL 驱动。

#### 必须成立的结果

- Reset Cause 包含 IWDG 证据；
- Bootloader 看到持久化 TRIAL 并写入 ROLLBACK；
- confirmed image 通过 prevalidate 后完成 restore；
- 最终回到 F3；
- Reset Cause 没有被当作 Rollback 状态机的替代条件。

如果 no-feed 注入不能保持 Confirm 前边界，不能将已有 S10_iwdg_no_feed_gdb.log 复用为本用例 PASS。

### S10-04：Trial 真实断电回滚

#### 目标

证明 Confirm 前的真实 POR/BOR 电源中断会使下一次启动从 TRIAL 进入 Rollback。

#### 固定动作

1. 先恢复 F0：使用新的 External Loader 建立 Slot A v1.0、Slot B 空白和 Metadata `confirmed=A/pending=NONE/upgrade=NONE`，并独立读回 A/B；旧 Factory Restore 不再作为 F0 入口；
2. 在第一次 PA0 前同时启动 RTT 监听和 YMODEM Sender；Sender 必须自动等待初始 `C`，不能等人工回复后才打开串口；
3. 提醒现场人员按第一次 PA0；测试编排继续运行并解析 YMODEM/RTT，发送达到 `READY_TO_INSTALL` 后才提醒第二次 PA0；
4. 第二次 PA0 后保持监听，确认 v1.1 已启动；现场人员只依据当前轮次的状态提示，在 v1.1 LED 闪 3～4 下时断电；不得用固定延时或对话反应时间代替状态判断；
5. 断电期间不关闭测试编排；现场人员恢复电源后，编排自动保存上电后的 Bootloader RTT、Application RTT、Reset Cause 和最终 Metadata；
6. RTT 监听器必须能在 Application RTT 控制块 `0x2000DE04` 与 Bootloader RTT 控制块 `0x200000E0` 之间自动切换或并行归档；单个固定地址的 RTT Logger 不能声称覆盖跨复位/掉电的完整 Bootloader 链；
7. 断电只允许发生在确认尚未提交的窗口之后；若已经看到 Confirm commit，取消本次用例，不得改名为 Trial power-cycle PASS。

#### 通过判据

断电前权威状态为 TRIAL，上电后依次看到 Rollback 决策、Confirmed restore、CRC/vector PASS、NONE，最终状态为 F3。只看到 Application 重新启动或 trial=0 不足以通过。

### S10-05：完整 Rollback 链和状态不变量

该批次不单独制造新故障，而是对 S10-02～S10-04 每次产生的最新 RTT/GDB 证据逐项核对，避免只看“设备最后亮了”。

每一次都必须核对：

~~~text
TRIAL detected
→ ROLLBACK persisted before Internal erase
→ confirmed Slot prevalidated
→ confirmed External Slot used as read-only source
→ Internal image restored
→ local read-back / Internal CRC / vector PASS
→ ROLLBACK → NONE committed
→ pendingSlot = NONE
→ confirmedSlot/version unchanged
→ Application v1.0 visible and stable
~~~

缺任一中间证据，结果为 PARTIAL，不能仅按最终画面记为 PASS。

### S10-06：Rollback 中断后从头恢复

#### 目标

证明 Rollback 已进入 ROLLBACK 后，在破坏性恢复阶段发生复位或断电，下一次启动仍从 confirmed External Slot 重新恢复，而不是信任不完整 Internal APP。

#### 先决条件

- S10-02、S10-03 或 S10-04 至少有一次完整进入 F2 的新证据；
- 已确认当前 AXF/GDB 能定位 Rollback commit、Internal erase/program 的稳定 checkpoint，或者已获得单独批准的 default-off test hook；
- 现场人员能在不误伤 PC、Probe 和供电设备的情况下执行电源动作。

#### 子用例

| 子用例 | 中断点 | 期望 |
|---|---|---|
| S10-06A | ROLLBACK commit 后、Internal erase 前 | 下一次启动读取 ROLLBACK，不重复写入第二个 begin，重新执行 restore |
| S10-06B | Internal erase 后、program 未完成 | 下一次启动不信任残缺 Internal APP，从 confirmed Slot 从头恢复 |
| S10-06C | program 进行中或完成但最终验证前 | 下一次启动仍重新执行完整验证，CRC/vector 未 PASS 前不提交 NONE |

优先使用可回读的 GDB/software reset；只有断点边界可靠且不会错过日志时才使用真实断电。若当前正式固件没有可靠注入点，不得临时凭时间拔电并声称覆盖了指定中断点；应记录 BLOCKED，再单独提出测试 hook 变更。

#### 通过判据

每个已执行子用例都必须有：中断前 RTT/GDB、下次启动 RTT/GDB、重新 restore 的源选择、Internal CRC/vector、最终 Metadata。至少一个破坏性阶段子用例完成，才能关闭 S10-06；其余未执行项逐项标记。

### S10-07：Known-Good Image destructive gate

#### 目标

确认 confirmed image 无效时，Bootloader 在 Internal APP erase 前拒绝 Rollback restore。

#### 子用例

分别从可恢复的 Known-Good 状态建立并执行以下场景，每个场景结束后回到 F0：

1. confirmed Slot Header 无效；
2. confirmed Slot Payload CRC 无效；
3. confirmed image Version 与 confirmedVersion 不一致。

#### 约束

- 只能使用已有的、明确标记为测试用途的 External Flash/Metadata 注入路径；
- 不允许直接修改生产源代码、绕过 Metadata 合同或使用未记录的 J-Link 内存写入；
- 如果当前工程没有安全、可回读的注入入口，该批次记为 BLOCKED，不要为了完成矩阵临时发明工具。

#### 通过判据

每个无效场景都必须证明：confirmed image prevalidate 失败、Internal APP 未被擦除、Rollback 不提交 NONE，并且失败原因可由 RTT/GDB 回读。仅 Host Test PASS 不升级为板级 PASS。

### S10-08：LED/LCD 现场验收

现场观察必须在对应自动化证据已经建立后进行，并使用同一轮测试的状态，不拿历史照片或描述代替。

| 阶段 | 观察内容 | 必须记录 |
|---|---|---|
| F0 v1.0 | v1.0 LED 正常节奏、LCD 初始/IDLE 状态 | 时间、现象、对应 RTT |
| OTA 接收 | PA0 后 LCD RECEIVING，传输中前台行为不异常 | PA0 次数、YMODEM JSON、RTT |
| 镜像校验/安装 | LCD VERIFYING/READY 或项目定义的安装状态 | 目标状态和是否出现错误画面 |
| Trial | v1.1 运行现象、Confirm 前状态 | checkpoint 时间和 LED/LCD |
| Rollback | 回滚期间的启动/错误/恢复状态 | Bootloader RTT 和肉眼现象 |
| F3 | v1.0 LED/LCD 恢复且继续运行 | confirmed metadata、最终 RTT |

观察结果只写 PASS、FAIL 或 NOT_OBSERVED。看不到现象时不能由 RTT 推断“肉眼通过”。

### S10-09：最终回归与证据收口

只有 S10-02～S10-08 的现场动作结束、目标状态已恢复或明确记录为阻塞后才执行。

固定顺序：

1. 检查生产源无临时测试开关、Fault Hook、强制复位和 TEST ONLY 标记；
2. 重新执行 S10 Host Test 五项；
3. 重新执行 Python Firmware Pack、YMODEM、S04 Persistence；
4. 重新执行 05_Tools\Contracts 中 S10、S05A、S07A、S09 和工具入口合同；
5. Application Clean Build；
6. Bootloader Clean Build，并确认 ROM 小于 64 KiB；
7. 执行 git diff --check；
8. 检查 J-Link 锁、临时工程改动、生成物和 Python cache，不把与本测试无关的用户文件加入提交；
9. 把每个用例的命令/动作、退出码、日志路径、结果和未验证项写入 verification.md；
10. 更新 verification_matrix.md、S10 handoff.md、PROJECT_CONTEXT.md 和 current_status.md，保持 代码验证 与 硬件验证 分开；
11. 只有证据完整时才进入 READY_FOR_REVIEW，不得在本阶段直接标记 CLOSED。

## 7. 失败、阻塞和恢复规则

### 7.1 结果定义

- PASS：本用例的全部前置、动作、目标证据和退出状态均满足；
- FAIL：目标行为与设计或验收条件矛盾；
- BLOCKED：工具、硬件、可观测性或安全前置不满足，尚未形成可判定的功能结果；
- NOT_EXECUTED：尚未尝试；
- PARTIAL：有中间证据但缺少该用例的完整闭环，不得当作 PASS。

### 7.2 统一停止条件

出现以下任一情况，立即停止当前批次：

- Soft-I2C BUSY、Boot halt、J-Link ownership 冲突、串口被占用；
- Metadata、Reset Cause 或 RTT 与预期状态不一致；
- 目标已自动 Confirm，导致本次失去 Confirm 前边界；
- Factory Restore、Rollback 或电源动作后的最终状态不明；
- 需要执行未在本方案或工具文档中定义的命令；
- 需要覆盖、删除或清理归属不明的用户文件。

### 7.3 恢复优先级

~~~text
保留现场和日志
→ 释放 J-Link / COM9 / 电源资源
→ 判断当前状态是否仍可读
→ 只有在现场确认后执行 Factory Restore
→ 重新验证 F0
→ 再决定是否重试同一用例
~~~

同一故障重复出现两次后，不再盲目重试，转为 BLOCKED 并记录需要的硬件或工具调查。

## 8. 证据记录模板

每个用例在验证报告中至少记录：

~~~text
Case ID:
Run ID:
HEAD / branch:
Start / End:
Precondition state:
Firmware image + size + version + hash:
Commands / manual actions:
J-Link / COM / power resource:
Expected state transition:
Observed RTT / GDB / sender evidence:
Evidence files:
Result: PASS / FAIL / BLOCKED / NOT_EXECUTED / PARTIAL
Recovery state:
Next permitted case:
~~~

板级功能结论必须同时具备：

~~~text
RTT or GDB authoritative evidence
+ Metadata / image validation evidence
+ 必要的人工现象记录
~~~

## 9. 执行前确认清单

在真正执行 S10-01 前，现场确认以下事项：

- [ ] 已阅读本方案并同意按 S10-00 → S10-09 顺序执行；
- [ ] 已确认新的 External Loader F0 会重写 Slot A、擦空 Slot B，Metadata baseline 会写入 AT24C02；旧 Factory Restore 不作为本阶段入口；
- [ ] 已确认是否允许真实断电测试；
- [ ] 已确认是否具备 Rollback 中断点的稳定注入/观测能力；
- [ ] 已确认 COM9、J-Link、目标电源和 LED/LCD 均可用；
- [ ] 已确认失败时先保留现场，不跨批次继续；
- [ ] 已确认 S09 Deferred 和 S05C follow-up 不混入 S10 PASS。

本清单未确认前，本轮只能执行 S10-00 只读预检，不能执行破坏性批次。
