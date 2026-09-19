/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_metadata_commit.c
 * @brief S09/S10 Metadata 生命周期原子提交实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>
#include <string.h>

#include "boot_metadata_commit.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_METADATA_BODY_AND_CRC_SIZE (0x7CU)
#define BOOT_METADATA_MARKER_OFFSET     (BOOT_METADATA_BODY_AND_CRC_SIZE)
#define BOOT_METADATA_MARKER_SIZE       (4U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static void boot_metadata_commit_write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static boot_driver_status_t boot_metadata_commit_load(
    boot_at24c02_t *eeprom,
    uint8_t copyA[BOOT_METADATA_COPY_SIZE],
    uint8_t copyB[BOOT_METADATA_COPY_SIZE],
    boot_firmware_metadata_t *metadata,
    boot_metadata_copy_id_t *selectedCopy)
{
    boot_driver_status_t resultA;
    boot_driver_status_t resultB;

    /* 无效 Copy 用全 FF 填充，允许另一份有效 Copy 继续成为 latest。 */
    (void)memset(copyA, 0xFF, BOOT_METADATA_COPY_SIZE);
    (void)memset(copyB, 0xFF, BOOT_METADATA_COPY_SIZE);
    resultA = boot_at24c02_read(eeprom, BOOT_METADATA_COPY_A_ADDRESS,
                                copyA, BOOT_METADATA_COPY_SIZE);
    resultB = boot_at24c02_read(eeprom, BOOT_METADATA_COPY_B_ADDRESS,
                                copyB, BOOT_METADATA_COPY_SIZE);
    if ((resultA != BOOT_DRIVER_OK) && (resultB != BOOT_DRIVER_OK)) {
        return BOOT_DRIVER_ERR_HAL;
    }
    return (boot_metadata_select_latest(copyA, copyB, metadata, selectedCopy) ==
            BOOT_CONTRACT_OK) ? BOOT_DRIVER_OK : BOOT_DRIVER_ERR_NOT_FOUND;
}

static uint16_t boot_metadata_commit_target_address(
    boot_metadata_copy_id_t targetCopy)
{
    return (targetCopy == BOOT_METADATA_COPY_A) ?
           BOOT_METADATA_COPY_A_ADDRESS : BOOT_METADATA_COPY_B_ADDRESS;
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
static boot_metadata_commit_result_t boot_metadata_commit_transition(
    boot_at24c02_t *eeprom,
    boot_upgrade_state_t expectedState,
    uint8_t checkPendingSlot,
    boot_firmware_slot_t expectedPendingSlot,
    boot_upgrade_state_t newState,
    uint8_t updatePendingSlot,
    boot_firmware_slot_t newPendingSlot,
    boot_firmware_metadata_t *committedMetadata)
{
    uint8_t copyA[BOOT_METADATA_COPY_SIZE];
    uint8_t copyB[BOOT_METADATA_COPY_SIZE];
    uint8_t raw[BOOT_METADATA_COPY_SIZE];
    uint8_t verify[BOOT_METADATA_COPY_SIZE];
    uint8_t invalidMarker[BOOT_METADATA_MARKER_SIZE];
    uint8_t commitMarker[BOOT_METADATA_MARKER_SIZE];
    boot_firmware_metadata_t latest;
    boot_firmware_metadata_t verified;
    boot_metadata_copy_id_t selectedCopy;
    boot_metadata_copy_id_t targetCopy;
    boot_driver_status_t result;
    uint16_t targetAddress;

    if ((eeprom == NULL) || (committedMetadata == NULL)) {
        return BOOT_METADATA_COMMIT_READ_FAILED;
    }
    result = boot_metadata_commit_load(eeprom, copyA, copyB, &latest, &selectedCopy);
    if (result != BOOT_DRIVER_OK) {
        return BOOT_METADATA_COMMIT_READ_FAILED;
    }
    if ((latest.upgradeState != expectedState) ||
        ((checkPendingSlot != 0U) &&
         (latest.pendingSlot != expectedPendingSlot))) {
        return BOOT_METADATA_COMMIT_STATE_INVALID;
    }
    latest.sequence++;
    latest.upgradeState = newState;
    if (updatePendingSlot != 0U) {
        latest.pendingSlot = newPendingSlot;
    }
    targetCopy = (selectedCopy == BOOT_METADATA_COPY_A) ?
                 BOOT_METADATA_COPY_B : BOOT_METADATA_COPY_A;
    targetAddress = boot_metadata_commit_target_address(targetCopy);
    if (boot_metadata_encode_uncommitted(&latest, raw) != BOOT_CONTRACT_OK) {
        return BOOT_METADATA_COMMIT_STATE_INVALID;
    }

    /* 先使目标 Copy 无效，再写 Body+CRC，最后单独提交 marker。 */
    boot_metadata_commit_write_u32_le(invalidMarker, BOOT_METADATA_INVALID_MARKER);
    if (boot_at24c02_write(eeprom,
                           targetAddress + BOOT_METADATA_MARKER_OFFSET,
                           invalidMarker,
                           BOOT_METADATA_MARKER_SIZE) != BOOT_DRIVER_OK) {
        return BOOT_METADATA_COMMIT_WRITE_FAILED;
    }
    if (boot_at24c02_write(eeprom,
                           targetAddress,
                           raw,
                           BOOT_METADATA_BODY_AND_CRC_SIZE) != BOOT_DRIVER_OK) {
        return BOOT_METADATA_COMMIT_WRITE_FAILED;
    }
    if (boot_at24c02_read(eeprom, targetAddress, verify,
                          BOOT_METADATA_BODY_AND_CRC_SIZE) != BOOT_DRIVER_OK) {
        return BOOT_METADATA_COMMIT_VERIFY_FAILED;
    }
    if (memcmp(verify, raw, BOOT_METADATA_BODY_AND_CRC_SIZE) != 0) {
        return BOOT_METADATA_COMMIT_VERIFY_FAILED;
    }

    boot_metadata_commit_write_u32_le(commitMarker, BOOT_METADATA_COMMIT_MARKER);
    if (boot_at24c02_write(eeprom,
                           targetAddress + BOOT_METADATA_MARKER_OFFSET,
                           commitMarker,
                           BOOT_METADATA_MARKER_SIZE) != BOOT_DRIVER_OK) {
        return BOOT_METADATA_COMMIT_WRITE_FAILED;
    }
    if (boot_at24c02_read(eeprom, targetAddress + BOOT_METADATA_MARKER_OFFSET,
                          verify, BOOT_METADATA_MARKER_SIZE) != BOOT_DRIVER_OK) {
        return BOOT_METADATA_COMMIT_VERIFY_FAILED;
    }
    if (memcmp(verify, commitMarker, BOOT_METADATA_MARKER_SIZE) != 0) {
        return BOOT_METADATA_COMMIT_VERIFY_FAILED;
    }

    result = boot_metadata_commit_load(eeprom, copyA, copyB, &verified, &selectedCopy);
    if ((result != BOOT_DRIVER_OK) || (selectedCopy != targetCopy) ||
        (verified.sequence != latest.sequence) ||
        (verified.confirmedSlot != latest.confirmedSlot) ||
        (verified.pendingSlot != latest.pendingSlot) ||
        (verified.slotAState != latest.slotAState) ||
        (verified.slotBState != latest.slotBState) ||
        (verified.upgradeState != latest.upgradeState) ||
        (verified.confirmedVersion.major != latest.confirmedVersion.major) ||
        (verified.confirmedVersion.minor != latest.confirmedVersion.minor) ||
        (verified.confirmedVersion.patch != latest.confirmedVersion.patch) ||
        (verified.confirmedVersion.reserved != latest.confirmedVersion.reserved)) {
        return BOOT_METADATA_COMMIT_VERIFY_FAILED;
    }

    *committedMetadata = verified;
    return BOOT_METADATA_COMMIT_OK;
}

boot_metadata_commit_result_t boot_metadata_commit_trial(
    boot_at24c02_t *eeprom,
    boot_firmware_slot_t installedSlot,
    boot_firmware_metadata_t *committedMetadata)
{
    return boot_metadata_commit_transition(eeprom,
                                           BOOT_UPGRADE_STATE_PENDING,
                                           1U,
                                           installedSlot,
                                           BOOT_UPGRADE_STATE_TRIAL,
                                           1U,
                                           installedSlot,
                                           committedMetadata);
}

boot_metadata_commit_result_t boot_metadata_commit_rollback_begin(
    boot_at24c02_t *eeprom,
    boot_firmware_metadata_t *committedMetadata)
{
    return boot_metadata_commit_transition(eeprom,
                                           BOOT_UPGRADE_STATE_TRIAL,
                                           0U,
                                           BOOT_FIRMWARE_SLOT_NONE,
                                           BOOT_UPGRADE_STATE_ROLLBACK,
                                           0U,
                                           BOOT_FIRMWARE_SLOT_NONE,
                                           committedMetadata);
}

boot_metadata_commit_result_t boot_metadata_commit_rollback_complete(
    boot_at24c02_t *eeprom,
    boot_firmware_metadata_t *committedMetadata)
{
    return boot_metadata_commit_transition(eeprom,
                                           BOOT_UPGRADE_STATE_ROLLBACK,
                                           0U,
                                           BOOT_FIRMWARE_SLOT_NONE,
                                           BOOT_UPGRADE_STATE_NONE,
                                           1U,
                                           BOOT_FIRMWARE_SLOT_NONE,
                                           committedMetadata);
}
