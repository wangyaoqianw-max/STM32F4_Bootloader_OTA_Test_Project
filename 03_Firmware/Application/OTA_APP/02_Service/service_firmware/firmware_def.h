/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file firmware_def.h
 * @brief Firmware Image Storage V1 固定数据合同
 * @author YaoQian Wang
 * @date 2026-09-13
 * @version V1.0
 *
 *****************************************************************************/

#ifndef FIRMWARE_DEF_H
#define FIRMWARE_DEF_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define FIRMWARE_SLOT_A_BASE                 (0x000000UL)
#define FIRMWARE_SLOT_B_BASE                 (0x080000UL)
#define FIRMWARE_SLOT_SIZE_BYTES             (0x080000UL)
#define FIRMWARE_HEADER_SECTOR_SIZE          (0x001000UL)
#define FIRMWARE_PAYLOAD_OFFSET              (0x001000UL)
#define FIRMWARE_SLOT_PAYLOAD_CAPACITY       (0x07F000UL)
#define FIRMWARE_IMAGE_HEADER_SIZE           (64U)
#define FIRMWARE_IMAGE_MAGIC                 (0x4D495746UL)
#define FIRMWARE_IMAGE_FORMAT_VERSION        (1U)
//******************************** Defines **********************************//

//******************************** Types ***********************************//
typedef enum
{
    FIRMWARE_SLOT_A = 0,
    FIRMWARE_SLOT_B = 1,
    FIRMWARE_SLOT_NONE = 0xFF
} firmware_slot_t;

typedef enum
{
    FIRMWARE_SLOT_STATE_EMPTY = 0,
    FIRMWARE_SLOT_STATE_VALID,
    FIRMWARE_SLOT_STATE_INVALID
} firmware_slot_state_t;

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
} firmware_version_t;

/**
 * @brief Firmware Image Header V1 的内存表达
 * @note 本结构体仅表达已解码字段；持久化与传输必须使用 fixed-offset raw buffer 编解码。
 */
typedef struct
{
    /** 固定 Magic，V1 为 FIRMWARE_IMAGE_MAGIC。 */
    uint32_t magic;
    /** 固定格式版本，V1 为 FIRMWARE_IMAGE_FORMAT_VERSION。 */
    uint16_t formatVersion;
    /** 固定 Header 长度，V1 为 FIRMWARE_IMAGE_HEADER_SIZE。 */
    uint16_t headerSize;
    /** 固件版本字段。 */
    firmware_version_t version;
    /** 原始 Firmware Payload 的实际字节数，不包含 Header 或对齐填充。 */
    uint32_t imageSize;
    /** 精确覆盖 imageSize Byte Payload 的 CRC-32/ISO-HDLC。 */
    uint32_t payloadCrc32;
    /** V1 固定保留区；编码时必须全零。 */
    uint8_t reserved[36];
    /** 覆盖 Header raw byte 0x00~0x3B 的 CRC-32/ISO-HDLC。 */
    uint32_t headerCrc32;
} firmware_image_header_t;

typedef enum
{
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
//******************************** Types ***********************************//

#endif
