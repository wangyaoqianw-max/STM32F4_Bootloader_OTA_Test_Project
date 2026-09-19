/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file firmware_lifecycle.c
 * @brief Firmware Runtime Lifecycle 实现
 * @author YaoQian Wang
 * @date 2026-09-19
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "firmware_lifecycle.h"
#include "firmware_version.h"
//******************************** Includes *********************************//

//******************************** Private Functions ************************//
static platform_bool_t firmware_lifecycle_slot_is_ab(firmware_slot_t slot)
{
    return ((slot == FIRMWARE_SLOT_A) || (slot == FIRMWARE_SLOT_B)) ?
           (platform_bool_t)1U : (platform_bool_t)0U;
}

static firmware_slot_state_t firmware_lifecycle_get_slot_state(
    const firmware_metadata_t *metadata,
    firmware_slot_t slot)
{
    return (slot == FIRMWARE_SLOT_A) ? metadata->slotAState : metadata->slotBState;
}

static platform_error_t firmware_lifecycle_check_metadata(
    const firmware_metadata_t *metadata)
{
    if (metadata == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (metadata->upgradeState != FIRMWARE_UPGRADE_STATE_TRIAL) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((firmware_lifecycle_slot_is_ab(metadata->confirmedSlot) == 0U) ||
        (firmware_lifecycle_slot_is_ab(metadata->pendingSlot) == 0U)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (metadata->confirmedSlot == metadata->pendingSlot) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if (firmware_lifecycle_get_slot_state(metadata, metadata->pendingSlot) !=
        FIRMWARE_SLOT_STATE_VALID) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t firmware_lifecycle_validate_pending_image(
    firmware_storage_t *storage,
    firmware_slot_t pendingSlot,
    firmware_image_header_t *header)
{
    firmware_image_validation_t validation;
    platform_error_t result;

    if ((storage == NULL) || (header == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    validation = FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    result = firmware_storage_validate_image(storage, pendingSlot, header, &validation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (validation == FIRMWARE_IMAGE_VALIDATION_INVALID_PAYLOAD_CRC) {
        return PLATFORM_ERR_CHECKSUM;
    }

    if (validation == FIRMWARE_IMAGE_VALIDATION_INVALID_SIZE) {
        return PLATFORM_ERR_OVERFLOW;
    }

    if (validation != FIRMWARE_IMAGE_VALIDATION_VALID) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if ((header->imageSize == 0U) ||
        (header->imageSize > FIRMWARE_SLOT_PAYLOAD_CAPACITY)) {
        return PLATFORM_ERR_OVERFLOW;
    }

    if (firmware_version_is_valid(&header->version) == 0U) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_OK;
}

static platform_bool_t firmware_lifecycle_metadata_matches(
    const firmware_metadata_t *actual,
    const firmware_metadata_t *expected)
{
    if ((actual == NULL) || (expected == NULL)) {
        return (platform_bool_t)0U;
    }

    return ((actual->confirmedSlot == expected->confirmedSlot) &&
            (actual->pendingSlot == expected->pendingSlot) &&
            (actual->slotAState == expected->slotAState) &&
            (actual->slotBState == expected->slotBState) &&
            (actual->upgradeState == expected->upgradeState) &&
            (firmware_version_compare(&actual->confirmedVersion,
                                      &expected->confirmedVersion) == 0)) ?
           (platform_bool_t)1U : (platform_bool_t)0U;
}
//******************************** Private Functions ************************//

//******************************** Functions *********************************//
platform_error_t firmware_lifecycle_confirm(firmware_storage_t *storage)
{
    firmware_metadata_t currentMetadata;
    firmware_metadata_t expectedMetadata;
    firmware_metadata_t verifiedMetadata;
    firmware_image_header_t pendingHeader;
    firmware_metadata_copy_id_t sourceCopy;
    firmware_metadata_copy_id_t committedCopy;
    firmware_metadata_copy_id_t verifiedCopy;
    platform_error_t result;

    if (storage == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = firmware_storage_load_metadata(storage, &currentMetadata, &sourceCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_lifecycle_check_metadata(&currentMetadata);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_lifecycle_validate_pending_image(
        storage,
        currentMetadata.pendingSlot,
        &pendingHeader);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    expectedMetadata = currentMetadata;
    expectedMetadata.confirmedSlot = currentMetadata.pendingSlot;
    expectedMetadata.confirmedVersion = pendingHeader.version;
    expectedMetadata.pendingSlot = FIRMWARE_SLOT_NONE;
    expectedMetadata.upgradeState = FIRMWARE_UPGRADE_STATE_NONE;

    result = firmware_storage_commit_metadata(storage, &expectedMetadata, &committedCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_load_metadata(storage, &verifiedMetadata, &verifiedCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (firmware_lifecycle_metadata_matches(&verifiedMetadata, &expectedMetadata) == 0U) {
        return PLATFORM_ERR_IO;
    }

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
