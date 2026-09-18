/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s09_contract_host_test.c
 * @brief S09 Bootloader 与 Application/PC 固定数据合同 Host Test。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 * @note 本测试不进入 Keil 工程，不参与固件构建。
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>
#include <string.h>

#include "boot_crc32.h"
#include "boot_image.h"
#include "boot_metadata.h"
#include "firmware_metadata.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define TEST_METADATA_COMMITTED_MARKER_OFFSET (0x7CU)
#define TEST_METADATA_CRC_OFFSET              (0x78U)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
/* Packer app_v1.1.img 的 64 Byte Header 固定向量。 */
static const uint8_t g_packedHeader[BOOT_FIRMWARE_HEADER_SIZE] = {
    0x46U, 0x57U, 0x49U, 0x4DU, 0x01U, 0x00U, 0x40U, 0x00U,
    0x01U, 0x00U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0xC4U, 0x3DU, 0x01U, 0x00U, 0x95U, 0xF8U, 0x8FU, 0x9BU,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0xCCU, 0x46U, 0x71U, 0x6EU
};

static const uint8_t g_metadataVector[BOOT_METADATA_COPY_SIZE] = {
    0x46U, 0x57U, 0x4DU, 0x44U, 0x02U, 0x00U, 0x80U, 0x00U,
    0x04U, 0x03U, 0x02U, 0x01U, 0x00U, 0x00U, 0x01U, 0x01U,
    0x01U, 0x00U, 0x02U, 0x00U, 0x03U, 0x00U, 0x00U, 0x00U,
    0x01U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0xE7U, 0xF3U, 0xB7U, 0x01U, 0x43U, 0x4DU, 0x49U, 0x54U
};
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static void test_write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static void test_mark_metadata_committed(uint8_t raw[BOOT_METADATA_COPY_SIZE])
{
    test_write_u32_le(
        &raw[TEST_METADATA_COMMITTED_MARKER_OFFSET],
        BOOT_METADATA_COMMIT_MARKER);
}

static void test_build_application_metadata(firmware_metadata_t *metadata)
{
    (void)memset(metadata, 0, sizeof(*metadata));
    metadata->sequence = 0x01020304UL;
    metadata->confirmedSlot = FIRMWARE_SLOT_A;
    metadata->pendingSlot = FIRMWARE_SLOT_B;
    metadata->slotAState = FIRMWARE_SLOT_STATE_VALID;
    metadata->slotBState = FIRMWARE_SLOT_STATE_VALID;
    metadata->upgradeState = FIRMWARE_UPGRADE_STATE_TRIAL;
    metadata->confirmedVersion.major = 1U;
    metadata->confirmedVersion.minor = 2U;
    metadata->confirmedVersion.patch = 3U;
}

static int test_crc_vector(void)
{
    static const uint8_t data[] = "123456789";
    boot_crc32_context_t context;

    boot_crc32_init(&context);
    boot_crc32_update(&context, data, 4U);
    boot_crc32_update(&context, &data[4], 5U);
    return (boot_crc32_finalize(&context) == 0xCBF43926UL) ? 0 : 1;
}

static int test_packer_header(void)
{
    boot_firmware_image_header_t header;
    uint8_t invalidHeader[BOOT_FIRMWARE_HEADER_SIZE];

    if ((boot_image_validate_header(g_packedHeader, &header) != BOOT_IMAGE_VALIDATION_VALID) ||
        (header.version.major != 1U) || (header.version.minor != 1U) ||
        (header.version.patch != 0U) || (header.imageSize != 81348U) ||
        (header.payloadCrc32 != 0x9B8FF895UL)) {
        return 1;
    }

    (void)memcpy(invalidHeader, g_packedHeader, sizeof(invalidHeader));
    invalidHeader[0x3CU] ^= 1U;
    return (boot_image_validate_header(invalidHeader, NULL) ==
            BOOT_IMAGE_VALIDATION_INVALID_HEADER_CRC) ? 0 : 1;
}

static int test_metadata_cross_compatibility(void)
{
    firmware_metadata_t applicationMetadata;
    firmware_metadata_t applicationDecoded;
    boot_firmware_metadata_t bootMetadata;
    boot_firmware_metadata_t bootDecoded;
    uint8_t applicationRaw[BOOT_METADATA_COPY_SIZE];
    uint8_t bootRaw[BOOT_METADATA_COPY_SIZE];
    uint8_t newerRaw[BOOT_METADATA_COPY_SIZE];
    boot_metadata_copy_id_t selectedCopy;

    if (boot_metadata_decode_committed(g_metadataVector, &bootMetadata) != BOOT_CONTRACT_OK) {
        return 1;
    }
    if ((bootMetadata.sequence != 0x01020304UL) ||
        (bootMetadata.confirmedSlot != BOOT_FIRMWARE_SLOT_A) ||
        (bootMetadata.pendingSlot != BOOT_FIRMWARE_SLOT_B) ||
        (bootMetadata.upgradeState != BOOT_UPGRADE_STATE_TRIAL) ||
        (bootMetadata.confirmedVersion.patch != 3U)) {
        return 1;
    }

    test_build_application_metadata(&applicationMetadata);
    if (firmware_metadata_encode_uncommitted(&applicationMetadata, applicationRaw) != PLATFORM_ERR_OK) {
        return 1;
    }
    test_mark_metadata_committed(applicationRaw);
    if ((memcmp(applicationRaw, g_metadataVector, sizeof(applicationRaw)) != 0) ||
        (boot_metadata_decode_committed(applicationRaw, &bootDecoded) != BOOT_CONTRACT_OK) ||
        (bootDecoded.pendingSlot != BOOT_FIRMWARE_SLOT_B)) {
        return 1;
    }

    bootMetadata.sequence++;
    if ((boot_metadata_encode_uncommitted(&bootMetadata, bootRaw) != BOOT_CONTRACT_OK) ||
        (boot_metadata_decode_committed(bootRaw, &bootDecoded) != BOOT_CONTRACT_ERR_COMMIT)) {
        return 1;
    }
    test_mark_metadata_committed(bootRaw);
    if ((firmware_metadata_decode_committed(bootRaw, &applicationDecoded) != PLATFORM_ERR_OK) ||
        (applicationDecoded.sequence != bootMetadata.sequence) ||
        (applicationDecoded.upgradeState != FIRMWARE_UPGRADE_STATE_TRIAL)) {
        return 1;
    }

    (void)memcpy(newerRaw, g_metadataVector, sizeof(newerRaw));
    test_write_u32_le(&newerRaw[0x08U], 0x01020305UL);
    test_write_u32_le(
        &newerRaw[TEST_METADATA_CRC_OFFSET],
        boot_crc32_calculate(newerRaw, TEST_METADATA_CRC_OFFSET));
    if ((boot_metadata_select_latest(g_metadataVector, newerRaw, &bootDecoded, &selectedCopy) !=
         BOOT_CONTRACT_OK) || (selectedCopy != BOOT_METADATA_COPY_B) ||
        (bootDecoded.sequence != 0x01020305UL)) {
        return 1;
    }

    return 0;
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
int main(void)
{
    if (test_crc_vector() != 0) {
        (void)printf("S09 CRC contract test failed.\n");
        return 1;
    }
    if (test_packer_header() != 0) {
        (void)printf("S09 Packer/Header compatibility test failed.\n");
        return 1;
    }
    if (test_metadata_cross_compatibility() != 0) {
        (void)printf("S09 Metadata compatibility test failed.\n");
        return 1;
    }

    (void)printf("S09 Bootloader contract host test passed.\n");
    return 0;
}
//******************************** Functions ********************************//
