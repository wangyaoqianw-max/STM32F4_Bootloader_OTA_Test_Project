/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_firmware_def.h
 * @brief Bootloader 使用的 Firmware Image / Metadata 固定数据合同。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_FIRMWARE_DEF_H
#define BOOT_FIRMWARE_DEF_H

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_FIRMWARE_SLOT_A_BASE             (0x000000UL)
#define BOOT_FIRMWARE_SLOT_B_BASE             (0x080000UL)
#define BOOT_FIRMWARE_SLOT_SIZE_BYTES         (0x080000UL)
#define BOOT_FIRMWARE_HEADER_SECTOR_SIZE      (0x001000UL)
#define BOOT_FIRMWARE_PAYLOAD_OFFSET          (0x001000UL)
#define BOOT_FIRMWARE_PAYLOAD_CAPACITY        (0x07F000UL)
#define BOOT_FIRMWARE_HEADER_SIZE             (64U)
#define BOOT_FIRMWARE_IMAGE_MAGIC             (0x4D495746UL)
#define BOOT_FIRMWARE_FORMAT_VERSION          (1U)

#define BOOT_METADATA_COPY_SIZE               (128U)
#define BOOT_METADATA_COPY_A_ADDRESS          (0x00U)
#define BOOT_METADATA_COPY_B_ADDRESS          (0x80U)
#define BOOT_METADATA_MAGIC                   (0x444D5746UL)
#define BOOT_METADATA_FORMAT_VERSION_V1       (1U)
#define BOOT_METADATA_FORMAT_VERSION_V2       (2U)
#define BOOT_METADATA_COMMIT_MARKER           (0x54494D43UL)
#define BOOT_METADATA_INVALID_MARKER          (0xFFFFFFFFUL)
//******************************** Defines **********************************//

//******************************** Types ***********************************//
typedef enum
{
    BOOT_CONTRACT_OK = 0,
    BOOT_CONTRACT_ERR_NULL,
    BOOT_CONTRACT_ERR_INVALID,
    BOOT_CONTRACT_ERR_FORMAT,
    BOOT_CONTRACT_ERR_COMMIT,
    BOOT_CONTRACT_ERR_CRC,
    BOOT_CONTRACT_ERR_NOT_FOUND
} boot_contract_status_t;

typedef enum
{
    BOOT_FIRMWARE_SLOT_A = 0,
    BOOT_FIRMWARE_SLOT_B = 1,
    BOOT_FIRMWARE_SLOT_NONE = 0xFF
} boot_firmware_slot_t;

typedef enum
{
    BOOT_FIRMWARE_SLOT_STATE_EMPTY = 0,
    BOOT_FIRMWARE_SLOT_STATE_VALID,
    BOOT_FIRMWARE_SLOT_STATE_INVALID
} boot_firmware_slot_state_t;

typedef enum
{
    BOOT_UPGRADE_STATE_NONE = 0,
    BOOT_UPGRADE_STATE_PENDING,
    BOOT_UPGRADE_STATE_TRIAL,
    BOOT_UPGRADE_STATE_ROLLBACK
} boot_upgrade_state_t;

typedef struct
{
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    uint16_t reserved;
} boot_firmware_version_t;

typedef struct
{
    uint32_t magic;
    uint16_t formatVersion;
    uint16_t headerSize;
    boot_firmware_version_t version;
    uint32_t imageSize;
    uint32_t payloadCrc32;
    uint8_t reserved[36];
    uint32_t headerCrc32;
} boot_firmware_image_header_t;

typedef struct
{
    uint32_t sequence;
    boot_firmware_slot_t confirmedSlot;
    boot_firmware_slot_t pendingSlot;
    boot_firmware_slot_state_t slotAState;
    boot_firmware_slot_state_t slotBState;
    boot_upgrade_state_t upgradeState;
    boot_firmware_version_t confirmedVersion;
} boot_firmware_metadata_t;
//******************************** Types ***********************************//

#endif
