/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s04_firmware_format_host_test.c
 * @brief S04 Firmware Version 与 Image Header Host Test
 * @author YaoQian Wang
 * @date 2026-09-13
 * @version V1.0
 *
 *****************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crc.h"
#include "firmware_image.h"
#include "firmware_version.h"

#define TEST_PAYLOAD_CRC32                (0x89ABCDEFUL)
#define TEST_IMAGE_SIZE                   (0x00001234UL)

static void test_write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static int test_firmware_version(void)
{
    firmware_version_t version123 = {1U, 2U, 3U, 0U};
    firmware_version_t version122 = {1U, 2U, 2U, 0U};
    firmware_version_t version130 = {1U, 3U, 0U, 0U};
    firmware_version_t invalidVersion = {1U, 2U, 3U, 1U};

    if (firmware_version_compare(&version123, &version122) <= 0) {
        return 1;
    }

    if (firmware_version_compare(&version123, &version130) >= 0) {
        return 1;
    }

    if (firmware_version_compare(&version123, &version123) != 0) {
        return 1;
    }

    if (firmware_version_is_valid(&invalidVersion) != 0U) {
        return 1;
    }

    return 0;
}

static int test_header_fixed_offsets_and_validation(void)
{
    firmware_image_header_t header = {0};
    firmware_image_header_t decodedHeader = {0};
    firmware_image_validation_t validation;
    uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE];

    header.magic = FIRMWARE_IMAGE_MAGIC;
    header.formatVersion = FIRMWARE_IMAGE_FORMAT_VERSION;
    header.headerSize = FIRMWARE_IMAGE_HEADER_SIZE;
    header.version.major = 1U;
    header.version.minor = 2U;
    header.version.patch = 3U;
    header.imageSize = TEST_IMAGE_SIZE;
    header.payloadCrc32 = TEST_PAYLOAD_CRC32;

    if (firmware_image_encode_header(&header, rawHeader) != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((rawHeader[0x00U] != 0x46U) || (rawHeader[0x01U] != 0x57U) ||
        (rawHeader[0x02U] != 0x49U) || (rawHeader[0x03U] != 0x4DU) ||
        (rawHeader[0x04U] != 0x01U) || (rawHeader[0x06U] != 0x40U) ||
        (rawHeader[0x08U] != 0x01U) || (rawHeader[0x0AU] != 0x02U) ||
        (rawHeader[0x0CU] != 0x03U) || (rawHeader[0x10U] != 0x34U) ||
        (rawHeader[0x11U] != 0x12U) || (rawHeader[0x14U] != 0xEFU) ||
        (rawHeader[0x15U] != 0xCDU) || (rawHeader[0x16U] != 0xABU) ||
        (rawHeader[0x17U] != 0x89U)) {
        return 1;
    }

    validation = firmware_image_validate_header(rawHeader, &decodedHeader);
    if ((validation != FIRMWARE_IMAGE_VALIDATION_VALID) ||
        (decodedHeader.imageSize != TEST_IMAGE_SIZE) ||
        (decodedHeader.payloadCrc32 != TEST_PAYLOAD_CRC32)) {
        return 1;
    }

    rawHeader[0x18U] = 1U;
    validation = firmware_image_validate_header(rawHeader, &decodedHeader);
    if (validation != FIRMWARE_IMAGE_VALIDATION_INVALID_RESERVED) {
        return 1;
    }

    rawHeader[0x18U] = 0U;
    rawHeader[0x10U] ^= 1U;
    validation = firmware_image_validate_header(rawHeader, &decodedHeader);
    if (validation != FIRMWARE_IMAGE_VALIDATION_INVALID_HEADER_CRC) {
        return 1;
    }

    return 0;
}

static int test_empty_and_invalid_size(void)
{
    firmware_image_header_t decodedHeader = {0};
    firmware_image_validation_t validation;
    uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE];

    (void)memset(rawHeader, 0xFF, sizeof(rawHeader));
    validation = firmware_image_validate_header(rawHeader, &decodedHeader);
    if (validation != FIRMWARE_IMAGE_VALIDATION_EMPTY) {
        return 1;
    }

    (void)memset(rawHeader, 0, sizeof(rawHeader));
    rawHeader[0x00U] = 0x46U;
    rawHeader[0x01U] = 0x57U;
    rawHeader[0x02U] = 0x49U;
    rawHeader[0x03U] = 0x4DU;
    rawHeader[0x04U] = 1U;
    rawHeader[0x06U] = FIRMWARE_IMAGE_HEADER_SIZE;
    test_write_u32_le(&rawHeader[0x10U], 0U);
    test_write_u32_le(&rawHeader[0x3CU], crc32_iso_hdlc_calculate(rawHeader, 0x3CU));
    validation = firmware_image_validate_header(rawHeader, &decodedHeader);
    if (validation != FIRMWARE_IMAGE_VALIDATION_INVALID_SIZE) {
        return 1;
    }

    test_write_u32_le(&rawHeader[0x10U], FIRMWARE_SLOT_PAYLOAD_CAPACITY + 1UL);
    test_write_u32_le(&rawHeader[0x3CU], crc32_iso_hdlc_calculate(rawHeader, 0x3CU));
    validation = firmware_image_validate_header(rawHeader, &decodedHeader);
    return (validation == FIRMWARE_IMAGE_VALIDATION_INVALID_SIZE) ? 0 : 1;
}

int main(void)
{
    if (test_firmware_version() != 0) {
        (void)printf("Firmware Version test failed.\n");
        return 1;
    }

    if (test_header_fixed_offsets_and_validation() != 0) {
        (void)printf("Firmware Header fixed-offset or validation test failed.\n");
        return 1;
    }

    if (test_empty_and_invalid_size() != 0) {
        (void)printf("Firmware Header empty or invalid-size test failed.\n");
        return 1;
    }

    (void)printf("S04 firmware format host test passed.\n");
    return 0;
}
