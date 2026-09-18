/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s09_prevalidate_host_test.c
 * @brief S09 Candidate 破坏性门禁 Host Test。
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
#include "boot_metadata.h"
#include "boot_prevalidate.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define TEST_PAYLOAD_SIZE (8U)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
static uint8_t g_header[BOOT_FIRMWARE_HEADER_SIZE];
static uint8_t g_payload[TEST_PAYLOAD_SIZE];
static uint8_t g_metadataA[BOOT_METADATA_COPY_SIZE];
static uint8_t g_metadataB[BOOT_METADATA_COPY_SIZE];
static uint32_t g_externalReadCount;
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
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

static void test_commit_metadata(uint8_t raw[BOOT_METADATA_COPY_SIZE])
{
    test_write_u32_le(&raw[0x7CU], BOOT_METADATA_COMMIT_MARKER);
}

static void test_build_header(void)
{
    (void)memset(g_header, 0, sizeof(g_header));
    test_write_u32_le(&g_header[0x00U], BOOT_FIRMWARE_IMAGE_MAGIC);
    test_write_u16_le(&g_header[0x04U], BOOT_FIRMWARE_FORMAT_VERSION);
    test_write_u16_le(&g_header[0x06U], BOOT_FIRMWARE_HEADER_SIZE);
    test_write_u16_le(&g_header[0x08U], 1U);
    test_write_u16_le(&g_header[0x0AU], 1U);
    test_write_u16_le(&g_header[0x0CU], 1U);
    test_write_u32_le(&g_header[0x10U], TEST_PAYLOAD_SIZE);
    test_write_u32_le(&g_header[0x14U], boot_crc32_calculate(g_payload, TEST_PAYLOAD_SIZE));
    test_write_u32_le(&g_header[0x3CU], boot_crc32_calculate(g_header, 0x3CU));
}

static void test_build_metadata(uint8_t pending)
{
    boot_firmware_metadata_t metadata = {0};

    metadata.sequence = 1U;
    metadata.confirmedSlot = BOOT_FIRMWARE_SLOT_A;
    metadata.pendingSlot = (pending != 0U) ? BOOT_FIRMWARE_SLOT_A : BOOT_FIRMWARE_SLOT_NONE;
    metadata.slotAState = BOOT_FIRMWARE_SLOT_STATE_VALID;
    metadata.slotBState = BOOT_FIRMWARE_SLOT_STATE_EMPTY;
    metadata.upgradeState = (pending != 0U) ? BOOT_UPGRADE_STATE_PENDING : BOOT_UPGRADE_STATE_NONE;
    metadata.confirmedVersion.major = 1U;
    if (boot_metadata_encode_uncommitted(&metadata, g_metadataA) != BOOT_CONTRACT_OK) {
        (void)printf("metadata fixture encode failed.\n");
    }
    test_commit_metadata(g_metadataA);
    (void)memcpy(g_metadataB, g_metadataA, sizeof(g_metadataB));
}

static void test_reset_fixture(uint8_t pending)
{
    g_payload[0] = 0x00U;
    g_payload[1] = 0x10U;
    g_payload[2] = 0x00U;
    g_payload[3] = 0x20U;
    g_payload[4] = 0x01U;
    g_payload[5] = 0x01U;
    g_payload[6] = 0x01U;
    g_payload[7] = 0x08U;
    test_build_header();
    test_build_metadata(pending);
    g_externalReadCount = 0U;
}

static boot_prevalidate_context_t test_context(void)
{
    static boot_w25q64_t flash = {0};
    static boot_at24c02_t eeprom = {0};
    boot_prevalidate_context_t context = {&flash, &eeprom};

    return context;
}
//******************************** Private Functions ************************//

//******************************** Test Doubles ******************************//
boot_driver_status_t boot_w25q64_read(
    boot_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    uint32_t length)
{
    (void)flash;
    g_externalReadCount++;
    if ((address == BOOT_FIRMWARE_SLOT_A_BASE) &&
        (length == BOOT_FIRMWARE_HEADER_SIZE)) {
        (void)memcpy(data, g_header, length);
        return BOOT_DRIVER_OK;
    }
    if ((address == (BOOT_FIRMWARE_SLOT_A_BASE + BOOT_FIRMWARE_PAYLOAD_OFFSET)) &&
        (length <= TEST_PAYLOAD_SIZE)) {
        (void)memcpy(data, g_payload, length);
        return BOOT_DRIVER_OK;
    }

    return BOOT_DRIVER_ERR_RANGE;
}

boot_driver_status_t boot_at24c02_read(
    boot_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    uint16_t length)
{
    (void)eeprom;
    if (length != BOOT_METADATA_COPY_SIZE) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    if (address == BOOT_METADATA_COPY_A_ADDRESS) {
        (void)memcpy(data, g_metadataA, length);
        return BOOT_DRIVER_OK;
    }
    if (address == BOOT_METADATA_COPY_B_ADDRESS) {
        (void)memcpy(data, g_metadataB, length);
        return BOOT_DRIVER_OK;
    }

    return BOOT_DRIVER_ERR_RANGE;
}
//******************************** Test Doubles ******************************//

//******************************** Functions ********************************//
static int test_valid_candidate(void)
{
    boot_candidate_t candidate;
    boot_prevalidate_context_t context;

    test_reset_fixture(1U);
    context = test_context();
    return (boot_prevalidate_candidate(&context, &candidate) == BOOT_PREVALIDATE_VALID) &&
                   (candidate.candidateSlot == BOOT_FIRMWARE_SLOT_A) &&
                   (g_externalReadCount == 3U) ? 0 : 1;
}

static int test_header_crc_gate(void)
{
    boot_candidate_t candidate;
    boot_prevalidate_context_t context;

    test_reset_fixture(1U);
    g_header[0x3CU] ^= 1U;
    context = test_context();
    return (boot_prevalidate_candidate(&context, &candidate) ==
            BOOT_PREVALIDATE_HEADER_INVALID) ? 0 : 1;
}

static int test_payload_crc_gate(void)
{
    boot_candidate_t candidate;
    boot_prevalidate_context_t context;

    test_reset_fixture(1U);
    g_payload[5] ^= 1U;
    context = test_context();
    return (boot_prevalidate_candidate(&context, &candidate) ==
            BOOT_PREVALIDATE_PAYLOAD_CRC_INVALID) ? 0 : 1;
}

static int test_vector_gate(void)
{
    boot_candidate_t candidate;
    boot_prevalidate_context_t context;

    test_reset_fixture(1U);
    g_payload[4] = 0x00U;
    test_build_header();
    context = test_context();
    return (boot_prevalidate_candidate(&context, &candidate) ==
            BOOT_PREVALIDATE_VECTOR_INVALID) ? 0 : 1;
}

static int test_no_pending_gate(void)
{
    boot_candidate_t candidate;
    boot_prevalidate_context_t context;

    test_reset_fixture(0U);
    context = test_context();
    return (boot_prevalidate_candidate(&context, &candidate) ==
            BOOT_PREVALIDATE_NO_PENDING) && (g_externalReadCount == 0U) ? 0 : 1;
}

int main(void)
{
    if ((test_valid_candidate() != 0) ||
        (test_header_crc_gate() != 0) ||
        (test_payload_crc_gate() != 0) ||
        (test_vector_gate() != 0) ||
        (test_no_pending_gate() != 0)) {
        (void)printf("S09 prevalidation host test failed.\n");
        return 1;
    }

    (void)printf("S09 prevalidation host test passed; no destructive API is linked.\n");
    return 0;
}
//******************************** Functions ********************************//
