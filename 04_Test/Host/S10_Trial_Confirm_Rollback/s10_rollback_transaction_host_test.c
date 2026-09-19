/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s10_rollback_transaction_host_test.c
 * @brief S10 ROLLBACK Metadata 原子事务 Host Test。
 * @author YaoQian Wang
 * @date 2026-09-19
 * @version V1.0
 * @note 本测试不进入 Keil 工程，不参与固件构建。
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boot_metadata_commit.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define TEST_EEPROM_SIZE_BYTES       (256U)
#define TEST_MARKER_OFFSET           (0x7CU)
#define TEST_BODY_SIZE               (0x7CU)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
static uint8_t g_eepromMemory[TEST_EEPROM_SIZE_BYTES];
static uint8_t g_failBodyWrite;
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
        (void)fprintf(stderr, "S10 rollback transaction host test failed: %s\n", message);
        (void)fflush(stderr);
        exit(1);
    }
}

static void test_prepare_state(boot_upgrade_state_t state)
{
    boot_firmware_metadata_t metadata = {0};
    uint8_t raw[BOOT_METADATA_COPY_SIZE];

    (void)memset(g_eepromMemory, 0xFF, sizeof(g_eepromMemory));
    (void)memset(g_writeAddresses, 0, sizeof(g_writeAddresses));
    g_failBodyWrite = 0U;
    g_failCommitMarkerWrite = 0U;
    g_writeCount = 0U;

    metadata.sequence = 10U;
    metadata.confirmedSlot = BOOT_FIRMWARE_SLOT_A;
    metadata.pendingSlot = (state == BOOT_UPGRADE_STATE_NONE) ?
                           BOOT_FIRMWARE_SLOT_NONE : BOOT_FIRMWARE_SLOT_B;
    metadata.slotAState = BOOT_FIRMWARE_SLOT_STATE_VALID;
    metadata.slotBState = (state == BOOT_UPGRADE_STATE_NONE) ?
                          BOOT_FIRMWARE_SLOT_STATE_EMPTY :
                          BOOT_FIRMWARE_SLOT_STATE_VALID;
    metadata.upgradeState = state;
    metadata.confirmedVersion.major = 1U;
    test_expect(boot_metadata_encode_uncommitted(&metadata, raw) == BOOT_CONTRACT_OK,
                "baseline metadata encodes");
    test_write_u32_le(&raw[TEST_MARKER_OFFSET], BOOT_METADATA_COMMIT_MARKER);
    (void)memcpy(&g_eepromMemory[BOOT_METADATA_COPY_A_ADDRESS], raw,
                 BOOT_METADATA_COPY_SIZE);
    (void)memcpy(&g_eepromMemory[BOOT_METADATA_COPY_B_ADDRESS], raw,
                 BOOT_METADATA_COPY_SIZE);
}

static boot_firmware_metadata_t test_load_latest(void)
{
    boot_firmware_metadata_t metadata = {0};
    boot_metadata_copy_id_t selectedCopy = BOOT_METADATA_COPY_NONE;
    boot_contract_status_t result;

    result = boot_metadata_select_latest(
        &g_eepromMemory[BOOT_METADATA_COPY_A_ADDRESS],
        &g_eepromMemory[BOOT_METADATA_COPY_B_ADDRESS],
        &metadata,
        &selectedCopy);
    test_expect(result == BOOT_CONTRACT_OK, "latest Metadata remains readable");
    return metadata;
}

static void test_expect_preserved_baseline(
    const boot_firmware_metadata_t *metadata,
    boot_upgrade_state_t state,
    boot_firmware_slot_t pendingSlot,
    boot_firmware_slot_state_t pendingSlotState,
    uint32_t sequence)
{
    test_expect(metadata->sequence == sequence, "sequence matches transaction result");
    test_expect(metadata->confirmedSlot == BOOT_FIRMWARE_SLOT_A,
                "confirmed Slot is preserved");
    test_expect(metadata->confirmedVersion.major == 1U,
                "confirmed Version is preserved");
    test_expect(metadata->slotAState == BOOT_FIRMWARE_SLOT_STATE_VALID,
                "confirmed Slot state is preserved");
    test_expect(metadata->slotBState == pendingSlotState,
                "candidate Slot state is preserved");
    test_expect(metadata->upgradeState == state, "upgrade state matches transaction result");
    test_expect(metadata->pendingSlot == pendingSlot, "pending Slot matches transaction result");
}

static void test_expect_marker_order(void)
{
    test_expect(g_writeCount >= 3U, "atomic transaction writes all phases");
    test_expect(g_writeAddresses[0] == (BOOT_METADATA_COPY_B_ADDRESS + TEST_MARKER_OFFSET),
                "invalid marker is written first");
    test_expect(g_writeAddresses[1] == BOOT_METADATA_COPY_B_ADDRESS,
                "body is written after invalid marker");
    test_expect(g_writeAddresses[2] == (BOOT_METADATA_COPY_B_ADDRESS + TEST_MARKER_OFFSET),
                "commit marker is written last");
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
    if ((g_failBodyWrite != 0U) && (length == TEST_BODY_SIZE) &&
        (address == BOOT_METADATA_COPY_B_ADDRESS)) {
        return BOOT_DRIVER_ERR_HAL;
    }
    if ((g_failCommitMarkerWrite != 0U) && (length == 4U) &&
        (address == (BOOT_METADATA_COPY_B_ADDRESS + TEST_MARKER_OFFSET)) &&
        (memcmp(data, "CMIT", 4U) == 0)) {
        return BOOT_DRIVER_ERR_HAL;
    }
    (void)memcpy(&g_eepromMemory[address], data, length);
    return BOOT_DRIVER_OK;
}
//******************************** Test Doubles ******************************//

//******************************** Functions ********************************//
static void test_rollback_begin_success(void)
{
    boot_at24c02_t eeprom = {0};
    boot_firmware_metadata_t committed;
    boot_metadata_commit_result_t result;
    boot_firmware_metadata_t latest;

    test_prepare_state(BOOT_UPGRADE_STATE_TRIAL);
    result = boot_metadata_commit_rollback_begin(&eeprom, &committed);
    test_expect(result == BOOT_METADATA_COMMIT_OK, "TRIAL enters ROLLBACK");
    test_expect_preserved_baseline(&committed, BOOT_UPGRADE_STATE_ROLLBACK,
                                   BOOT_FIRMWARE_SLOT_B,
                                   BOOT_FIRMWARE_SLOT_STATE_VALID, 11U);
    latest = test_load_latest();
    test_expect_preserved_baseline(&latest, BOOT_UPGRADE_STATE_ROLLBACK,
                                   BOOT_FIRMWARE_SLOT_B,
                                   BOOT_FIRMWARE_SLOT_STATE_VALID, 11U);
    test_expect_marker_order();
}

static void test_rollback_complete_success(void)
{
    boot_at24c02_t eeprom = {0};
    boot_firmware_metadata_t committed;
    boot_metadata_commit_result_t result;
    boot_firmware_metadata_t latest;

    test_prepare_state(BOOT_UPGRADE_STATE_ROLLBACK);
    result = boot_metadata_commit_rollback_complete(&eeprom, &committed);
    test_expect(result == BOOT_METADATA_COMMIT_OK, "ROLLBACK enters NONE");
    test_expect_preserved_baseline(&committed, BOOT_UPGRADE_STATE_NONE,
                                   BOOT_FIRMWARE_SLOT_NONE,
                                   BOOT_FIRMWARE_SLOT_STATE_VALID, 11U);
    latest = test_load_latest();
    test_expect_preserved_baseline(&latest, BOOT_UPGRADE_STATE_NONE,
                                   BOOT_FIRMWARE_SLOT_NONE,
                                   BOOT_FIRMWARE_SLOT_STATE_VALID, 11U);
    test_expect_marker_order();
}

static void test_wrong_state_is_rejected(void)
{
    boot_at24c02_t eeprom = {0};
    boot_firmware_metadata_t committed;

    test_prepare_state(BOOT_UPGRADE_STATE_NONE);
    test_expect(boot_metadata_commit_rollback_begin(&eeprom, &committed) ==
                    BOOT_METADATA_COMMIT_STATE_INVALID,
                "rollback begin rejects NONE");
    test_expect(g_writeCount == 0U, "wrong begin state causes no write");

    test_prepare_state(BOOT_UPGRADE_STATE_TRIAL);
    test_expect(boot_metadata_commit_rollback_complete(&eeprom, &committed) ==
                    BOOT_METADATA_COMMIT_STATE_INVALID,
                "rollback complete rejects TRIAL");
    test_expect(g_writeCount == 0U, "wrong complete state causes no write");
}

static void test_body_failure_keeps_old_state(void)
{
    boot_at24c02_t eeprom = {0};
    boot_firmware_metadata_t committed;
    boot_firmware_metadata_t latest;

    test_prepare_state(BOOT_UPGRADE_STATE_ROLLBACK);
    g_failBodyWrite = 1U;
    test_expect(boot_metadata_commit_rollback_complete(&eeprom, &committed) ==
                    BOOT_METADATA_COMMIT_WRITE_FAILED,
                "body failure is reported");
    latest = test_load_latest();
    test_expect_preserved_baseline(&latest, BOOT_UPGRADE_STATE_ROLLBACK,
                                   BOOT_FIRMWARE_SLOT_B,
                                   BOOT_FIRMWARE_SLOT_STATE_VALID, 10U);
}

static void test_marker_failure_keeps_old_state(void)
{
    boot_at24c02_t eeprom = {0};
    boot_firmware_metadata_t committed;
    boot_firmware_metadata_t latest;

    test_prepare_state(BOOT_UPGRADE_STATE_TRIAL);
    g_failCommitMarkerWrite = 1U;
    test_expect(boot_metadata_commit_rollback_begin(&eeprom, &committed) ==
                    BOOT_METADATA_COMMIT_WRITE_FAILED,
                "rollback marker failure is reported");
    latest = test_load_latest();
    test_expect_preserved_baseline(&latest, BOOT_UPGRADE_STATE_TRIAL,
                                   BOOT_FIRMWARE_SLOT_B,
                                   BOOT_FIRMWARE_SLOT_STATE_VALID, 10U);

    test_prepare_state(BOOT_UPGRADE_STATE_ROLLBACK);
    g_failCommitMarkerWrite = 1U;
    test_expect(boot_metadata_commit_rollback_complete(&eeprom, &committed) ==
                    BOOT_METADATA_COMMIT_WRITE_FAILED,
                "complete marker failure is reported");
    latest = test_load_latest();
    test_expect_preserved_baseline(&latest, BOOT_UPGRADE_STATE_ROLLBACK,
                                   BOOT_FIRMWARE_SLOT_B,
                                   BOOT_FIRMWARE_SLOT_STATE_VALID, 10U);
}

int main(void)
{
    test_rollback_begin_success();
    test_rollback_complete_success();
    test_wrong_state_is_rejected();
    test_body_failure_keeps_old_state();
    test_marker_failure_keeps_old_state();
    (void)printf("S10 rollback transaction host test passed; atomic recovery boundaries verified.\n");
    return 0;
}
//******************************** Functions ********************************//
