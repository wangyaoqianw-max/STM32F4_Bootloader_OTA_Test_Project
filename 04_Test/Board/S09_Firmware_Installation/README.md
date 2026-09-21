# S09 Metadata Baseline Board Test

该目录提供 `toolkit.bat metadata baseline` 使用的临时 Application board test。它不加入正式 `OTA_APP.uvprojx`，由 Toolkit 在执行期间临时替换 `app_system.c` 并加入工程，流程结束后恢复工程文件。

测试固件使用正式 Application 的 Firmware Storage 和 Metadata V2：

```text
External Loader 预烧录并读回 Slot A
→ 只读验证 Slot A / Slot B
→ 提交两次 Metadata V2 baseline
→ 回读 Slot A / Slot B / Metadata
```

Metadata Baseline 会写入 AT24C02 Metadata，并在结束后重新烧录正式 Application；不会再通过旧 YMODEM 流程写入 W25Q64。

该测试入口只允许由统一 Toolkit workflow 使用，不属于正式 Application 启动链。
