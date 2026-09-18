# S07 Factory Provisioning Board Test

本目录提供一次性的 S07 Factory Provisioning 板测入口，用于建立以下初始状态：

```text
Slot A         = v1.0 VALID image
Slot B         = EMPTY
confirmedSlot  = A
confirmedVersion = image header version
pendingSlot    = NONE
upgradeState   = NONE
```

测试入口复用 production `ota_firmware_sink`、YMODEM Receiver、Firmware Storage 和
Metadata persistence，不引用 S05 Board Test sink。它只在以下保护条件同时满足时继续：

- 全新空白状态：EEPROM 没有有效 Metadata 副本，Slot A/Slot B Header 均为空；或
- 已确认的 S04 测试残留状态：Metadata 为 `confirmedSlot=NONE`、Slot A `EMPTY`、Slot B
  `VALID`，且 Metadata 版本与 Slot B Header 一致。

任一条件不满足时，入口拒绝继续，不会擦除 Slot 或覆盖 Metadata。对第二种已确认的 S04
残留状态，只有 Slot A 新镜像完整接收并验证成功后，入口才擦除旧 Slot B 并提交新的
Metadata baseline。入口本身不属于正式
`OTA_APP` 启动链，也不应加入正式 `OTA_APP.uvprojx`。

## 临时运行步骤

1. 在本地测试副本中将 `app_s07_provision_test.c/.h` 加入 Keil 工程，并加入本目录 Include Path。
2. 在 `01_APP/system/app_system.c` 临时启动 `app_s07_provision_test_run()`，不要同时启动正式 `otaWorker`。
3. 编译临时测试镜像，确认 RTT 输出 `[S07-PROVISION] YMODEM_READY`。
4. 先打开串口 Sender，再烧录/复位临时测试镜像，发送已经由 `toolkit firmware pack` 生成的 `.img`：

   ```text
   05_Tools\toolkit.bat firmware pack --input <OTA_APP.bin> --output <OTA_APP_v1.0.0.img> --version 1.0.0
   05_Tools\toolkit.bat ymodem python send <OTA_APP_v1.0.0.img> --port COM9 --baud 115200 --json
   ```

5. RTT 必须出现 `baseline PASS`，并记录 Slot A 版本、Metadata sequence、copy 和最终字段。
6. 恢复 `01_APP/system/app_system.c` 与 Keil 工程文件，确认正式工程不再引用本目录；再重新编译、烧录正式
   `OTA_APP`，进入 S07 OTA 验收。

该入口会写入 External Flash 和 EEPROM，只能在明确的空白设备或已确认可擦除测试设备上运行。
