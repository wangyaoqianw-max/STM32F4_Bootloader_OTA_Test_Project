/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_image.h
 * @brief Bootloader Firmware Image Header V1 解析接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_IMAGE_H
#define BOOT_IMAGE_H

//******************************** Includes *********************************//
#include "boot_firmware_def.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
/**
 * @brief Header V1 固定字段和 CRC 的验证结果。
 */
typedef enum
{
    BOOT_IMAGE_VALIDATION_UNKNOWN = 0,
    BOOT_IMAGE_VALIDATION_EMPTY,
    BOOT_IMAGE_VALIDATION_VALID,
    BOOT_IMAGE_VALIDATION_INVALID_MAGIC,
    BOOT_IMAGE_VALIDATION_INVALID_FORMAT,
    BOOT_IMAGE_VALIDATION_INVALID_HEADER_SIZE,
    BOOT_IMAGE_VALIDATION_INVALID_RESERVED,
    BOOT_IMAGE_VALIDATION_INVALID_VERSION,
    BOOT_IMAGE_VALIDATION_INVALID_SIZE,
    BOOT_IMAGE_VALIDATION_INVALID_HEADER_CRC
} boot_image_validation_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
/**
 * @brief 从固定 64 Byte little-endian Buffer 解码 Header。
 * @param[in] rawHeader : 固定长度 Header Buffer；不得为空。
 * @param[out] header : 解码输出；不得为空。
 * @return Bootloader 合同状态；本函数只解码，不判定字段合法性。
 */
boot_contract_status_t boot_image_decode_header(
    const uint8_t rawHeader[BOOT_FIRMWARE_HEADER_SIZE],
    boot_firmware_image_header_t *header);
/**
 * @brief 按 Header V1 合同验证已读取的 Header。
 * @param[in] rawHeader : 固定长度 Header Buffer；不得为空。
 * @param[out] decodedHeader : 可选解码输出；为空时只返回验证结果。
 * @return Header 验证结果；不验证 Payload 内容本身。
 */
boot_image_validation_t boot_image_validate_header(
    const uint8_t rawHeader[BOOT_FIRMWARE_HEADER_SIZE],
    boot_firmware_image_header_t *decodedHeader);
//******************************** Functions ********************************//

#endif
