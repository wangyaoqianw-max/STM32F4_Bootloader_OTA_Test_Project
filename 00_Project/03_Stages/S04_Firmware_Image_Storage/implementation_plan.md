# S04 Firmware Image Storage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 Firmware Image / A-B Slot / CRC / Metadata 双副本基础存储能力，并通过 PC 打包工具、Host Test、UART test-only 注入和 RTT + EasyLogger 真实板测证明 S04 数据契约可用。

**Architecture:** Application 新增 `service_firmware`，把 Firmware Version、Header、Metadata 格式与存储编排从 W25Q64 / AT24C02 Raw Driver 中分离；通用 CRC 放入 `service_common/crc`。格式模块只处理固定二进制合同和纯算法，`firmware_storage` 才依赖 W25Q64 / AT24C02；S04 板测固定向 Slot B 注入 `[64 Byte Header][Payload]`，Payload 先写、CRC 通过后最后提交 Header。

**Tech Stack:** STM32F411CEU6, C99, Keil MDK, STM32 HAL underneath existing Platform/Impl, W25Q64JV, AT24C02, existing `service_uart`, SEGGER RTT, EasyLogger, Python 3, host GCC.

**Spec:** `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`

## Metadata

- Stage: `S04_Firmware_Image_Storage`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Status: `READY_FOR_IMPLEMENTATION` after this plan is accepted and status documents are synchronized.

## Global Constraints

- Slot A 固定为 `0x000000 ~ 0x07FFFF`，Slot B 固定为 `0x080000 ~ 0x0FFFFF`，每个 Slot 512 KiB。
- 每个 Slot 的 Header Sector 固定为前 4 KiB；Payload 固定从 `slot_base + 0x1000` 开始，不能使用 `header_size` 推导 Payload 地址。
- Header V1 固定 64 Byte、little-endian、Magic `0x4D495746` (`FWIM`)、Format Version 1、Header Size 64。
- Metadata V1 固定双副本：Copy A `0x00~0x7F`，Copy B `0x80~0xFF`，每份 128 Byte。
- Metadata 采用 `sequence + CRC32 + commit_marker`；目标副本必须最后提交 `CMIT`。
- CRC 固定实现 CRC-8/SMBUS、CRC-16/XMODEM、CRC-32/ISO-HDLC；提供 one-shot 与 streaming API；V1 使用软件 bitwise 实现。
- S04 Slot State 仅有 `EMPTY / VALID / INVALID`，不得提前引入 `PENDING / TRIAL / CONFIRMED / ROLLBACK`。
- Image Header 是 Version / Size / Payload CRC 的镜像权威来源；EEPROM Metadata 是 active / confirmed / slot state / confirmed_version 的系统状态权威来源。
- Image Invalid 与 Validation I/O Failure 必须区分；I/O Error 不得自动把 Slot 标为 INVALID。
- 当前只有 Application 一个消费者，S04 不提前把单一消费者代码放入 `03_Firmware/Shared`。
- S04 可以复用现有 `service_uart` 做 test-only raw binary injection，但不得实现 Ymodem、重传协议或正式 OTA Service。
- S04 板测固定目标 Slot B，不增加 Slot 选择串口命令协议。
- UART 中断、data loss、Payload CRC mismatch 时不得写 Header；Header 只有在完整 Payload 验证通过后才最后提交。
- 修改任何项目自研 C 文件前必须读取 `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`；修改 Keil 工程前必须读取 `03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md`。
- 每个任务结束执行 `git diff --check`，能 Host Test 的任务必须先完成 Host Test，再执行 Keil build；不得用编译成功替代真实板测。

---

## Planned File Structure

```text
03_Firmware/Application/OTA_APP/02_Service/
├─ service_common/
│  └─ crc/
│     ├─ crc.h
│     └─ crc.c
│
└─ service_firmware/
   ├─ firmware_def.h
   ├─ firmware_version.h
   ├─ firmware_version.c
   ├─ firmware_image.h
   ├─ firmware_image.c
   ├─ firmware_metadata.h
   ├─ firmware_metadata.c
   ├─ firmware_storage.h
   └─ firmware_storage.c

04_Test/Host/S04_Firmware_Image_Storage/
├─ README.md
├─ s04_crc_host_test.c
├─ s04_firmware_format_host_test.c
├─ s04_firmware_storage_host_test.c
└─ stubs/
   ├─ platform_w25q64.h
   └─ platform_at24c02.h

04_Test/Board/S04_Firmware_Image_Storage/
├─ app_s04_firmware_image_test.h
└─ app_s04_firmware_image_test.c

05_Tools/Firmware/
├─ pack_firmware.py
└─ test_pack_firmware.py
```

`04_Test/Host` 和 `04_Test/Board` 不进入正式产品交付逻辑；Board Test 在验收完成后必须从正常 Application 启动路径与正式 Keil target 中退出。

---

### Task 1: Add CRC common module with standard test vectors

**Files:**
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_common/crc/crc.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_common/crc/crc.c`
- Create: `04_Test/Host/S04_Firmware_Image_Storage/s04_crc_host_test.c`
- Create: `04_Test/Host/S04_Firmware_Image_Storage/README.md`

**Interfaces:**

```c
typedef struct { uint8_t value; } crc8_smbus_context_t;
typedef struct { uint16_t value; } crc16_xmodem_context_t;
typedef struct { uint32_t value; } crc32_iso_hdlc_context_t;

void crc8_smbus_init(crc8_smbus_context_t *context);
void crc8_smbus_update(crc8_smbus_context_t *context,
                       const uint8_t *data,
                       uint32_t length);
uint8_t crc8_smbus_finalize(const crc8_smbus_context_t *context);
uint8_t crc8_smbus_calculate(const uint8_t *data, uint32_t length);

void crc16_xmodem_init(crc16_xmodem_context_t *context);
void crc16_xmodem_update(crc16_xmodem_context_t *context,
                         const uint8_t *data,
                         uint32_t length);
uint16_t crc16_xmodem_finalize(const crc16_xmodem_context_t *context);
uint16_t crc16_xmodem_calculate(const uint8_t *data, uint32_t length);

void crc32_iso_hdlc_init(crc32_iso_hdlc_context_t *context);
void crc32_iso_hdlc_update(crc32_iso_hdlc_context_t *context,
                           const uint8_t *data,
                           uint32_t length);
uint32_t crc32_iso_hdlc_finalize(const crc32_iso_hdlc_context_t *context);
uint32_t crc32_iso_hdlc_calculate(const uint8_t *data, uint32_t length);
```

- [ ] **Step 1: 编写 Host Test，先验证标准字符串 `"123456789"` 的固定结果。**

```text
CRC-8/SMBUS     = 0xF4
CRC-16/XMODEM   = 0x31C3
CRC-32/ISO-HDLC = 0xCBF43926
```

同时验证一次性计算与 `3 + 2 + 4` 分块 streaming 得到完全相同的结果。

- [ ] **Step 2: 编译 Host Test，确认在实现缺失时失败。**

```powershell
gcc -std=c99 -Wall -Wextra -Werror `
  -I03_Firmware/Application/OTA_APP/02_Service/service_common/crc `
  -o "$env:TEMP\s04_crc_host_test.exe" `
  04_Test/Host/S04_Firmware_Image_Storage/s04_crc_host_test.c `
  03_Firmware/Application/OTA_APP/02_Service/service_common/crc/crc.c
```

Expected before implementation: compile/link fails because CRC module is absent or incomplete.

- [ ] **Step 3: 实现三个固定 CRC 变体的 bitwise algorithm。**
  - CRC-8/SMBUS: poly `0x07`, init `0x00`, refin/refout false, xorout `0x00`。
  - CRC-16/XMODEM: poly `0x1021`, init `0x0000`, refin/refout false, xorout `0x0000`。
  - CRC-32/ISO-HDLC: reflected implementation poly `0xEDB88320`, init `0xFFFFFFFF`, final xor `0xFFFFFFFF`。
  - `update()` 必须允许多次连续调用；`finalize()` 不修改 context。

- [ ] **Step 4: 运行 Host Test。**

```powershell
& "$env:TEMP\s04_crc_host_test.exe"
```

Expected: all one-shot and streaming vectors PASS, exit code 0.

- [ ] **Step 5: 执行 `git diff --check` 并提交。**

Suggested commit: `feat: add common crc algorithms`

---

### Task 2: Add Firmware Version and Image Header V1 format

**Files:**
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_def.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_version.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_version.c`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_image.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_image.c`
- Create: `04_Test/Host/S04_Firmware_Image_Storage/s04_firmware_format_host_test.c`

**Interfaces:**

```c
#define FIRMWARE_SLOT_A_BASE             (0x000000UL)
#define FIRMWARE_SLOT_B_BASE             (0x080000UL)
#define FIRMWARE_SLOT_SIZE_BYTES         (0x080000UL)
#define FIRMWARE_HEADER_SECTOR_SIZE      (0x001000UL)
#define FIRMWARE_PAYLOAD_OFFSET          (0x001000UL)
#define FIRMWARE_SLOT_PAYLOAD_CAPACITY   (0x07F000UL)
#define FIRMWARE_IMAGE_HEADER_SIZE       (64U)
#define FIRMWARE_IMAGE_MAGIC             (0x4D495746UL)
#define FIRMWARE_IMAGE_FORMAT_VERSION    (1U)

typedef enum {
    FIRMWARE_SLOT_A = 0,
    FIRMWARE_SLOT_B = 1,
    FIRMWARE_SLOT_NONE = 0xFF
} firmware_slot_t;

typedef enum {
    FIRMWARE_SLOT_STATE_EMPTY = 0,
    FIRMWARE_SLOT_STATE_VALID,
    FIRMWARE_SLOT_STATE_INVALID
} firmware_slot_state_t;

typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    uint16_t reserved;
} firmware_version_t;

typedef struct {
    uint32_t magic;
    uint16_t formatVersion;
    uint16_t headerSize;
    firmware_version_t version;
    uint32_t imageSize;
    uint32_t payloadCrc32;
    uint8_t reserved[36];
    uint32_t headerCrc32;
} firmware_image_header_t;

typedef enum {
    FIRMWARE_IMAGE_VALIDATION_UNKNOWN = 0,
    FIRMWARE_IMAGE_VALIDATION_EMPTY,
    FIRMWARE_IMAGE_VALIDATION_VALID,
    FIRMWARE_IMAGE_VALIDATION_INVALID_MAGIC,
    FIRMWARE_IMAGE_VALIDATION_INVALID_FORMAT_VERSION,
    FIRMWARE_IMAGE_VALIDATION_INVALID_HEADER_SIZE,
    FIRMWARE_IMAGE_VALIDATION_INVALID_RESERVED,
    FIRMWARE_IMAGE_VALIDATION_INVALID_VERSION,
    FIRMWARE_IMAGE_VALIDATION_INVALID_SIZE,
    FIRMWARE_IMAGE_VALIDATION_INVALID_HEADER_CRC,
    FIRMWARE_IMAGE_VALIDATION_INVALID_PAYLOAD_CRC
} firmware_image_validation_t;

int32_t firmware_version_compare(const firmware_version_t *left,
                                 const firmware_version_t *right);
platform_bool_t firmware_version_is_valid(const firmware_version_t *version);

platform_error_t firmware_image_decode_header(
    const uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE],
    firmware_image_header_t *header);
platform_error_t firmware_image_encode_header(
    const firmware_image_header_t *header,
    uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE]);
firmware_image_validation_t firmware_image_validate_header(
    const uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE],
    firmware_image_header_t *decodedHeader);
```

- [ ] **Step 1: 先写 Host Test，覆盖固定 offset / little-endian / version compare / invalid version / image size / Header CRC。**
  - 构造版本 `1.2.3`，验证 `1.2.3 > 1.2.2`、`1.2.3 < 1.3.0`、相等返回 0。
  - `version.reserved != 0` → invalid。
  - `image_size = 0` 与 `image_size > 0x07F000` → `INVALID_SIZE`。
  - Header 前 64 Byte 全 `0xFF` → `EMPTY`。
  - 正常 Header 修改 `0x18` reserved 中任意 1 Byte → `INVALID_RESERVED` 或 CRC mismatch；测试顺序必须与设计中的 validation order 一致。
  - 修改 Header 已覆盖字段但保留旧 CRC → `INVALID_HEADER_CRC`。

- [ ] **Step 2: 实现 `firmware_def.h` 固定常量和类型，不把 Slot 地址放进 `project_config.h`。**
  - Slot Layout 属于 Firmware Binary Contract，而不是板级可调参数。

- [ ] **Step 3: 实现 Version compare / validation。**

- [ ] **Step 4: 实现 Header fixed-offset encode / decode。**
  - 明确按 byte 写入/读取 `uint16_t`、`uint32_t` little-endian；不得 `memcpy(struct)` 作为协议实现。
  - `encode_header()` 先把 V1 reserved 填 0，再计算前 60 Byte CRC32，最后写入 offset `0x3C`。

- [ ] **Step 5: 实现 `firmware_image_validate_header()`。**
  - 先检查全 `0xFF`；再按 `magic → format_version → header_size → reserved → version → image_size → header_crc` 顺序返回明确 validation result。

- [ ] **Step 6: 编译并运行 Host Test。**

```powershell
gcc -std=c99 -Wall -Wextra -Werror `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_common `
  -I03_Firmware/Application/OTA_APP/04_Impl/impl_board `
  -I03_Firmware/Application/OTA_APP/02_Service/service_common/crc `
  -I03_Firmware/Application/OTA_APP/02_Service/service_firmware `
  -o "$env:TEMP\s04_firmware_format_host_test.exe" `
  04_Test/Host/S04_Firmware_Image_Storage/s04_firmware_format_host_test.c `
  03_Firmware/Application/OTA_APP/02_Service/service_common/crc/crc.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_version.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_image.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.c

& "$env:TEMP\s04_firmware_format_host_test.exe"
```

Expected: all format tests PASS, exit code 0.

- [ ] **Step 7: `git diff --check`，提交本任务。**

Suggested commit: `feat: add firmware image format`

---

### Task 3: Add Metadata V1 codec, validation and double-copy selection

**Files:**
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.c`
- Extend: `04_Test/Host/S04_Firmware_Image_Storage/s04_firmware_format_host_test.c`

**Interfaces:**

```c
#define FIRMWARE_METADATA_COPY_SIZE       (128U)
#define FIRMWARE_METADATA_COPY_A_ADDRESS  (0x00U)
#define FIRMWARE_METADATA_COPY_B_ADDRESS  (0x80U)
#define FIRMWARE_METADATA_MAGIC           (0x444D5746UL)
#define FIRMWARE_METADATA_FORMAT_VERSION  (1U)
#define FIRMWARE_METADATA_COMMIT_MARKER   (0x54494D43UL)
#define FIRMWARE_METADATA_INVALID_MARKER  (0xFFFFFFFFUL)

typedef enum {
    FIRMWARE_METADATA_COPY_NONE = 0,
    FIRMWARE_METADATA_COPY_A,
    FIRMWARE_METADATA_COPY_B
} firmware_metadata_copy_id_t;

typedef struct {
    uint32_t sequence;
    firmware_slot_t activeSlot;
    firmware_slot_t confirmedSlot;
    firmware_slot_state_t slotAState;
    firmware_slot_state_t slotBState;
    firmware_version_t confirmedVersion;
} firmware_metadata_t;

platform_error_t firmware_metadata_encode_uncommitted(
    const firmware_metadata_t *metadata,
    uint8_t raw[FIRMWARE_METADATA_COPY_SIZE]);
platform_error_t firmware_metadata_decode_committed(
    const uint8_t raw[FIRMWARE_METADATA_COPY_SIZE],
    firmware_metadata_t *metadata);
platform_bool_t firmware_metadata_sequence_is_newer(uint32_t candidate,
                                                    uint32_t reference);
platform_error_t firmware_metadata_select_latest(
    const uint8_t copyA[FIRMWARE_METADATA_COPY_SIZE],
    const uint8_t copyB[FIRMWARE_METADATA_COPY_SIZE],
    firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *selectedCopy);
```

- [ ] **Step 1: 写失败 Host Test，覆盖 Metadata fixed offsets、CRC、commit marker 和 sequence wrap-around。**
  - Copy A committed valid / B invalid → A。
  - A invalid / B committed valid → B。
  - A sequence 10 / B sequence 11 → B。
  - A sequence `0xFFFFFFFF` / B sequence `0` → B 视为更新。
  - 两份 CRC invalid → 返回 `PLATFORM_ERR_NOT_FOUND` 或项目已有最匹配错误码；不得伪造默认 Metadata。
  - `active_slot / confirmed_slot / slot_state / confirmed_version` 越界或非法 → Copy invalid。
  - commit marker != `CMIT` → Copy invalid，即使 CRC 正确。

- [ ] **Step 2: 实现固定 128 Byte raw layout encode。**
  - `0x00~0x77` 构造 Body；
  - CRC32 写 `0x78~0x7B`；
  - `0x7C~0x7F` 初始写 `FIRMWARE_METADATA_INVALID_MARKER`；
  - V1 reserved `0x18~0x77` 全部写 0。

- [ ] **Step 3: 实现 committed copy validation / decode。**
  - 必须先验证 magic / format / size / commit marker / field range / CRC，再输出 decoded metadata。

- [ ] **Step 4: 实现 sequence wrap-around 比较和双副本选择。**
  - 用 modulo-32-bit 规则判断 candidate 是否较新；相同 sequence 时必须采用确定性规则并记录在注释中，建议优先 Copy A，且 Host Test 固定该行为。

- [ ] **Step 5: 运行更新后的 Host Test。**

Expected: Header/Version/Metadata 全部 PASS。

- [ ] **Step 6: `git diff --check`，提交。**

Suggested commit: `feat: add firmware metadata contract`

---

### Task 4: Add Firmware Storage service and host-stubbed storage tests

**Files:**
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_storage.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_storage.c`
- Create: `04_Test/Host/S04_Firmware_Image_Storage/s04_firmware_storage_host_test.c`
- Create: `04_Test/Host/S04_Firmware_Image_Storage/stubs/platform_w25q64.h`
- Create: `04_Test/Host/S04_Firmware_Image_Storage/stubs/platform_at24c02.h`

**Interfaces:**

```c
typedef struct {
    platform_w25q64_t *flash;
    platform_at24c02_t *eeprom;
    platform_bool_t initialized;
} firmware_storage_t;

#define FIRMWARE_STORAGE_INITIALIZER {0}

platform_error_t firmware_storage_init(
    firmware_storage_t *storage,
    platform_w25q64_t *flash,
    platform_at24c02_t *eeprom);

platform_error_t firmware_storage_read_header(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation);

platform_error_t firmware_storage_validate_image(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation);

platform_error_t firmware_storage_load_metadata(
    firmware_storage_t *storage,
    firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *sourceCopy);

platform_error_t firmware_storage_commit_metadata(
    firmware_storage_t *storage,
    const firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *committedCopy);

platform_error_t firmware_storage_erase_slot(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadSize);
```

**Storage behavior:**
- `read_header()` 只读 64 Byte 并返回 Header validation，不修改 EEPROM。
- `validate_image()` Header 合法后按固定小 Buffer 分块读取 Payload，使用 streaming CRC32；建议 Buffer 256 Byte，与 W25Q64 Page Size 对齐但不依赖一次性大 RAM。
- `erase_slot(slot, payloadSize)` 只擦 Header Sector 和 `ceil(payloadSize / 4096)` 个 Payload Sector，必须先完成所有范围计算，再执行第一笔 erase。
- `load_metadata()` 读取 EEPROM 0x00/0x80 两份 raw copy，再调用 pure metadata module 选择。
- `commit_metadata()` 读取当前有效 copy，目标选择另一份；先 invalid marker，再 Body + CRC，read-back 验证，最后单独写 `CMIT` 并再次 read-back。

- [ ] **Step 1: 编写 W25Q64 / AT24C02 memory stubs 和 Host Test。**
  - Flash stub 使用至少覆盖 Slot B Header + 测试 Payload 的 host buffer，不需要模拟完整 8 MiB；地址转换必须拒绝越界。
  - EEPROM stub 使用 256 Byte buffer。
  - 支持注入 read/write/erase error，用于验证 I/O Error 与 Image Invalid 分离。

- [ ] **Step 2: 写失败用例。**
  - valid Header + Payload → `PLATFORM_ERR_OK` + `VALID`。
  - Header all `0xFF` → `PLATFORM_ERR_OK` + `EMPTY`。
  - Payload 1 Byte corruption → `PLATFORM_ERR_OK` + `INVALID_PAYLOAD_CRC`。
  - Flash read injected error → 函数返回 I/O error，validation 保持 `UNKNOWN`。
  - invalid `imageSize` → 不发生 Payload read。
  - Metadata A valid/B invalid、A invalid/B valid、A/B valid newer sequence。
  - Metadata commit target 的 marker 写入前模拟掉电：旧 copy 仍可 load。
  - Metadata commit 完成：新 copy sequence 增加并成为 load 结果。

- [ ] **Step 3: 实现 `firmware_storage_init/read_header/validate_image/erase_slot`。**

- [ ] **Step 4: 实现 `load_metadata/commit_metadata`。**
  - `commit_metadata()` 不允许调用者直接控制写 A/B；它根据当前有效 copy 选择目标。
  - 两份都无效时，首次 commit 固定写 Copy A，sequence 使用调用者给定的初始化 sequence（建议 0）。
  - 已有有效 Metadata 时，新提交 sequence 必须由 storage 层基于当前值 `+1`，不要信任调用者手工自增。

- [ ] **Step 5: 编译和运行 Host Test。**

```powershell
gcc -std=c99 -Wall -Wextra -Werror `
  -I04_Test/Host/S04_Firmware_Image_Storage/stubs `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_common `
  -I03_Firmware/Application/OTA_APP/02_Service/service_common/crc `
  -I03_Firmware/Application/OTA_APP/02_Service/service_firmware `
  -o "$env:TEMP\s04_firmware_storage_host_test.exe" `
  04_Test/Host/S04_Firmware_Image_Storage/s04_firmware_storage_host_test.c `
  03_Firmware/Application/OTA_APP/02_Service/service_common/crc/crc.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_version.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_image.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_storage.c

& "$env:TEMP\s04_firmware_storage_host_test.exe"
```

Expected: all storage / I/O error / metadata recovery tests PASS, exit code 0.

- [ ] **Step 6: `git diff --check`，提交。**

Suggested commit: `feat: add firmware storage service`

---

### Task 5: Add PC firmware pack tool and binary-contract tests

**Files:**
- Create: `05_Tools/Firmware/pack_firmware.py`
- Create: `05_Tools/Firmware/test_pack_firmware.py`
- Modify: `04_Test/Host/S04_Firmware_Image_Storage/s04_firmware_format_host_test.c` to optionally validate a generated `.img` file when a path argument is supplied.

**CLI:**

```text
python 05_Tools/Firmware/pack_firmware.py \
  --input <app.bin> \
  --output <firmware.img> \
  --version <major.minor.patch>
```

Output binary:

```text
[64 Byte Header][raw APP.bin Payload]
```

- [ ] **Step 1: 写 Python unit tests。**
  - 3 Byte payload `01 02 03`；
  - output size = 64 + 3；
  - Header magic / format / header size / version / image size offsets 正确；
  - Payload 从 file offset 64 开始，不插入 4 KiB padding；
  - Header CRC32 覆盖前 60 Byte；
  - Payload CRC32 与 Python `zlib.crc32()` 一致；
  - malformed version、空 payload、payload > 508 KiB 必须拒绝且不生成有效输出。

- [ ] **Step 2: 实现 CLI。**
  - 使用 `struct.pack_into('<I', ...)` / `'<H'` 等 fixed-offset little-endian 写法；不得依赖 Python object serialization。
  - `zlib.crc32()` 结果 `& 0xFFFFFFFF`，对应 CRC-32/ISO-HDLC。

- [ ] **Step 3: 运行 Python tests。**

```powershell
python -m unittest 05_Tools/Firmware/test_pack_firmware.py -v
```

Expected: PASS.

- [ ] **Step 4: 生成一个小型 `.img`，交给 C Host Test 解码。**

```powershell
python 05_Tools/Firmware/pack_firmware.py `
  --input 04_Test/Host/S04_Firmware_Image_Storage/test_payload.bin `
  --output "$env:TEMP\s04_test.img" `
  --version 1.1.0

& "$env:TEMP\s04_firmware_format_host_test.exe" "$env:TEMP\s04_test.img"
```

Host Test 预期验证：Header 合法、Version=1.1.0、Image Size 与输入一致、Header CRC / Payload CRC 均匹配。

测试 payload 可由 test 脚本临时生成，不需要长期提交二进制 fixture。

- [ ] **Step 5: `git diff --check`，提交。**

Suggested commit: `feat: add firmware image pack tool`

---

### Task 6: Integrate S04 production modules into Keil project

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`
- Modify only if needed for compile-time board-test switch: `03_Firmware/Application/OTA_APP/00_Config/project_config.h`

**Interfaces:**
- Adds `crc.c`, `firmware_version.c`, `firmware_image.c`, `firmware_metadata.c`, `firmware_storage.c` to the Keil target.
- Adds include paths for `service_common/crc` and `service_firmware`.
- Does not yet add S04 Board Test source to the final target.

- [ ] **Step 1: 读取 Keil 工程规范并审计当前 `.uvprojx` group/include-path 组织。**
- [ ] **Step 2: 只加入 S04 production sources/includes，不改 `.uvoptx`。**
- [ ] **Step 3: 执行仓库统一 build。**

```bat
05_Tools\Scripts\build_app.bat
```

Expected: no new errors; existing warnings must be classified as pre-existing or new.

- [ ] **Step 4: 执行一次 Keil Clean + Rebuild。**
  - 使用现有 Keil UI 或等价稳定命令完成 Clean/Rebuild；不得修改 `build_app.bat` 伪装成 clean。
  - Expected: all S04 production source compiles from clean state.

- [ ] **Step 5: `git diff --check`，提交。**

Suggested commit: `build: integrate S04 firmware modules`

---

### Task 7: Add isolated S04 UART-to-Slot-B board test

**Files:**
- Create: `04_Test/Board/S04_Firmware_Image_Storage/app_s04_firmware_image_test.h`
- Create: `04_Test/Board/S04_Firmware_Image_Storage/app_s04_firmware_image_test.c`
- Temporarily modify: `03_Firmware/Application/OTA_APP/01_APP/app_main.c`
- Temporarily modify: `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`
- Modify if a test gate is used: `03_Firmware/Application/OTA_APP/00_Config/project_config.h`

**Consumes existing APIs:**
- `platform_bsp_spi_construct_storage_bus()` + SPI lifecycle；
- existing W25Q64 constructor/init path used by S02 board test；
- `platform_bsp_gpio_construct_soft_i2c_scl()` / `platform_bsp_gpio_construct_soft_i2c_sda()`；
- `platform_i2c_init()` + `platform_at24c02_init()`；
- `platform_bsp_uart_construct_communication()`；
- Platform UART lifecycle；
- `platform_thread_get_current()`；
- `service_uart_init/start/wait_event/read/get_status()`；
- `firmware_storage_*`；
- RTT + EasyLogger through `service_log`。

**Board-test entry:**

```c
platform_error_t app_s04_firmware_image_test_run(void);
```

- [ ] **Step 1: 构造 S04 所需 W25Q64、Software I2C/AT24C02、communication UART 和 `service_uart`。**
  - Board Test 在 `appSystem` Task context 执行，使用 `platform_thread_get_current()` 获取 owner thread 传给 `service_uart_config_t.ownerThread`，不暴露 `app_system.c` 的 static thread object。
  - DMA RX Buffer 与 RingBuffer Storage 使用固定静态数组，不动态分配。

- [ ] **Step 2: 实现固定 Slot B 的 Header receive state。**
  - 累计读取恰好 64 Byte Header；允许一次 `service_uart_read()` 返回任意分片长度。
  - Header 满 64 Byte 后调用 `firmware_image_validate_header()`；Header 不合法立即 abort，不执行 erase。
  - 日志打印 Version、Image Size、Expected Payload CRC32。

- [ ] **Step 3: Header 合法后擦除 Slot B 所需区域。**
  - 调用 `firmware_storage_erase_slot(SLOT_B, imageSize)`；必须包含 Header Sector 和实际 Payload 所需 sector。

- [ ] **Step 4: 实现 Payload receive / write loop。**
  - 从 `service_uart` 读取 chunk；
  - 写入 `0x081000 + received_offset`；
  - 同时 `crc32_iso_hdlc_update()`；
  - `received_offset` 绝不能超过 Header `imageSize`；
  - 检查 `service_uart_get_status().dataLossOccurred`、ERROR / STOPPED state；任一异常 abort。
  - 不使用 silence timeout 作为“文件正常结束”；正常结束唯一条件是 `received_offset == imageSize`。

- [ ] **Step 5: Payload 接收完整后比较 streaming CRC32。**
  - mismatch → log expected/actual，abort，Header 不写。
  - match → 将最初收到并验证的 64 Byte Header 写到 Slot B base `0x080000`，这是唯一 Image Commit 动作。

- [ ] **Step 6: Header commit 后调用 `firmware_storage_validate_image(SLOT_B)` 从 W25Q64 完整重读验证。**
  - Board Test 必须证明写入时 CRC 和重读 CRC 均通过。

- [ ] **Step 7: 验证 Metadata load / first commit / second commit / copy selection。**
  - 首次无有效 Metadata：建立基础 Metadata（Slot B VALID，其余字段按测试初始化语义设置）并 commit。
  - 再次 commit 触发交替副本与 sequence 增长。
  - 读取并日志输出 selected Copy / sequence。
  - destructive copy corruption 测试必须只改 Metadata 测试数据，且日志说明正在执行破坏性验证。

- [ ] **Step 8: 临时把 Board Test 接入 `app_main.c`，执行 Keil build。**
  - 建议用显式 compile-time gate，例如 `PROJECT_ENABLE_S04_BOARD_TEST`，测试期 = 1，最终 cleanup = 0/移除。
  - 正常 LED Blink 路径不能与 destructive board test 同时运行。

- [ ] **Step 9: PC 生成实际测试镜像，串口助手 raw binary send，收集 RTT。**
  - 固定目标 Slot B。
  - 日志至少包含 erase、Header、Version、Size、Expected/Calculated CRC、receive progress、Header commit、full re-validation、Metadata copy/sequence、final result。

- [ ] **Step 10: 人工执行负面场景。**
  1. 正常 `.img` → VALID；
  2. 修改 Payload 1 Byte → `INVALID_PAYLOAD_CRC`；
  3. 修改 Header 1 Byte → Header validation failure；
  4. Header Sector erased → EMPTY；
  5. invalid image size → reject before erase；
  6. 发送过程中停止 → Header never committed, Slot B reports EMPTY；
  7. 制造 UART data loss/error → abort, Header never committed；
  8. Metadata 单副本损坏 → 另一份恢复；
  9. Reset 后 Metadata 可恢复；
  10. 真正 power-cycle 后 Metadata 可恢复。

- [ ] **Step 11: 提交 board-test implementation，并把真实板测待执行项写入 handoff。**

Suggested commit: `test: add S04 firmware image board test`

---

### Task 8: Remove destructive test from production path and prepare verification handoff

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/01_APP/app_main.c`
- Modify: `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`
- Modify if used: `03_Firmware/Application/OTA_APP/00_Config/project_config.h`
- Retain: `04_Test/Board/S04_Firmware_Image_Storage/app_s04_firmware_image_test.h/.c`
- Modify: `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
- Modify: `PROJECT_CONTEXT.md`
- Modify: `00_Project/05_Status/current_status.md`

- [ ] **Step 1: 板测完成后从 production `app_main()` 移除 S04 destructive test invocation。**
- [ ] **Step 2: 从正式 Keil target 移除 `04_Test/Board/S04_Firmware_Image_Storage/*.c`；测试源码保留在 `04_Test/Board`。**
- [ ] **Step 3: 执行全部 Host Test。**
  - CRC test PASS；
  - Firmware format/metadata test PASS；
  - Firmware storage stub test PASS；
  - Python pack tool tests PASS；
  - Python-generated image passes C decoder compatibility test。

- [ ] **Step 4: 执行最终 normal build + Clean/Rebuild。**

```bat
05_Tools\Scripts\build_app.bat
```

Expected: production Application 不编译 S04 board-test source；S04 production modules保留并可链接。

- [ ] **Step 5: 更新 `handoff.md` Implementation Output。**
  - 记录每个 Task commit；
  - changed files；
  - Host Test 结果；
  - build / clean rebuild；
  - user-provided RTT hardware evidence；
  - Reset / Power-cycle evidence；
  - deviations；
  - known issues。

- [ ] **Step 6: Coding Standard Review。**

记录：

```text
Coding Standard Review: PASS / NEEDS_FIX / EXCEPTION
```

任何 `EXCEPTION` 必须写文件、规则、工程理由和后续处理。

- [ ] **Step 7: Stage 推进到 `READY_FOR_VERIFICATION`，停止 Implementation Role。**
  - Implementation Role 不自行创建 PASS Verification 结论，也不自行关闭 S04。

Suggested commit: `chore: prepare S04 verification handoff`

---

## Final Verification Matrix

Verification Role 后续必须独立核对：

```text
CRC-8/SMBUS standard vector                PASS
CRC-16/XMODEM standard vector              PASS
CRC-32/ISO-HDLC standard vector            PASS
CRC one-shot == streaming                  PASS
Firmware Version compare                   PASS
Header fixed offsets / little-endian       PASS
Header CRC                                 PASS
Payload CRC                                PASS
PC pack tool ↔ C decoder compatibility     PASS
Slot A/B address boundaries                PASS
Slot B erased Header → EMPTY               PASS
Valid image → VALID                        PASS
Payload corruption → INVALID_PAYLOAD_CRC   PASS
Header corruption → invalid                PASS
Validation I/O error → UNKNOWN             PASS
Metadata A/B selection                     PASS
Metadata sequence wrap-around              PASS
Metadata commit-marker recovery            PASS
Metadata single-copy corruption recovery   PASS
UART interrupted transfer → no Header      PASS
UART data-loss/error → no Header           PASS
Reset persistence                          PASS
Power-cycle persistence                    PASS
Keil normal build                          PASS
Keil clean/rebuild                         PASS
RTT + EasyLogger evidence                  PASS
Board test removed from production path    PASS
Scope boundary / no Ymodem                 PASS
```

Verification evidence belongs in:

```text
04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md
```

## Completion Condition

Implementation 完成条件：

1. Task 1~8 的生产代码、Host Test、PC Tool、Board Test 均完成并提交；
2. Host Test 与 Python tests 全部通过；
3. Keil normal build 和 clean/rebuild 通过；
4. 用户在真实 STM32F411 + W25Q64 + AT24C02 上完成 S04 RTT 板测并提供证据；
5. destructive S04 Board Test 已退出 production runtime 和正式 Keil target；
6. `handoff.md` 已记录所有实现提交和验证输入；
7. Stage 状态仅推进到 `READY_FOR_VERIFICATION`。

S04 只有在独立 Verification 和 Review 都 PASS 后才允许进入 `CLOSED`。
