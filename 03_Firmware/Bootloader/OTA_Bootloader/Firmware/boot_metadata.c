/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_metadata.c
 * @brief Bootloader Metadata V1/V2 固定格式实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>
#include <string.h>

#include "boot_crc32.h"
#include "boot_metadata.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_METADATA_OFFSET_MAGIC             (0x00U)
#define BOOT_METADATA_OFFSET_FORMAT            (0x04U)
#define BOOT_METADATA_OFFSET_SIZE              (0x06U)
#define BOOT_METADATA_OFFSET_SEQUENCE          (0x08U)
#define BOOT_METADATA_OFFSET_V2_RESERVED       (0x0CU)
#define BOOT_METADATA_OFFSET_CONFIRMED_SLOT    (0x0DU)
#define BOOT_METADATA_OFFSET_SLOT_A_STATE      (0x0EU)
#define BOOT_METADATA_OFFSET_SLOT_B_STATE      (0x0FU)
#define BOOT_METADATA_OFFSET_CONFIRMED_VERSION (0x10U)
#define BOOT_METADATA_OFFSET_PENDING_SLOT      (0x18U)
#define BOOT_METADATA_OFFSET_UPGRADE_STATE     (0x19U)
#define BOOT_METADATA_OFFSET_CRC32             (0x78U)
#define BOOT_METADATA_OFFSET_COMMIT_MARKER     (0x7CU)
#define BOOT_METADATA_BODY_SIZE                (0x78U)
#define BOOT_METADATA_V2_RESERVED_OFFSET       (0x1AU)
#define BOOT_METADATA_V1_RESERVED_OFFSET       (0x18U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static uint16_t boot_metadata_read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

static uint32_t boot_metadata_read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

static void boot_metadata_write_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void boot_metadata_write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static uint8_t boot_metadata_slot_is_valid(boot_firmware_slot_t slot)
{
    return ((slot == BOOT_FIRMWARE_SLOT_A) || (slot == BOOT_FIRMWARE_SLOT_B) ||
            (slot == BOOT_FIRMWARE_SLOT_NONE)) ? 1U : 0U;
}

static uint8_t boot_metadata_state_is_valid(boot_firmware_slot_state_t state)
{
    return (state <= BOOT_FIRMWARE_SLOT_STATE_INVALID) ? 1U : 0U;
}

static uint8_t boot_metadata_upgrade_is_valid(boot_upgrade_state_t state)
{
    return (state <= BOOT_UPGRADE_STATE_ROLLBACK) ? 1U : 0U;
}

static uint8_t boot_metadata_reserved_is_zero(
    const uint8_t raw[BOOT_METADATA_COPY_SIZE],
    uint16_t formatVersion)
{
    uint32_t index;

    if ((formatVersion == BOOT_METADATA_FORMAT_VERSION_V2) &&
        (raw[BOOT_METADATA_OFFSET_V2_RESERVED] != 0U)) {
        return 0U;
    }
    index = (formatVersion == BOOT_METADATA_FORMAT_VERSION_V1) ?
            BOOT_METADATA_V1_RESERVED_OFFSET : BOOT_METADATA_V2_RESERVED_OFFSET;
    for (; index < BOOT_METADATA_OFFSET_CRC32; index++) {
        if (raw[index] != 0U) {
            return 0U;
        }
    }

    return 1U;
}

/* 与 Application 保持相同的跨字段约束，避免 Bootloader 接受不可提交状态。 */
static uint8_t boot_metadata_fields_are_valid(const boot_firmware_metadata_t *metadata)
{
    if ((metadata == NULL) ||
        (boot_metadata_slot_is_valid(metadata->confirmedSlot) == 0U) ||
        (boot_metadata_slot_is_valid(metadata->pendingSlot) == 0U) ||
        (boot_metadata_state_is_valid(metadata->slotAState) == 0U) ||
        (boot_metadata_state_is_valid(metadata->slotBState) == 0U) ||
        (boot_metadata_upgrade_is_valid(metadata->upgradeState) == 0U) ||
        (metadata->confirmedVersion.reserved != 0U)) {
        return 0U;
    }
    if (((metadata->upgradeState == BOOT_UPGRADE_STATE_NONE) &&
         (metadata->pendingSlot != BOOT_FIRMWARE_SLOT_NONE)) ||
        ((metadata->upgradeState != BOOT_UPGRADE_STATE_NONE) &&
         (metadata->pendingSlot == BOOT_FIRMWARE_SLOT_NONE))) {
        return 0U;
    }
    if (((metadata->pendingSlot == BOOT_FIRMWARE_SLOT_A) &&
         (metadata->slotAState != BOOT_FIRMWARE_SLOT_STATE_VALID)) ||
        ((metadata->pendingSlot == BOOT_FIRMWARE_SLOT_B) &&
         (metadata->slotBState != BOOT_FIRMWARE_SLOT_STATE_VALID))) {
        return 0U;
    }

    return 1U;
}

static void boot_metadata_decode_fields(
    const uint8_t raw[BOOT_METADATA_COPY_SIZE],
    uint16_t formatVersion,
    boot_firmware_metadata_t *metadata)
{
    metadata->sequence = boot_metadata_read_u32_le(&raw[BOOT_METADATA_OFFSET_SEQUENCE]);
    metadata->confirmedSlot = (boot_firmware_slot_t)raw[BOOT_METADATA_OFFSET_CONFIRMED_SLOT];
    metadata->slotAState = (boot_firmware_slot_state_t)raw[BOOT_METADATA_OFFSET_SLOT_A_STATE];
    metadata->slotBState = (boot_firmware_slot_state_t)raw[BOOT_METADATA_OFFSET_SLOT_B_STATE];
    metadata->confirmedVersion.major = boot_metadata_read_u16_le(
        &raw[BOOT_METADATA_OFFSET_CONFIRMED_VERSION]);
    metadata->confirmedVersion.minor = boot_metadata_read_u16_le(
        &raw[BOOT_METADATA_OFFSET_CONFIRMED_VERSION + 2U]);
    metadata->confirmedVersion.patch = boot_metadata_read_u16_le(
        &raw[BOOT_METADATA_OFFSET_CONFIRMED_VERSION + 4U]);
    metadata->confirmedVersion.reserved = boot_metadata_read_u16_le(
        &raw[BOOT_METADATA_OFFSET_CONFIRMED_VERSION + 6U]);
    metadata->pendingSlot = BOOT_FIRMWARE_SLOT_NONE;
    metadata->upgradeState = BOOT_UPGRADE_STATE_NONE;
    if (formatVersion == BOOT_METADATA_FORMAT_VERSION_V2) {
        metadata->pendingSlot = (boot_firmware_slot_t)raw[BOOT_METADATA_OFFSET_PENDING_SLOT];
        metadata->upgradeState = (boot_upgrade_state_t)raw[BOOT_METADATA_OFFSET_UPGRADE_STATE];
    }
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
boot_contract_status_t boot_metadata_encode_uncommitted(
    const boot_firmware_metadata_t *metadata,
    uint8_t raw[BOOT_METADATA_COPY_SIZE])
{
    uint32_t metadataCrc32;

    if ((metadata == NULL) || (raw == NULL)) {
        return BOOT_CONTRACT_ERR_NULL;
    }
    if (boot_metadata_fields_are_valid(metadata) == 0U) {
        return BOOT_CONTRACT_ERR_INVALID;
    }
    (void)memset(raw, 0, BOOT_METADATA_COPY_SIZE);
    boot_metadata_write_u32_le(&raw[BOOT_METADATA_OFFSET_MAGIC], BOOT_METADATA_MAGIC);
    boot_metadata_write_u16_le(&raw[BOOT_METADATA_OFFSET_FORMAT], BOOT_METADATA_FORMAT_VERSION_V2);
    boot_metadata_write_u16_le(&raw[BOOT_METADATA_OFFSET_SIZE], BOOT_METADATA_COPY_SIZE);
    boot_metadata_write_u32_le(&raw[BOOT_METADATA_OFFSET_SEQUENCE], metadata->sequence);
    raw[BOOT_METADATA_OFFSET_CONFIRMED_SLOT] = (uint8_t)metadata->confirmedSlot;
    raw[BOOT_METADATA_OFFSET_SLOT_A_STATE] = (uint8_t)metadata->slotAState;
    raw[BOOT_METADATA_OFFSET_SLOT_B_STATE] = (uint8_t)metadata->slotBState;
    boot_metadata_write_u16_le(&raw[BOOT_METADATA_OFFSET_CONFIRMED_VERSION], metadata->confirmedVersion.major);
    boot_metadata_write_u16_le(&raw[BOOT_METADATA_OFFSET_CONFIRMED_VERSION + 2U], metadata->confirmedVersion.minor);
    boot_metadata_write_u16_le(&raw[BOOT_METADATA_OFFSET_CONFIRMED_VERSION + 4U], metadata->confirmedVersion.patch);
    raw[BOOT_METADATA_OFFSET_PENDING_SLOT] = (uint8_t)metadata->pendingSlot;
    raw[BOOT_METADATA_OFFSET_UPGRADE_STATE] = (uint8_t)metadata->upgradeState;
    metadataCrc32 = boot_crc32_calculate(raw, BOOT_METADATA_BODY_SIZE);
    boot_metadata_write_u32_le(&raw[BOOT_METADATA_OFFSET_CRC32], metadataCrc32);
    boot_metadata_write_u32_le(&raw[BOOT_METADATA_OFFSET_COMMIT_MARKER], BOOT_METADATA_INVALID_MARKER);

    return BOOT_CONTRACT_OK;
}

boot_contract_status_t boot_metadata_decode_committed(
    const uint8_t raw[BOOT_METADATA_COPY_SIZE],
    boot_firmware_metadata_t *metadata)
{
    boot_firmware_metadata_t decodedMetadata;
    uint16_t formatVersion;
    uint32_t expectedCrc32;

    if ((raw == NULL) || (metadata == NULL)) {
        return BOOT_CONTRACT_ERR_NULL;
    }
    if (boot_metadata_read_u32_le(&raw[BOOT_METADATA_OFFSET_MAGIC]) != BOOT_METADATA_MAGIC) {
        return BOOT_CONTRACT_ERR_FORMAT;
    }
    formatVersion = boot_metadata_read_u16_le(&raw[BOOT_METADATA_OFFSET_FORMAT]);
    if ((formatVersion != BOOT_METADATA_FORMAT_VERSION_V1) &&
        (formatVersion != BOOT_METADATA_FORMAT_VERSION_V2)) {
        return BOOT_CONTRACT_ERR_FORMAT;
    }
    if (boot_metadata_read_u16_le(&raw[BOOT_METADATA_OFFSET_SIZE]) != BOOT_METADATA_COPY_SIZE) {
        return BOOT_CONTRACT_ERR_FORMAT;
    }
    if (boot_metadata_read_u32_le(&raw[BOOT_METADATA_OFFSET_COMMIT_MARKER]) !=
        BOOT_METADATA_COMMIT_MARKER) {
        return BOOT_CONTRACT_ERR_COMMIT;
    }
    if (boot_metadata_reserved_is_zero(raw, formatVersion) == 0U) {
        return BOOT_CONTRACT_ERR_INVALID;
    }
    boot_metadata_decode_fields(raw, formatVersion, &decodedMetadata);
    if (boot_metadata_fields_are_valid(&decodedMetadata) == 0U) {
        return BOOT_CONTRACT_ERR_INVALID;
    }
    expectedCrc32 = boot_crc32_calculate(raw, BOOT_METADATA_BODY_SIZE);
    if (boot_metadata_read_u32_le(&raw[BOOT_METADATA_OFFSET_CRC32]) != expectedCrc32) {
        return BOOT_CONTRACT_ERR_CRC;
    }

    *metadata = decodedMetadata;
    return BOOT_CONTRACT_OK;
}

uint8_t boot_metadata_sequence_is_newer(uint32_t candidate, uint32_t reference)
{
    uint32_t difference = candidate - reference;

    return ((difference != 0U) && (difference < 0x80000000UL)) ? 1U : 0U;
}

boot_contract_status_t boot_metadata_select_latest(
    const uint8_t copyA[BOOT_METADATA_COPY_SIZE],
    const uint8_t copyB[BOOT_METADATA_COPY_SIZE],
    boot_firmware_metadata_t *metadata,
    boot_metadata_copy_id_t *selectedCopy)
{
    boot_firmware_metadata_t metadataA;
    boot_firmware_metadata_t metadataB;
    boot_contract_status_t resultA;
    boot_contract_status_t resultB;

    if ((copyA == NULL) || (copyB == NULL) || (metadata == NULL) || (selectedCopy == NULL)) {
        return BOOT_CONTRACT_ERR_NULL;
    }
    /* 单副本损坏时仍允许从另一份有效 Copy 恢复启动决策。 */
    resultA = boot_metadata_decode_committed(copyA, &metadataA);
    resultB = boot_metadata_decode_committed(copyB, &metadataB);
    if ((resultA != BOOT_CONTRACT_OK) && (resultB != BOOT_CONTRACT_OK)) {
        return BOOT_CONTRACT_ERR_NOT_FOUND;
    }
    if ((resultA == BOOT_CONTRACT_OK) && (resultB != BOOT_CONTRACT_OK)) {
        *metadata = metadataA;
        *selectedCopy = BOOT_METADATA_COPY_A;
        return BOOT_CONTRACT_OK;
    }
    if ((resultA != BOOT_CONTRACT_OK) && (resultB == BOOT_CONTRACT_OK)) {
        *metadata = metadataB;
        *selectedCopy = BOOT_METADATA_COPY_B;
        return BOOT_CONTRACT_OK;
    }
    if (boot_metadata_sequence_is_newer(metadataB.sequence, metadataA.sequence) != 0U) {
        *metadata = metadataB;
        *selectedCopy = BOOT_METADATA_COPY_B;
    } else {
        *metadata = metadataA;
        *selectedCopy = BOOT_METADATA_COPY_A;
    }

    return BOOT_CONTRACT_OK;
}
