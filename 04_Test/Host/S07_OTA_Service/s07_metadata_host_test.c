/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s07_metadata_host_test.c
 * @brief S07 Firmware Metadata V1/V2 Host Test
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crc.h"
#include "firmware_metadata.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define TEST_METADATA_CRC_OFFSET          (0x78U)
#define TEST_METADATA_COMMIT_OFFSET       (0x7CU)
//******************************** Defines **********************************//

//******************************** Functions *********************************//
static void test_write_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void test_write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static uint16_t test_read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

static void test_mark_committed(uint8_t raw[FIRMWARE_METADATA_COPY_SIZE])
{
    test_write_u32_le(&raw[TEST_METADATA_COMMIT_OFFSET], FIRMWARE_METADATA_COMMIT_MARKER);
}

static void test_build_v1_record(
    uint8_t raw[FIRMWARE_METADATA_COPY_SIZE],
    uint32_t sequence,
    firmware_slot_t legacyActiveSlot,
    firmware_slot_t confirmedSlot,
    firmware_slot_state_t slotAState,
    firmware_slot_state_t slotBState)
{
    (void)memset(raw, 0, FIRMWARE_METADATA_COPY_SIZE);
    test_write_u32_le(&raw[0x00U], FIRMWARE_METADATA_MAGIC);
    test_write_u16_le(&raw[0x04U], FIRMWARE_METADATA_FORMAT_VERSION_V1);
    test_write_u16_le(&raw[0x06U], FIRMWARE_METADATA_COPY_SIZE);
    test_write_u32_le(&raw[0x08U], sequence);
    raw[0x0CU] = (uint8_t)legacyActiveSlot;
    raw[0x0DU] = (uint8_t)confirmedSlot;
    raw[0x0EU] = (uint8_t)slotAState;
    raw[0x0FU] = (uint8_t)slotBState;
    test_write_u16_le(&raw[0x10U], 1U);
    test_write_u16_le(&raw[0x12U], 0U);
    test_write_u16_le(&raw[0x14U], 0U);
    test_write_u16_le(&raw[0x16U], 0U);
    test_write_u32_le(&raw[TEST_METADATA_CRC_OFFSET],
                      crc32_iso_hdlc_calculate(raw, TEST_METADATA_CRC_OFFSET));
    test_mark_committed(raw);
}

static int test_v2_round_trip(void)
{
    firmware_metadata_t metadata = {0};
    firmware_metadata_t decodedMetadata = {0};
    uint8_t raw[FIRMWARE_METADATA_COPY_SIZE];

    metadata.sequence = 7U;
    metadata.confirmedSlot = FIRMWARE_SLOT_A;
    metadata.pendingSlot = FIRMWARE_SLOT_NONE;
    metadata.slotAState = FIRMWARE_SLOT_STATE_VALID;
    metadata.slotBState = FIRMWARE_SLOT_STATE_EMPTY;
    metadata.upgradeState = FIRMWARE_UPGRADE_STATE_NONE;
    metadata.confirmedVersion.major = 1U;

    if (firmware_metadata_encode_uncommitted(&metadata, raw) != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((test_read_u16_le(&raw[0x04U]) != FIRMWARE_METADATA_FORMAT_VERSION_V2) ||
        (raw[0x0CU] != 0U) ||
        (raw[0x0DU] != FIRMWARE_SLOT_A) ||
        (raw[0x18U] != FIRMWARE_SLOT_NONE) ||
        (raw[0x19U] != FIRMWARE_UPGRADE_STATE_NONE) ||
        (raw[TEST_METADATA_COMMIT_OFFSET] != 0xFFU)) {
        return 1;
    }

    test_mark_committed(raw);
    if ((firmware_metadata_decode_committed(raw, &decodedMetadata) != PLATFORM_ERR_OK) ||
        (decodedMetadata.sequence != metadata.sequence) ||
        (decodedMetadata.confirmedSlot != metadata.confirmedSlot) ||
        (decodedMetadata.pendingSlot != metadata.pendingSlot) ||
        (decodedMetadata.upgradeState != metadata.upgradeState)) {
        return 1;
    }

    return 0;
}

static int test_v1_backward_compatible_decode(void)
{
    firmware_metadata_t decodedMetadata = {0};
    uint8_t migratedRaw[FIRMWARE_METADATA_COPY_SIZE];
    uint8_t raw[FIRMWARE_METADATA_COPY_SIZE];

    test_build_v1_record(raw, 3U, FIRMWARE_SLOT_B, FIRMWARE_SLOT_A,
                         FIRMWARE_SLOT_STATE_VALID, FIRMWARE_SLOT_STATE_EMPTY);
    if (firmware_metadata_decode_committed(raw, &decodedMetadata) != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((decodedMetadata.sequence != 3U) ||
        (decodedMetadata.confirmedSlot != FIRMWARE_SLOT_A) ||
        (decodedMetadata.pendingSlot != FIRMWARE_SLOT_NONE) ||
        (decodedMetadata.upgradeState != FIRMWARE_UPGRADE_STATE_NONE)) {
        return 1;
    }

    if (firmware_metadata_encode_uncommitted(&decodedMetadata, migratedRaw) != PLATFORM_ERR_OK) {
        return 1;
    }

    return (test_read_u16_le(&migratedRaw[0x04U]) == FIRMWARE_METADATA_FORMAT_VERSION_V2) ? 0 : 1;
}

static int test_cross_field_validation(void)
{
    firmware_metadata_t metadata = {0};
    uint8_t raw[FIRMWARE_METADATA_COPY_SIZE];

    metadata.confirmedSlot = FIRMWARE_SLOT_A;
    metadata.pendingSlot = FIRMWARE_SLOT_B;
    metadata.slotAState = FIRMWARE_SLOT_STATE_VALID;
    metadata.slotBState = FIRMWARE_SLOT_STATE_VALID;
    metadata.upgradeState = FIRMWARE_UPGRADE_STATE_NONE;
    if (firmware_metadata_encode_uncommitted(&metadata, raw) == PLATFORM_ERR_OK) {
        return 1;
    }

    metadata.upgradeState = FIRMWARE_UPGRADE_STATE_PENDING;
    metadata.slotBState = FIRMWARE_SLOT_STATE_INVALID;
    if (firmware_metadata_encode_uncommitted(&metadata, raw) == PLATFORM_ERR_OK) {
        return 1;
    }

    metadata.slotBState = FIRMWARE_SLOT_STATE_VALID;
    return (firmware_metadata_encode_uncommitted(&metadata, raw) == PLATFORM_ERR_OK) ? 0 : 1;
}

static int test_reserved_and_crc_validation(void)
{
    firmware_metadata_t metadata = {0};
    firmware_metadata_t decodedMetadata = {0};
    uint8_t raw[FIRMWARE_METADATA_COPY_SIZE];

    metadata.confirmedSlot = FIRMWARE_SLOT_A;
    metadata.pendingSlot = FIRMWARE_SLOT_NONE;
    metadata.slotAState = FIRMWARE_SLOT_STATE_VALID;
    metadata.slotBState = FIRMWARE_SLOT_STATE_EMPTY;
    metadata.upgradeState = FIRMWARE_UPGRADE_STATE_NONE;
    if (firmware_metadata_encode_uncommitted(&metadata, raw) != PLATFORM_ERR_OK) {
        return 1;
    }

    test_mark_committed(raw);
    raw[0x1AU] = 1U;
    test_write_u32_le(&raw[TEST_METADATA_CRC_OFFSET],
                      crc32_iso_hdlc_calculate(raw, TEST_METADATA_CRC_OFFSET));
    if (firmware_metadata_decode_committed(raw, &decodedMetadata) == PLATFORM_ERR_OK) {
        return 1;
    }

    raw[0x1AU] = 0U;
    raw[TEST_METADATA_CRC_OFFSET] ^= 1U;
    return (firmware_metadata_decode_committed(raw, &decodedMetadata) == PLATFORM_ERR_CHECKSUM) ? 0 : 1;
}

int main(void)
{
    if (test_v2_round_trip() != 0) {
        (void)printf("S07 Metadata V2 round-trip test failed.\n");
        return 1;
    }

    if (test_v1_backward_compatible_decode() != 0) {
        (void)printf("S07 Metadata V1 compatibility test failed.\n");
        return 1;
    }

    if (test_cross_field_validation() != 0) {
        (void)printf("S07 Metadata cross-field validation test failed.\n");
        return 1;
    }

    if (test_reserved_and_crc_validation() != 0) {
        (void)printf("S07 Metadata reserved or CRC validation test failed.\n");
        return 1;
    }

    (void)printf("S07 Metadata host test passed.\n");
    return 0;
}
//******************************** Functions *********************************//
