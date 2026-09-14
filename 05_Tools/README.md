# Tools

本目录保存与项目直接相关、需要纳入版本管理的开发辅助工具，并作为人工与不同 AI Agent 访问本地开发环境的统一接口。

当前职责：

- `Config/`：本机工具链配置模板；真实本机路径只保存在被 Git 忽略的 `toolchain.local.bat`。
- `Scripts/`：稳定入口，包括 Keil 编译、J-Link 烧录、RTT 采集和本地开发闭环。
- `Firmware/`：Firmware Image V1 打包工具及其 Python 测试。
- `Debug/`：后续保存调试器配置、诊断脚本和崩溃分析工具。
- `Packaging/`：后续保存 Firmware Image / OTA 包生成与完整性工具。
- `CI/`：后续保存适合自动化环境执行的静态检查、Host Test 和 CI 配置。

设计原则：

```text
GPT / DeepSeek / Claude / 其他 Agent
                ↓
          05_Tools/Scripts
                ↓
       Keil / J-Link / RTT
                ↓
            STM32F411
```

模型不应把“如何找到本机工具”作为日常推理的一部分。机器相关信息由 `Config` 收口，项目相关流程由 `Scripts` 收口。

## 已提供入口

```text
05_Tools/Scripts/build_app.bat       Keil OTA_APP 编译
05_Tools/Scripts/flash_app.bat      J-Link SWD 烧录并运行
05_Tools/Scripts/rtt_capture.bat    J-Link RTT Channel 0 采集
05_Tools/Scripts/run_app_cycle.bat  编译 → 烧录 → RTT 闭环
05_Tools/Firmware/pack_firmware.py  Firmware Image V1 打包
```

四个 Application 脚本共用 `Config/toolchain.local.bat`。本机配置只填写
`KEIL_UV4`、`JLINK_EXE`、`JLINK_RTT_LOGGER` 及目标连接参数，不得提交真实机器路径。
