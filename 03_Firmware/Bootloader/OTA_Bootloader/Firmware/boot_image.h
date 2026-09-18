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
boot_contract_status_t boot_image_decode_header(
    const uint8_t rawHeader[BOOT_FIRMWARE_HEADER_SIZE],
    boot_firmware_image_header_t *header);
boot_image_validation_t boot_image_validate_header(
    const uint8_t rawHeader[BOOT_FIRMWARE_HEADER_SIZE],
    boot_firmware_image_header_t *decodedHeader);
//******************************** Functions ********************************//

#endif
