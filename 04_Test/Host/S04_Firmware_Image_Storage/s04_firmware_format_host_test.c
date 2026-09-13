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
#include "firmware_metadata.h"
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

static void test_write_committed_marker(uint8_t raw[FIRMWARE_METADATA_COPY_SIZE])
{
    test_write_u32_le(&raw[0x7CU], FIRMWARE_METADATA_COMMIT_MARKER);
}

static int test_metadata_copy_selection(void)
{
    firmware_metadata_t metadata = {0};
    firmware_metadata_t decodedMetadata = {0};
    firmware_metadata_copy_id_t selectedCopy;
    uint8_t copyA[FIRMWARE_METADATA_COPY_SIZE];
    uint8_t copyB[FIRMWARE_METADATA_COPY_SIZE];

    metadata.sequence = 10UL;
    metadata.activeSlot = FIRMWARE_SLOT_B;
    metadata.confirmedSlot = FIRMWARE_SLOT_A;
    metadata.slotAState = FIRMWARE_SLOT_STATE_VALID;
    metadata.slotBState = FIRMWARE_SLOT_STATE_VALID;
    metadata.confirmedVersion.major = 1U;
    metadata.confirmedVersion.minor = 2U;
    metadata.confirmedVersion.patch = 3U;

    if (firmware_metadata_encode_uncommitted(&metadata, copyA) != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((copyA[0x00U] != 0x46U) || (copyA[0x01U] != 0x57U) ||
        (copyA[0x02U] != 0x4DU) || (copyA[0x03U] != 0x44U) ||
        (copyA[0x0CU] != FIRMWARE_SLOT_B) || (copyA[0x0DU] != FIRMWARE_SLOT_A) ||
        (copyA[0x7CU] != 0xFFU)) {
        return 1;
    }

    test_write_committed_marker(copyA);
    (void)memset(copyB, 0xFF, sizeof(copyB));
    if (firmware_metadata_select_latest(copyA, copyB, &decodedMetadata, &selectedCopy) != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((selectedCopy != FIRMWARE_METADATA_COPY_A) || (decodedMetadata.sequence != 10UL)) {
        return 1;
    }

    metadata.sequence = 11UL;
    if (firmware_metadata_encode_uncommitted(&metadata, copyB) != PLATFORM_ERR_OK) {
        return 1;
    }

    test_write_committed_marker(copyB);
    if (firmware_metadata_select_latest(copyA, copyB, &decodedMetadata, &selectedCopy) != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((selectedCopy != FIRMWARE_METADATA_COPY_B) || (decodedMetadata.sequence != 11UL)) {
        return 1;
    }

    return 0;
}

static int test_metadata_wrap_and_invalid_copy(void)
{
    firmware_metadata_t metadata = {0};
    firmware_metadata_t decodedMetadata = {0};
    firmware_metadata_copy_id_t selectedCopy;
    uint8_t copyA[FIRMWARE_METADATA_COPY_SIZE];
    uint8_t copyB[FIRMWARE_METADATA_COPY_SIZE];

    metadata.activeSlot = FIRMWARE_SLOT_NONE;
    metadata.confirmedSlot = FIRMWARE_SLOT_NONE;
    metadata.slotAState = FIRMWARE_SLOT_STATE_EMPTY;
    metadata.slotBState = FIRMWARE_SLOT_STATE_EMPTY;

    metadata.sequence = 0xFFFFFFFFUL;
    if (firmware_metadata_encode_uncommitted(&metadata, copyA) != PLATFORM_ERR_OK) {
        return 1;
    }

    test_write_committed_marker(copyA);
    metadata.sequence = 0UL;
    if (firmware_metadata_encode_uncommitted(&metadata, copyB) != PLATFORM_ERR_OK) {
        return 1;
    }

    test_write_committed_marker(copyB);
    if ((firmware_metadata_sequence_is_newer(0UL, 0xFFFFFFFFUL) == 0U) ||
        (firmware_metadata_select_latest(copyA, copyB, &decodedMetadata, &selectedCopy) != PLATFORM_ERR_OK) ||
        (selectedCopy != FIRMWARE_METADATA_COPY_B)) {
        return 1;
    }

    copyB[0x18U] = 1U;
    test_write_u32_le(&copyB[0x78U], crc32_iso_hdlc_calculate(copyB, 0x78U));
    if (firmware_metadata_decode_committed(copyB, &decodedMetadata) == PLATFORM_ERR_OK) {
        return 1;
    }

    copyA[0x78U] ^= 1U;
    copyB[0x7CU] = 0U;
    if (firmware_metadata_select_latest(copyA, copyB, &decodedMetadata, &selectedCopy) != PLATFORM_ERR_NOT_FOUND) {
        return 1;
    }

    return 0;
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

    if (test_metadata_copy_selection() != 0) {
        (void)printf("Firmware Metadata copy selection test failed.\n");
        return 1;
    }

    if (test_metadata_wrap_and_invalid_copy() != 0) {
        (void)printf("Firmware Metadata wrap or invalid-copy test failed.\n");
        return 1;
    }

    (void)printf("S04 firmware format host test passed.\n");
    return 0;
}
