/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s09_metadata_commit_host_test.c
 * @brief S09 Metadata PENDING 到 TRIAL 原子提交故障注入 Host Test。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 * @note 本测试不进入 Keil 工程，不参与固件构建。
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boot_metadata_commit.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define TEST_EEPROM_SIZE_BYTES (256U)
#define TEST_MARKER_OFFSET     (0x7CU)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
static uint8_t g_eepromMemory[TEST_EEPROM_SIZE_BYTES];
static uint8_t g_failCommitMarkerWrite;
static uint8_t g_writeAddresses[8U];
static uint32_t g_writeCount;
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static void test_write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static void test_expect(uint8_t condition, const char *message)
{
    if (condition == 0U) {
        (void)fprintf(stderr, "S09 metadata commit host test failed: %s\n", message);
        (void)fflush(stderr);
        exit(1);
    }
}

static void test_reset_memory(void)
{
    boot_firmware_metadata_t metadata;
    uint8_t raw[BOOT_METADATA_COPY_SIZE];

    (void)memset(g_eepromMemory, 0xFF, sizeof(g_eepromMemory));
    (void)memset(g_writeAddresses, 0, sizeof(g_writeAddresses));
    g_failCommitMarkerWrite = 0U;
    g_writeCount = 0U;

    (void)memset(&metadata, 0, sizeof(metadata));
    metadata.sequence = 10U;
    metadata.confirmedSlot = BOOT_FIRMWARE_SLOT_A;
    metadata.pendingSlot = BOOT_FIRMWARE_SLOT_B;
    metadata.slotAState = BOOT_FIRMWARE_SLOT_STATE_VALID;
    metadata.slotBState = BOOT_FIRMWARE_SLOT_STATE_VALID;
    metadata.upgradeState = BOOT_UPGRADE_STATE_PENDING;
    metadata.confirmedVersion.major = 1U;
    metadata.confirmedVersion.minor = 0U;
    metadata.confirmedVersion.patch = 0U;

    test_expect(boot_metadata_encode_uncommitted(&metadata, raw) == BOOT_CONTRACT_OK,
                "encode baseline metadata");
    test_write_u32_le(&raw[TEST_MARKER_OFFSET], BOOT_METADATA_COMMIT_MARKER);
    (void)memcpy(&g_eepromMemory[BOOT_METADATA_COPY_A_ADDRESS], raw,
                 BOOT_METADATA_COPY_SIZE);
    (void)memcpy(&g_eepromMemory[BOOT_METADATA_COPY_B_ADDRESS], raw,
                 BOOT_METADATA_COPY_SIZE);
}

static void test_expect_pending_baseline(void)
{
    boot_firmware_metadata_t metadata;
    boot_metadata_copy_id_t selectedCopy;
    boot_contract_status_t result;

    result = boot_metadata_select_latest(
        &g_eepromMemory[BOOT_METADATA_COPY_A_ADDRESS],
        &g_eepromMemory[BOOT_METADATA_COPY_B_ADDRESS],
        &metadata,
        &selectedCopy);
    test_expect(result == BOOT_CONTRACT_OK, "select baseline metadata");
    test_expect(selectedCopy == BOOT_METADATA_COPY_A, "baseline tie selects Copy A");
    test_expect(metadata.sequence == 10U, "baseline sequence is preserved");
    test_expect(metadata.upgradeState == BOOT_UPGRADE_STATE_PENDING,
                "failed commit keeps PENDING");
}

static void test_marker_write_order(void)
{
    test_expect(g_writeCount >= 3U, "metadata commit writes all phases");
    test_expect(g_writeAddresses[0] == BOOT_METADATA_COPY_B_ADDRESS + TEST_MARKER_OFFSET,
                "invalid marker is written first to target Copy B");
    test_expect(g_writeAddresses[1] == BOOT_METADATA_COPY_B_ADDRESS,
                "body and CRC are written after invalid marker");
    test_expect(g_writeAddresses[2] == BOOT_METADATA_COPY_B_ADDRESS + TEST_MARKER_OFFSET,
                "commit marker is attempted last");
}

static void test_commit_marker_failure(void)
{
    boot_at24c02_t eeprom = {0};
    boot_firmware_metadata_t committed;
    boot_metadata_commit_result_t result;

    test_reset_memory();
    g_failCommitMarkerWrite = 1U;
    result = boot_metadata_commit_trial(&eeprom, BOOT_FIRMWARE_SLOT_B, &committed);
    test_expect(result == BOOT_METADATA_COMMIT_WRITE_FAILED,
                "commit marker failure is reported");
    test_marker_write_order();
    test_expect_pending_baseline();
}

static void test_successful_commit(void)
{
    boot_at24c02_t eeprom = {0};
    boot_firmware_metadata_t committed;
    boot_metadata_commit_result_t result;

    test_reset_memory();
    result = boot_metadata_commit_trial(&eeprom, BOOT_FIRMWARE_SLOT_B, &committed);
    test_expect(result == BOOT_METADATA_COMMIT_OK, "successful trial commit");
    test_expect(committed.sequence == 11U, "sequence increments once");
    test_expect(committed.upgradeState == BOOT_UPGRADE_STATE_TRIAL,
                "successful commit enters TRIAL");
    test_expect(committed.confirmedSlot == BOOT_FIRMWARE_SLOT_A,
                "confirmed slot is preserved");
    test_expect(committed.pendingSlot == BOOT_FIRMWARE_SLOT_B,
                "pending slot is preserved");
    test_expect(committed.confirmedVersion.major == 1U &&
                    committed.confirmedVersion.minor == 0U &&
                    committed.confirmedVersion.patch == 0U,
                "confirmed version is preserved");
}

static void test_reject_wrong_slot(void)
{
    boot_at24c02_t eeprom = {0};
    boot_firmware_metadata_t committed;
    boot_metadata_commit_result_t result;

    test_reset_memory();
    result = boot_metadata_commit_trial(&eeprom, BOOT_FIRMWARE_SLOT_A, &committed);
    test_expect(result == BOOT_METADATA_COMMIT_STATE_INVALID,
                "wrong installed slot is rejected");
    test_expect(g_writeCount == 0U, "wrong installed slot causes no EEPROM write");
    test_expect_pending_baseline();
}
//******************************** Private Functions ************************//

//******************************** Test Doubles ******************************//
boot_driver_status_t boot_at24c02_read(
    boot_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    uint16_t length)
{
    (void)eeprom;
    if ((data == NULL) || (address > TEST_EEPROM_SIZE_BYTES) ||
        (length > (TEST_EEPROM_SIZE_BYTES - address))) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    (void)memcpy(data, &g_eepromMemory[address], length);
    return BOOT_DRIVER_OK;
}

boot_driver_status_t boot_at24c02_write(
    boot_at24c02_t *eeprom,
    uint32_t address,
    const uint8_t *data,
    uint16_t length)
{
    (void)eeprom;
    if ((data == NULL) || (address > TEST_EEPROM_SIZE_BYTES) ||
        (length > (TEST_EEPROM_SIZE_BYTES - address))) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    if (g_writeCount < (uint32_t)(sizeof(g_writeAddresses) / sizeof(g_writeAddresses[0]))) {
        g_writeAddresses[g_writeCount] = (uint8_t)address;
    }
    g_writeCount++;
    if ((g_failCommitMarkerWrite != 0U) &&
        (length == 4U) && (address == (BOOT_METADATA_COPY_B_ADDRESS + TEST_MARKER_OFFSET)) &&
        (memcmp(data, "CMIT", 4U) == 0)) {
        return BOOT_DRIVER_ERR_HAL;
    }
    (void)memcpy(&g_eepromMemory[address], data, length);
    return BOOT_DRIVER_OK;
}
//******************************** Test Doubles ******************************//

//******************************** Main Function ****************************//
int main(void)
{
    test_commit_marker_failure();
    test_successful_commit();
    test_reject_wrong_slot();
    (void)printf("S09 metadata commit host test passed; atomic marker ordering verified.\n");
    return 0;
}
//******************************** Main Function ****************************//
