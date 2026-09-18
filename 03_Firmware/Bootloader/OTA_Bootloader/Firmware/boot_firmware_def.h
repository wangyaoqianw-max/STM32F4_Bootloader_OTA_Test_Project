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
/**
 * @brief Bootloader 固定格式解析和设备访问共用的状态码。
 */
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

/**
 * @brief 外部 Firmware Slot 标识。
 */
typedef enum
{
    BOOT_FIRMWARE_SLOT_A = 0,
    BOOT_FIRMWARE_SLOT_B = 1,
    BOOT_FIRMWARE_SLOT_NONE = 0xFF
} boot_firmware_slot_t;

/**
 * @brief 外部 Slot 的基础镜像状态。
 */
typedef enum
{
    BOOT_FIRMWARE_SLOT_STATE_EMPTY = 0,
    BOOT_FIRMWARE_SLOT_STATE_VALID,
    BOOT_FIRMWARE_SLOT_STATE_INVALID
} boot_firmware_slot_state_t;

/**
 * @brief S07/S09 Metadata 升级生命周期值。
 */
typedef enum
{
    BOOT_UPGRADE_STATE_NONE = 0,
    BOOT_UPGRADE_STATE_PENDING,
    BOOT_UPGRADE_STATE_TRIAL,
    BOOT_UPGRADE_STATE_ROLLBACK
} boot_upgrade_state_t;

/**
 * @brief Firmware Version V1 的固定字段表达。
 */
typedef struct
{
    /** 固件主版本号。 */
    uint16_t major;
    /** 固件次版本号。 */
    uint16_t minor;
    /** 固件修订版本号。 */
    uint16_t patch;
    /** V1 保留字段，必须为 0。 */
    uint16_t reserved;
} boot_firmware_version_t;

/**
 * @brief Firmware Image Header V1 的解码结果。
 * @note 持久化数据仍使用固定 little-endian raw buffer，不使用结构体布局。
 */
typedef struct
{
    /** 固定 Firmware Image Magic。 */
    uint32_t magic;
    /** 固定格式版本。 */
    uint16_t formatVersion;
    /** 固定 Header 字节数。 */
    uint16_t headerSize;
    /** Firmware Version 字段。 */
    boot_firmware_version_t version;
    /** Payload 实际字节数，不含 Header。 */
    uint32_t imageSize;
    /** Payload CRC-32/ISO-HDLC。 */
    uint32_t payloadCrc32;
    /** Header V1 保留区；当前版本要求全零。 */
    uint8_t reserved[36];
    /** 覆盖 Header raw 0x00~0x3B 的 CRC-32。 */
    uint32_t headerCrc32;
} boot_firmware_image_header_t;

/**
 * @brief Metadata V2 的解码结果。
 * @note 不依赖 Application Platform Object；持久化使用固定偏移 raw buffer。
 */
typedef struct
{
    /** 双副本提交序列号。 */
    uint32_t sequence;
    /** 已确认 Slot。 */
    boot_firmware_slot_t confirmedSlot;
    /** Bootloader 下一次需要安装的 Slot。 */
    boot_firmware_slot_t pendingSlot;
    /** Slot A 的基础镜像状态。 */
    boot_firmware_slot_state_t slotAState;
    /** Slot B 的基础镜像状态。 */
    boot_firmware_slot_state_t slotBState;
    /** 跨 Reset 保存的升级生命周期状态。 */
    boot_upgrade_state_t upgradeState;
    /** 已确认版本基准；S09 不提前更新。 */
    boot_firmware_version_t confirmedVersion;
} boot_firmware_metadata_t;
//******************************** Types ***********************************//

#endif
