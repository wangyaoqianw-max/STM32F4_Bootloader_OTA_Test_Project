/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_prevalidate.c
 * @brief S09 Candidate 破坏性操作前预校验实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>
#include <string.h>

#include "boot_crc32.h"
#include "boot_prevalidate.h"
#include "memory_layout.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_PREVALIDATE_METADATA_COPY_COUNT (2U)
#define BOOT_PREVALIDATE_READ_CHUNK_SIZE     (256U)
#define BOOT_PREVALIDATE_VECTOR_SIZE         (8U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static uint32_t boot_prevalidate_read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

static uint32_t boot_prevalidate_slot_base(boot_firmware_slot_t slot)
{
    return (slot == BOOT_FIRMWARE_SLOT_A) ?
           BOOT_FIRMWARE_SLOT_A_BASE : BOOT_FIRMWARE_SLOT_B_BASE;
}

static boot_prevalidate_result_t boot_prevalidate_load_metadata(
    const boot_prevalidate_context_t *context,
    boot_firmware_metadata_t *metadata,
    boot_metadata_copy_id_t *metadataCopy)
{
    uint8_t copyA[BOOT_METADATA_COPY_SIZE];
    uint8_t copyB[BOOT_METADATA_COPY_SIZE];
    boot_driver_status_t resultA;
    boot_driver_status_t resultB;

    /* 允许单个 EEPROM Copy 读取失败，由另一份有效副本继续恢复。 */
    (void)memset(copyA, 0xFF, sizeof(copyA));
    (void)memset(copyB, 0xFF, sizeof(copyB));
    resultA = boot_at24c02_read(context->eeprom,
                                BOOT_METADATA_COPY_A_ADDRESS,
                                copyA,
                                sizeof(copyA));
    resultB = boot_at24c02_read(context->eeprom,
                                BOOT_METADATA_COPY_B_ADDRESS,
                                copyB,
                                sizeof(copyB));
    if ((resultA != BOOT_DRIVER_OK) && (resultB != BOOT_DRIVER_OK)) {
        return BOOT_PREVALIDATE_METADATA_READ_FAILED;
    }
    if (boot_metadata_select_latest(copyA, copyB, metadata, metadataCopy) !=
        BOOT_CONTRACT_OK) {
        return BOOT_PREVALIDATE_METADATA_INVALID;
    }

    return BOOT_PREVALIDATE_VALID;
}

static boot_prevalidate_result_t boot_prevalidate_payload(
    const boot_prevalidate_context_t *context,
    uint32_t payloadAddress,
    uint32_t imageSize,
    uint32_t expectedCrc32)
{
    boot_crc32_context_t crcContext;
    uint8_t buffer[BOOT_PREVALIDATE_READ_CHUNK_SIZE];
    uint32_t currentAddress = payloadAddress;
    uint32_t remaining = imageSize;
    uint16_t chunkLength;
    uint32_t actualCrc32;

    /* 只保留 256 Byte 缓冲，CRC 在外部 Flash 上流式完成。 */
    boot_crc32_init(&crcContext);
    while (remaining > 0U) {
        chunkLength = (remaining > sizeof(buffer)) ?
                      (uint16_t)sizeof(buffer) : (uint16_t)remaining;
        if (boot_w25q64_read(context->flash,
                             currentAddress,
                             buffer,
                             chunkLength) != BOOT_DRIVER_OK) {
            return BOOT_PREVALIDATE_EXTERNAL_READ_FAILED;
        }
        boot_crc32_update(&crcContext, buffer, chunkLength);
        currentAddress += chunkLength;
        remaining -= chunkLength;
    }
    actualCrc32 = boot_crc32_finalize(&crcContext);
    return (actualCrc32 == expectedCrc32) ?
           BOOT_PREVALIDATE_VALID : BOOT_PREVALIDATE_PAYLOAD_CRC_INVALID;
}

static uint8_t boot_prevalidate_version_matches(
    const boot_firmware_version_t *actual,
    const boot_firmware_version_t *expected)
{
    if (expected == NULL) {
        return 1U;
    }
    return ((actual->major == expected->major) &&
            (actual->minor == expected->minor) &&
            (actual->patch == expected->patch) &&
            (actual->reserved == expected->reserved)) ? 1U : 0U;
}

/* Pending Install 与 Confirmed Restore 共用完整的只读镜像验证。 */
static boot_prevalidate_result_t boot_prevalidate_image_slot(
    const boot_prevalidate_context_t *context,
    boot_firmware_slot_t sourceSlot,
    const boot_firmware_version_t *expectedVersion,
    boot_prevalidated_image_t *image)
{
    uint8_t rawHeader[BOOT_FIRMWARE_HEADER_SIZE];
    uint8_t vectorBytes[BOOT_PREVALIDATE_VECTOR_SIZE];
    boot_prevalidate_result_t result;
    uint32_t slotBase;

    if ((context == NULL) || (image == NULL) ||
        (context->flash == NULL) || (context->eeprom == NULL)) {
        return BOOT_PREVALIDATE_INVALID;
    }
    if ((sourceSlot != BOOT_FIRMWARE_SLOT_A) &&
        (sourceSlot != BOOT_FIRMWARE_SLOT_B)) {
        return BOOT_PREVALIDATE_INVALID;
    }

    slotBase = boot_prevalidate_slot_base(sourceSlot);
    if (boot_w25q64_read(context->flash, slotBase, rawHeader,
                         sizeof(rawHeader)) != BOOT_DRIVER_OK) {
        return BOOT_PREVALIDATE_EXTERNAL_READ_FAILED;
    }
    if (boot_image_validate_header(rawHeader, &image->header) !=
        BOOT_IMAGE_VALIDATION_VALID) {
        return BOOT_PREVALIDATE_HEADER_INVALID;
    }
    if ((image->header.imageSize < BOOT_PREVALIDATE_VECTOR_SIZE) ||
        (image->header.imageSize > APP_FLASH_SIZE)) {
        return BOOT_PREVALIDATE_SIZE_INVALID;
    }
    if (boot_prevalidate_version_matches(&image->header.version,
                                         expectedVersion) == 0U) {
        return BOOT_PREVALIDATE_CONFIRMED_VERSION_MISMATCH;
    }
    if (boot_w25q64_read(context->flash,
                         slotBase + BOOT_FIRMWARE_PAYLOAD_OFFSET,
                         vectorBytes,
                         sizeof(vectorBytes)) != BOOT_DRIVER_OK) {
        return BOOT_PREVALIDATE_EXTERNAL_READ_FAILED;
    }
    image->vector.initialMsp = boot_prevalidate_read_u32_le(vectorBytes);
    image->vector.resetHandler = boot_prevalidate_read_u32_le(&vectorBytes[4]);
    if (boot_validate_vector_values(image->vector.initialMsp,
                                    image->vector.resetHandler) !=
        BOOT_APP_VECTOR_VALID) {
        return BOOT_PREVALIDATE_VECTOR_INVALID;
    }
    result = boot_prevalidate_payload(context,
                                      slotBase + BOOT_FIRMWARE_PAYLOAD_OFFSET,
                                      image->header.imageSize,
                                      image->header.payloadCrc32);
    if (result != BOOT_PREVALIDATE_VALID) {
        return result;
    }
    image->sourceSlot = sourceSlot;
    return BOOT_PREVALIDATE_VALID;
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
boot_prevalidate_result_t boot_prevalidate_candidate(
    const boot_prevalidate_context_t *context,
    boot_prevalidated_image_t *image)
{
    boot_prevalidate_result_t result;

    if ((context == NULL) || (image == NULL) ||
        (context->flash == NULL) || (context->eeprom == NULL)) {
        return BOOT_PREVALIDATE_INVALID;
    }
    (void)memset(image, 0, sizeof(*image));
    result = boot_prevalidate_load_metadata(context,
                                            &image->metadata,
                                            &image->metadataCopy);
    if (result != BOOT_PREVALIDATE_VALID) {
        return result;
    }
    /* 到这里仍未调用 Internal Flash；以下条件全部通过才允许进入 Installer。 */
    if (image->metadata.upgradeState != BOOT_UPGRADE_STATE_PENDING) {
        return BOOT_PREVALIDATE_NO_PENDING;
    }
    if ((image->metadata.pendingSlot != BOOT_FIRMWARE_SLOT_A) &&
        (image->metadata.pendingSlot != BOOT_FIRMWARE_SLOT_B)) {
        return BOOT_PREVALIDATE_PENDING_SLOT_INVALID;
    }
    if (image->metadata.pendingSlot == image->metadata.confirmedSlot) {
        return BOOT_PREVALIDATE_PENDING_SLOT_INVALID;
    }
    if (((image->metadata.pendingSlot == BOOT_FIRMWARE_SLOT_A) &&
         (image->metadata.slotAState != BOOT_FIRMWARE_SLOT_STATE_VALID)) ||
        ((image->metadata.pendingSlot == BOOT_FIRMWARE_SLOT_B) &&
         (image->metadata.slotBState != BOOT_FIRMWARE_SLOT_STATE_VALID))) {
        return BOOT_PREVALIDATE_PENDING_SLOT_INVALID;
    }
    return boot_prevalidate_image_slot(context,
                                       image->metadata.pendingSlot,
                                       NULL,
                                       image);
}

boot_prevalidate_result_t boot_prevalidate_confirmed(
    const boot_prevalidate_context_t *context,
    boot_prevalidated_image_t *image)
{
    boot_prevalidate_result_t result;

    if ((context == NULL) || (image == NULL) ||
        (context->flash == NULL) || (context->eeprom == NULL)) {
        return BOOT_PREVALIDATE_INVALID;
    }
    (void)memset(image, 0, sizeof(*image));
    result = boot_prevalidate_load_metadata(context,
                                            &image->metadata,
                                            &image->metadataCopy);
    if (result != BOOT_PREVALIDATE_VALID) {
        return result;
    }
    if ((image->metadata.upgradeState != BOOT_UPGRADE_STATE_TRIAL) &&
        (image->metadata.upgradeState != BOOT_UPGRADE_STATE_ROLLBACK)) {
        return BOOT_PREVALIDATE_NO_RECOVERY;
    }
    if ((image->metadata.confirmedSlot != BOOT_FIRMWARE_SLOT_A) &&
        (image->metadata.confirmedSlot != BOOT_FIRMWARE_SLOT_B)) {
        return BOOT_PREVALIDATE_CONFIRMED_SLOT_INVALID;
    }
    if (image->metadata.confirmedSlot == image->metadata.pendingSlot) {
        return BOOT_PREVALIDATE_CONFIRMED_SLOT_INVALID;
    }
    if (((image->metadata.confirmedSlot == BOOT_FIRMWARE_SLOT_A) &&
         (image->metadata.slotAState != BOOT_FIRMWARE_SLOT_STATE_VALID)) ||
        ((image->metadata.confirmedSlot == BOOT_FIRMWARE_SLOT_B) &&
         (image->metadata.slotBState != BOOT_FIRMWARE_SLOT_STATE_VALID))) {
        return BOOT_PREVALIDATE_CONFIRMED_SLOT_INVALID;
    }
    return boot_prevalidate_image_slot(context,
                                       image->metadata.confirmedSlot,
                                       &image->metadata.confirmedVersion,
                                       image);
}
