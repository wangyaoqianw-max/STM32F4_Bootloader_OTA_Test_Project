# Scripts

保存自动化检查、数据转换和批处理脚本。脚本必须说明输入、输出和运行环境。

## Application Build

统一 Application 编译入口：

```bat
05_Tools\Scripts\build_app.bat
```

该脚本负责：

- 读取 `05_Tools\Config\toolchain.local.bat`；
- 校验本机 Keil uVision 路径；
- 编译 `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx` 的 `OTA_APP` Target；
- 将 Keil 输出保存到 `06_Output/Logs/OTA_APP_build.log`；
- 通过进程退出码向人工或 Agent 返回 PASS / WARN / FAIL 状态。

首次使用时复制：

```text
05_Tools/Config/toolchain.local.example.bat
```

为：

```text
05_Tools/Config/toolchain.local.bat
```

然后按当前电脑实际安装位置修改工具路径。`toolchain.local.bat` 为本机配置，禁止提交 Git。
