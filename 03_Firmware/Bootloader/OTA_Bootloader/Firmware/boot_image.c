/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_image.c
 * @brief Bootloader Firmware Image Header V1 固定格式实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "boot_crc32.h"
#include "boot_image.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_IMAGE_OFFSET_MAGIC             (0x00U)
#define BOOT_IMAGE_OFFSET_FORMAT             (0x04U)
#define BOOT_IMAGE_OFFSET_HEADER_SIZE        (0x06U)
#define BOOT_IMAGE_OFFSET_VERSION_MAJOR     (0x08U)
#define BOOT_IMAGE_OFFSET_VERSION_MINOR     (0x0AU)
#define BOOT_IMAGE_OFFSET_VERSION_PATCH     (0x0CU)
#define BOOT_IMAGE_OFFSET_VERSION_RESERVED  (0x0EU)
#define BOOT_IMAGE_OFFSET_IMAGE_SIZE        (0x10U)
#define BOOT_IMAGE_OFFSET_PAYLOAD_CRC       (0x14U)
#define BOOT_IMAGE_OFFSET_RESERVED           (0x18U)
#define BOOT_IMAGE_RESERVED_SIZE             (36U)
#define BOOT_IMAGE_OFFSET_HEADER_CRC         (0x3CU)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static uint16_t boot_image_read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

static uint32_t boot_image_read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

static uint8_t boot_image_is_erased(const uint8_t *rawHeader)
{
    uint32_t index;

    for (index = 0U; index < BOOT_FIRMWARE_HEADER_SIZE; index++) {
        if (rawHeader[index] != 0xFFU) {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t boot_image_reserved_is_zero(const uint8_t *rawHeader)
{
    uint32_t index;

    for (index = 0U; index < BOOT_IMAGE_RESERVED_SIZE; index++) {
        if (rawHeader[BOOT_IMAGE_OFFSET_RESERVED + index] != 0U) {
            return 0U;
        }
    }

    return 1U;
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
boot_contract_status_t boot_image_decode_header(
    const uint8_t rawHeader[BOOT_FIRMWARE_HEADER_SIZE],
    boot_firmware_image_header_t *header)
{
    uint32_t index;

    if ((rawHeader == NULL) || (header == NULL)) {
        return BOOT_CONTRACT_ERR_NULL;
    }

    header->magic = boot_image_read_u32_le(&rawHeader[BOOT_IMAGE_OFFSET_MAGIC]);
    header->formatVersion = boot_image_read_u16_le(&rawHeader[BOOT_IMAGE_OFFSET_FORMAT]);
    header->headerSize = boot_image_read_u16_le(&rawHeader[BOOT_IMAGE_OFFSET_HEADER_SIZE]);
    header->version.major = boot_image_read_u16_le(&rawHeader[BOOT_IMAGE_OFFSET_VERSION_MAJOR]);
    header->version.minor = boot_image_read_u16_le(&rawHeader[BOOT_IMAGE_OFFSET_VERSION_MINOR]);
    header->version.patch = boot_image_read_u16_le(&rawHeader[BOOT_IMAGE_OFFSET_VERSION_PATCH]);
    header->version.reserved = boot_image_read_u16_le(&rawHeader[BOOT_IMAGE_OFFSET_VERSION_RESERVED]);
    header->imageSize = boot_image_read_u32_le(&rawHeader[BOOT_IMAGE_OFFSET_IMAGE_SIZE]);
    header->payloadCrc32 = boot_image_read_u32_le(&rawHeader[BOOT_IMAGE_OFFSET_PAYLOAD_CRC]);
    for (index = 0U; index < BOOT_IMAGE_RESERVED_SIZE; index++) {
        header->reserved[index] = rawHeader[BOOT_IMAGE_OFFSET_RESERVED + index];
    }
    header->headerCrc32 = boot_image_read_u32_le(&rawHeader[BOOT_IMAGE_OFFSET_HEADER_CRC]);

    return BOOT_CONTRACT_OK;
}

boot_image_validation_t boot_image_validate_header(
    const uint8_t rawHeader[BOOT_FIRMWARE_HEADER_SIZE],
    boot_firmware_image_header_t *decodedHeader)
{
    boot_firmware_image_header_t header;
    uint32_t headerCrc32;

    if (rawHeader == NULL) {
        return BOOT_IMAGE_VALIDATION_UNKNOWN;
    }
    if (boot_image_is_erased(rawHeader) != 0U) {
        return BOOT_IMAGE_VALIDATION_EMPTY;
    }
    if (boot_image_decode_header(rawHeader, &header) != BOOT_CONTRACT_OK) {
        return BOOT_IMAGE_VALIDATION_UNKNOWN;
    }
    if (header.magic != BOOT_FIRMWARE_IMAGE_MAGIC) {
        return BOOT_IMAGE_VALIDATION_INVALID_MAGIC;
    }
    if (header.formatVersion != BOOT_FIRMWARE_FORMAT_VERSION) {
        return BOOT_IMAGE_VALIDATION_INVALID_FORMAT;
    }
    if (header.headerSize != BOOT_FIRMWARE_HEADER_SIZE) {
        return BOOT_IMAGE_VALIDATION_INVALID_HEADER_SIZE;
    }
    if (boot_image_reserved_is_zero(rawHeader) == 0U) {
        return BOOT_IMAGE_VALIDATION_INVALID_RESERVED;
    }
    if (header.version.reserved != 0U) {
        return BOOT_IMAGE_VALIDATION_INVALID_VERSION;
    }
    if ((header.imageSize == 0U) ||
        (header.imageSize > BOOT_FIRMWARE_PAYLOAD_CAPACITY)) {
        return BOOT_IMAGE_VALIDATION_INVALID_SIZE;
    }
    headerCrc32 = boot_crc32_calculate(rawHeader, BOOT_IMAGE_OFFSET_HEADER_CRC);
    if (header.headerCrc32 != headerCrc32) {
        return BOOT_IMAGE_VALIDATION_INVALID_HEADER_CRC;
    }
    if (decodedHeader != NULL) {
        *decodedHeader = header;
    }

    return BOOT_IMAGE_VALIDATION_VALID;
}
