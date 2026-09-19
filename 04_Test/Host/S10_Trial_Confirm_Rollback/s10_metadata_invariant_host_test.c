/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s10_metadata_invariant_host_test.c
 * @brief S10 Application 与 Bootloader Metadata 生命周期不变量 Host Test。
 * @author YaoQian Wang
 * @date 2026-09-19
 * @version V1.0
 * @note 本测试不进入 Keil 工程，不参与固件构建。
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "boot_crc32.h"
#include "boot_metadata.h"
#include "crc.h"
#include "firmware_metadata.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define TEST_METADATA_PENDING_OFFSET       (0x18U)
#define TEST_METADATA_UPGRADE_OFFSET       (0x19U)
#define TEST_METADATA_CRC_OFFSET           (0x78U)
#define TEST_METADATA_COMMIT_OFFSET        (0x7CU)
//******************************** Defines **********************************//

//******************************** Types ************************************//
typedef struct
{
    const char *name;
    uint8_t confirmedSlot;
    uint8_t pendingSlot;
    uint8_t slotAState;
    uint8_t slotBState;
    uint8_t upgradeState;
    uint8_t expectedValid;
} metadata_case_t;
//******************************** Types ************************************//

//******************************** Variables ********************************//
static const metadata_case_t g_cases[] = {
    {"valid pending", 0U, 1U, 1U, 1U, 1U, 1U},
    {"valid trial", 0U, 1U, 1U, 1U, 2U, 1U},
    {"valid rollback", 0U, 1U, 1U, 1U, 3U, 1U},
    {"pending equals confirmed", 0U, 0U, 1U, 1U, 1U, 0U},
    {"trial confirmed none", 0xFFU, 1U, 0U, 1U, 2U, 0U},
    {"rollback confirmed none", 0xFFU, 1U, 0U, 1U, 3U, 0U},
    {"trial confirmed not valid", 0U, 1U, 0U, 1U, 2U, 0U},
    {"rollback confirmed not valid", 0U, 1U, 2U, 1U, 3U, 0U},
    {"pending image not valid", 0U, 1U, 1U, 0U, 1U, 0U},
    {"rollback image not valid", 0U, 1U, 1U, 2U, 3U, 0U},
    {"factory stable none", 0xFFU, 0xFFU, 0U, 0U, 0U, 1U},
    {"stable known good", 0U, 0xFFU, 1U, 0U, 0U, 1U}
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

static void test_mark_committed(uint8_t raw[FIRMWARE_METADATA_COPY_SIZE])
{
    test_write_u32_le(&raw[TEST_METADATA_COMMIT_OFFSET], FIRMWARE_METADATA_COMMIT_MARKER);
}

static void test_build_application_metadata(
    const metadata_case_t *testCase,
    firmware_metadata_t *metadata)
{
    (void)memset(metadata, 0, sizeof(*metadata));
    metadata->sequence = 0x01020304UL;
    metadata->confirmedSlot = (firmware_slot_t)testCase->confirmedSlot;
    metadata->pendingSlot = (firmware_slot_t)testCase->pendingSlot;
    metadata->slotAState = (firmware_slot_state_t)testCase->slotAState;
    metadata->slotBState = (firmware_slot_state_t)testCase->slotBState;
    metadata->upgradeState = (firmware_upgrade_state_t)testCase->upgradeState;
}

static void test_build_boot_metadata(
    const metadata_case_t *testCase,
    boot_firmware_metadata_t *metadata)
{
    (void)memset(metadata, 0, sizeof(*metadata));
    metadata->sequence = 0x01020304UL;
    metadata->confirmedSlot = (boot_firmware_slot_t)testCase->confirmedSlot;
    metadata->pendingSlot = (boot_firmware_slot_t)testCase->pendingSlot;
    metadata->slotAState = (boot_firmware_slot_state_t)testCase->slotAState;
    metadata->slotBState = (boot_firmware_slot_state_t)testCase->slotBState;
    metadata->upgradeState = (boot_upgrade_state_t)testCase->upgradeState;
}

static int test_application_and_bootloader_encode_contract(void)
{
    uint32_t index;

    for (index = 0U; index < (sizeof(g_cases) / sizeof(g_cases[0])); index++) {
        const metadata_case_t *testCase = &g_cases[index];
        firmware_metadata_t applicationMetadata;
        boot_firmware_metadata_t bootMetadata;
        uint8_t applicationRaw[FIRMWARE_METADATA_COPY_SIZE];
        uint8_t bootRaw[BOOT_METADATA_COPY_SIZE];
        platform_error_t applicationResult;
        boot_contract_status_t bootResult;

        test_build_application_metadata(testCase, &applicationMetadata);
        test_build_boot_metadata(testCase, &bootMetadata);
        applicationResult = firmware_metadata_encode_uncommitted(
            &applicationMetadata, applicationRaw);
        bootResult = boot_metadata_encode_uncommitted(&bootMetadata, bootRaw);

        if (testCase->expectedValid != 0U) {
            if ((applicationResult != PLATFORM_ERR_OK) ||
                (bootResult != BOOT_CONTRACT_OK) ||
                (memcmp(applicationRaw, bootRaw, sizeof(applicationRaw)) != 0)) {
                (void)printf("Metadata encode contract failed: %s.\n", testCase->name);
                return 1;
            }
        } else if ((applicationResult == PLATFORM_ERR_OK) ||
                   (bootResult == BOOT_CONTRACT_OK)) {
            (void)printf("Metadata invalid case accepted: %s.\n", testCase->name);
            return 1;
        }
    }

    return 0;
}

static int test_committed_decode_rejects_invalid_lifecycle(void)
{
    firmware_metadata_t applicationMetadata;
    firmware_metadata_t applicationDecoded;
    boot_firmware_metadata_t bootDecoded;
    uint8_t raw[FIRMWARE_METADATA_COPY_SIZE];
    uint32_t index;

    test_build_application_metadata(&g_cases[1], &applicationMetadata);
    if (firmware_metadata_encode_uncommitted(&applicationMetadata, raw) != PLATFORM_ERR_OK) {
        return 1;
    }

    for (index = 3U; index <= 9U; index++) {
        uint8_t invalidRaw[FIRMWARE_METADATA_COPY_SIZE];

        (void)memcpy(invalidRaw, raw, sizeof(invalidRaw));
        if (index == 3U) {
            invalidRaw[TEST_METADATA_PENDING_OFFSET] = 0U;
        } else if (index == 4U) {
            invalidRaw[0x0DU] = 0xFFU;
        } else if (index == 5U) {
            invalidRaw[0x0EU] = 0U;
        } else if (index == 6U) {
            invalidRaw[0x0EU] = 2U;
        } else if (index == 7U) {
            invalidRaw[0x0FU] = 0U;
        } else if (index == 8U) {
            invalidRaw[TEST_METADATA_UPGRADE_OFFSET] = 1U;
            invalidRaw[0x0DU] = 0xFFU;
        } else {
            invalidRaw[TEST_METADATA_UPGRADE_OFFSET] = 3U;
            invalidRaw[0x0FU] = 2U;
        }
        test_write_u32_le(
            &invalidRaw[TEST_METADATA_CRC_OFFSET],
            boot_crc32_calculate(invalidRaw, TEST_METADATA_CRC_OFFSET));
        test_mark_committed(invalidRaw);

        if ((firmware_metadata_decode_committed(invalidRaw, &applicationDecoded) == PLATFORM_ERR_OK) ||
            (boot_metadata_decode_committed(invalidRaw, &bootDecoded) == BOOT_CONTRACT_OK)) {
            (void)printf("Metadata invalid committed record accepted at case %lu.\n",
                          (unsigned long)index);
            return 1;
        }
    }

    return 0;
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
int main(void)
{
    if (test_application_and_bootloader_encode_contract() != 0) {
        return 1;
    }

    if (test_committed_decode_rejects_invalid_lifecycle() != 0) {
        return 1;
    }

    (void)printf("S10 Metadata invariant host test passed.\n");
    return 0;
}
//******************************** Functions ********************************//
